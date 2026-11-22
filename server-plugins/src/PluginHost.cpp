#include <basalt/server/plugins/PluginConfig.hpp>
#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/server/plugins/WasmPlugin.hpp>
#include <basalt/wire/plugins/PluginFolder.hpp>
#include <basalt/wire/plugins/PluginOrder.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace basalt::server::plugins {
LoadedPlugin::LoadedPlugin(wire::plugins::PluginManifest manifest, std::shared_ptr<wire::plugins::PluginPackage> pkg,
                           wire::common::Logger log, wire::common::toml::TomlTable config, std::string folder,
                           std::map<std::string, std::string, std::less<>> files)
    : manifest(std::move(manifest)), pkg(std::move(pkg)), log(std::move(log)), config(std::move(config)),
      folder(std::move(folder)), files(std::move(files)), text(this->folder, "en_us") {
    text.load();
}

LoadedPlugin::~LoadedPlugin() = default;

const PluginCommandSpec* LoadedPlugin::commandNamed(std::string_view name) const {
    for (const auto& [key, spec] : commands) {
        if (key == name) {
            return &spec;
        }
    }
    return nullptr;
}

PluginHost::PluginHost(PluginServices& services, std::int64_t memoryBytes, std::int32_t strikeLimit)
    : services_(&services), memoryBytes_(memoryBytes), strikeLimit_(strikeLimit),
      surfaces_([this](const std::string& message) { log_.warn([&message] { return message; }); }) {
    for (const PluginEventType* type : pluginEventTypes::all()) {
        events_.declare(*type);
    }
}

PluginHost::~PluginHost() { unloadAll(); }

bool PluginHost::listeningOnChannel(std::string_view channel) const { return channels_.listeningOnChannel(channel); }

void PluginHost::channelMessage(const std::string& channel, const std::string& playerUuid,
                                std::span<const std::uint8_t> bytes) {
    channels_.channelMessage(channel, playerUuid, bytes);
}

std::set<wire::plugins::PluginPermission> PluginHost::allowed(const std::string& pluginId) const {
    return allowed_ ? allowed_(pluginId) : std::set<wire::plugins::PluginPermission>{};
}

std::vector<PluginStatus> PluginHost::statuses() const {
    std::vector<PluginStatus> out;
    out.reserve(plugins_.size());
    for (const std::unique_ptr<LoadedPlugin>& plugin : plugins_) {
        std::vector<std::string> usages;
        usages.reserve(plugin->commands.size());
        for (const auto& [name, spec] : plugin->commands) {
            usages.push_back(spec.usage());
        }
        out.push_back(PluginStatus{.id = plugin->manifest.id,
                                   .name = plugin->manifest.name,
                                   .version = plugin->manifest.version,
                                   .origin = plugin->pkg->origin(),
                                   .editable = plugin->pkg->editable(),
                                   .registrations = static_cast<std::int32_t>(plugin->registrations.size()),
                                   .errorStreak = plugin->strikes,
                                   .errors = plugin->failures,
                                   .disabled = plugin->disabled,
                                   .commands = std::move(usages)});
    }
    return out;
}

bool PluginHost::isLoaded(std::string_view id) const { return pluginById(id) != nullptr; }

LoadedPlugin* PluginHost::pluginById(std::string_view id) const {
    for (const std::unique_ptr<LoadedPlugin>& plugin : plugins_) {
        if (plugin->manifest.id == id) {
            return plugin.get();
        }
    }
    return nullptr;
}

PluginLoadReport PluginHost::loadAll() {
    std::vector<std::pair<std::shared_ptr<wire::plugins::PluginPackage>, wire::plugins::PluginManifest>> candidates;
    PluginLoadReport report;
    std::expected<std::vector<std::unique_ptr<wire::plugins::PluginPackage>>, wire::plugins::PluginLoadError>
        discovered = wire::plugins::discoverPackages(services_->paths().packages(), ".bspkg");
    if (!discovered.has_value()) {
        return PluginLoadReport{.loaded = {}, .refused = {{"plugins/", discovered.error().message}}};
    }
    for (std::unique_ptr<wire::plugins::PluginPackage>& owned : *discovered) {
        const std::shared_ptr<wire::plugins::PluginPackage> pkg{std::move(owned)};
        const std::optional<std::string> text = pkg->readText(wire::plugins::packageLayout::kManifest);
        if (!text.has_value()) {
            report.refused.emplace_back(pkg->origin(),
                                        "it has no " + std::string{wire::plugins::packageLayout::kManifest});
            continue;
        }
        const std::expected<wire::plugins::PluginManifest, wire::plugins::ManifestError> manifest =
            wire::plugins::PluginManifest::parse(*text);
        if (!manifest.has_value()) {
            report.refused.emplace_back(pkg->origin(), manifest.error().message);
            continue;
        }
        const auto clash = std::ranges::find_if(
            candidates, [&manifest](const auto& candidate) { return candidate.second.id == manifest->id; });
        if (clash != candidates.end()) {
            report.refused.emplace_back(pkg->origin(), "'" + manifest->id + "' is also at " + clash->first->origin());
            continue;
        }
        if (isLoaded(manifest->id)) {
            continue;
        }
        candidates.emplace_back(pkg, *manifest);
    }

    std::vector<wire::plugins::PluginManifest> manifests;
    manifests.reserve(candidates.size());
    for (const auto& [pkg, manifest] : candidates) {
        manifests.push_back(manifest);
    }
    for (const wire::plugins::PluginOrdering& decision : wire::plugins::pluginOrder::resolve(manifests)) {
        if (!decision.loads()) {
            report.refused.emplace_back(decision.manifest.id, decision.refusal);
            continue;
        }
        const auto found = std::ranges::find_if(
            candidates, [&decision](const auto& candidate) { return candidate.second.id == decision.manifest.id; });
        if (found == candidates.end()) {
            continue;
        }
        if (start(decision.manifest, found->first)) {
            report.loaded.push_back(decision.manifest.id);
        } else {
            report.refused.emplace_back(decision.manifest.id, "it failed while starting");
        }
    }
    for (const auto& [what, why] : report.refused) {
        log_.warn([&what, &why] { return what + " did not load: " + why; });
    }
    return report;
}

std::vector<std::string> PluginHost::unload(std::string_view id) {
    if (pluginById(id) == nullptr) {
        return {};
    }
    std::vector<wire::plugins::PluginManifest> manifests;
    manifests.reserve(plugins_.size());
    for (const std::unique_ptr<LoadedPlugin>& plugin : plugins_) {
        manifests.push_back(plugin->manifest);
    }
    std::vector<std::string> order = wire::plugins::pluginOrder::dependents(id, manifests);
    if (!order.empty()) {
        std::string names;
        for (const std::string& each : order) {
            names += names.empty() ? each : ", " + each;
        }
        log_.info([&id, &names] {
            return "unloading " + std::string{id} + " also unloads " + names + ", which depend on it";
        });
    }
    order.emplace_back(id);

    std::vector<std::string> gone;
    for (const std::string& each : order) {
        LoadedPlugin* target = pluginById(each);
        if (target == nullptr) {
            continue;
        }
        stop(*target);
        std::erase_if(plugins_, [target](const std::unique_ptr<LoadedPlugin>& held) { return held.get() == target; });
        gone.push_back(each);
    }
    return gone;
}

PluginLoadReport PluginHost::reload(std::string_view id) {
    const std::vector<std::string> gone = unload(id);
    if (gone.empty()) {
        return PluginLoadReport{.loaded = {}, .refused = {{std::string{id}, "it is not loaded"}}};
    }
    return loadAll();
}

void PluginHost::unloadAll() {
    while (!plugins_.empty()) {
        static_cast<void>(unload(plugins_.back()->manifest.id));
    }
}

std::map<std::string, std::int64_t> PluginHost::orphanedData() const {
    std::map<std::string, std::int64_t> orphans;
    for (const auto& [id, rows] : services_->storage().namespaces()) {
        if (!isLoaded(id)) {
            orphans.emplace(id, rows);
        }
    }
    return orphans;
}

std::optional<std::int64_t> PluginHost::purge(const std::string& pluginId) {
    if (isLoaded(pluginId)) {
        return std::nullopt;
    }
    const std::int64_t rows = services_->storage().purge(pluginId);
    log_.warn([&pluginId, rows] { return "purged " + std::to_string(rows) + " row(s) of '" + pluginId + "'"; });
    return rows;
}

std::optional<std::string> PluginHost::interruptIfStuck(std::int64_t limitMillis) {
    return watchdog_.interruptIfStuck(limitMillis, plugins_);
}

void PluginHost::interruptAll() { watchdog_.interruptAll(plugins_); }

void PluginHost::pauseWatchdog() { watchdog_.clear(); }

void PluginHost::resumeWatchdog(std::string_view pluginId) { watchdog_.mark(pluginId); }

bool PluginHost::start(const wire::plugins::PluginManifest& manifest,
                       const std::shared_ptr<wire::plugins::PluginPackage>& pkg) {
    wire::common::Logger log{"plugin/" + manifest.id};
    bool verified = false;
    if (wire::plugins::signature::carriesSignature(*pkg)) {
        const auto signer = wire::plugins::signature::verify(*pkg, trustedKeys_);
        if (!signer.has_value()) {
            log.error([&manifest, &signer] { return manifest.id + " was refused: " + signer.error(); });
            return false;
        }
        verified = true;
    }
    const std::string folderPath = pkg->workingFolder().value_or(services_->paths().folder(manifest.id));
    const wire::plugins::PluginFolder folder{*pkg, manifest, folderPath};
    folder.materialize([&log](std::string_view message) { log.info([&message] { return std::string{message}; }); });
    const std::expected<std::map<std::string, std::string, std::less<>>, wire::plugins::PluginLoadError> files =
        folder.index();
    if (!files.has_value()) {
        log.error([&manifest, &files] { return manifest.id + " did not load: " + files.error().message; });
        return false;
    }
    const std::expected<wire::common::toml::TomlTable, wire::plugins::PluginLoadError> config =
        pluginConfig::load(*files);
    if (!config.has_value()) {
        log.error([&manifest, &config] { return manifest.id + " did not load: " + config.error().message; });
        return false;
    }
    auto plugin = std::make_unique<LoadedPlugin>(manifest, pkg, std::move(log), *config, folderPath, *files);
    plugin->bytecodeVerified = verified;

    const std::set<wire::plugins::PluginPermission> granted = allowed(manifest.id);
    std::vector<std::string> missing;
    for (const wire::plugins::PluginPermission asked : manifest.capabilities.asks()) {
        if (!granted.contains(asked)) {
            missing.emplace_back(wire::plugins::permissionName(asked));
        }
    }
    if (!missing.empty()) {
        std::string names;
        std::string quoted;
        for (const std::string& each : missing) {
            names += names.empty() ? each : ", " + each;
            quoted += quoted.empty() ? "\"" + each + "\"" : ", \"" + each + "\"";
        }
        plugin->log.warn([&manifest, &names, &quoted] {
            return manifest.id + " asks for " + names + " and the operator has not granted it. Add " + manifest.id +
                   " = [" + quoted + "] under [plugins.allow] in config/server.toml to allow it.";
        });
    }

    return startWasm(std::move(plugin), verified);
}

void PluginHost::stop(LoadedPlugin& plugin) {
    if (plugin.wasm != nullptr) {
        plugin.wasm->stop();
    }
    plugin.registrations.retireAll([&plugin](std::string_view what, std::string_view error) {
        plugin.log.warn(
            [&what, &error] { return "could not retire " + std::string{what} + ": " + std::string{error}; });
    });
    events_.removeAll(plugin.manifest.id);
    if (plugin.wasm != nullptr) {
        plugin.wasm->close();
        plugin.wasm.reset();
    }
    plugin.log.info([&plugin] { return plugin.manifest.name + " unloaded"; });
}
} // namespace basalt::server::plugins
