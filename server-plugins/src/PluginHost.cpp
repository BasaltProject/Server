#include <basalt/server/plugins/PluginConfig.hpp>
#include <basalt/server/plugins/PluginDispatch.hpp>
#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/wire/lua/LuaCall.hpp>
#include <basalt/wire/plugins/PluginFolder.hpp>
#include <basalt/wire/plugins/PluginOrder.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace basalt::server::plugins {
namespace {
/// A plugin's two lanes must not walk the same random sequence.
[[nodiscard]] std::uint64_t seedFor(std::string_view pluginId, std::int32_t lane) {
    std::uint64_t hash = 0;
    for (const char letter : pluginId) {
        hash = (hash * 31) + static_cast<std::uint64_t>(static_cast<unsigned char>(letter));
    }
    return (hash * 31) + static_cast<std::uint64_t>(lane);
}
} // namespace

LoadedPlugin::LoadedPlugin(wire::plugins::PluginManifest manifest, std::shared_ptr<wire::plugins::PluginPackage> pkg,
                           wire::common::Logger log, wire::common::toml::TomlTable config, std::string folder,
                           std::map<std::string, std::string, std::less<>> files)
    : manifest(std::move(manifest)), pkg(std::move(pkg)), log(std::move(log)), config(std::move(config)),
      folder(std::move(folder)), files(std::move(files)), text(this->folder, "en_us") {
    text.load();
}

CommandEntry* LoadedPlugin::commandNamed(std::string_view name) {
    for (auto& [key, entry] : commands) {
        if (key == name) {
            return &entry;
        }
    }
    return nullptr;
}

PluginHost::PluginHost(PluginServices& services, std::int32_t lanes, wire::lua::LuaLimits limits,
                       std::int32_t strikeLimit)
    : services_(&services), lanes_(lanes), limits_(limits), strikeLimit_(strikeLimit),
      surfaces_([this](const std::string& message) { log_.warn([&message] { return message; }); }),
      packets_([this](LoadedPlugin& plugin, const wire::lua::LuaRef& fn,
                      PluginPacket& packet) { deliverPacket(plugin, fn, packet); },
               [this](const PacketAddress& address) { return services_->packets().nameOf(address); }),
      commandDispatch_(std::make_unique<PluginCommandDispatch>(*this)),
      eventDispatch_(std::make_unique<PluginEventDispatch>(*this)) {
    for (const PluginEventType* type : pluginEventTypes::all()) {
        events_.declare(*type);
    }
}

PluginHost::~PluginHost() { unloadAll(); }

void PluginHost::registerCommand(LoadedPlugin& plugin, PluginInstance& instance, const PluginCommandSpec& spec,
                                 wire::lua::LuaRef handler) {
    commandDispatch_->registerCommand(plugin, instance, spec, std::move(handler));
}

void PluginHost::registerEventHandler(LoadedPlugin& plugin, PluginInstance& instance, const PluginEventType& type,
                                      PluginEventPriority priority, bool observer, bool wantsCancelled,
                                      std::map<std::string, PluginEventValue, std::less<>> filters,
                                      wire::lua::LuaRef handler) {
    eventDispatch_->registerEventHandler(plugin, instance, type, priority, observer, wantsCancelled, std::move(filters),
                                         std::move(handler));
}

void PluginHost::announce(LoadedPlugin& plugin, const std::string& name, wire::lua::LuaRef handler) {
    announcements_.announce(*this, plugin, name, std::move(handler));
}

std::int32_t PluginHost::emit(const std::string& name, const wire::lua::LuaRef& payload) {
    return announcements_.emit(*this, name, payload);
}

void PluginHost::listenOn(LoadedPlugin& plugin, const std::string& channel, wire::lua::LuaRef fn) {
    channels_.listenOn(plugin, channel, std::move(fn));
}

bool PluginHost::listeningOnChannel(std::string_view channel) const { return channels_.listeningOnChannel(channel); }

void PluginHost::channelMessage(const std::string& channel, const std::string& playerUuid,
                                std::span<const std::uint8_t> bytes) {
    channels_.channelMessage(*this, channel, playerUuid, bytes);
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
        for (const auto& [name, entry] : plugin->commands) {
            usages.push_back(entry.spec.usage());
        }
        out.push_back(PluginStatus{.id = plugin->manifest.id,
                                   .name = plugin->manifest.name,
                                   .version = plugin->manifest.version,
                                   .origin = plugin->pkg->origin(),
                                   .editable = plugin->pkg->editable(),
                                   .lanes = static_cast<std::int32_t>(plugin->instances.size()),
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

std::unique_ptr<wire::lua::LuaSandbox> PluginHost::sandboxFor(LoadedPlugin& plugin, std::int32_t lane) {
    auto sandbox = wire::lua::LuaSandbox::create(
        plugin.manifest.id, limits_, seedFor(plugin.manifest.id, lane),
        [this, &plugin](std::string_view name) { return moduleSource(plugin, name); },
        [&plugin](std::string_view line) { plugin.log.info([&line] { return std::string{line}; }); });
    if (sandbox != nullptr) {
        sandbox->allowBytecode(plugin.bytecodeVerified);
    }
    return sandbox;
}

bool PluginHost::start(const wire::plugins::PluginManifest& manifest,
                       const std::shared_ptr<wire::plugins::PluginPackage>& pkg) {
    wire::common::Logger log{"plugin/" + manifest.id};
    bool verified = false;
    if (wire::plugins::signature::hasBytecode(*pkg)) {
        const auto signer = wire::plugins::signature::verify(*pkg, trustedKeys_);
        if (!signer.has_value()) {
            log.error([&manifest, &signer] { return manifest.id + " was refused: " + signer.error(); });
            return false;
        }
        verified = true;
    }
    std::optional<std::string> source = pkg->readText(manifest.main);
    std::optional<std::vector<std::uint8_t>> bytecode;
    if (verified && manifest.main.ends_with(".lua")) {
        bytecode = pkg->read(manifest.main + "c");
        if (bytecode.has_value()) {
            source.reset();
        }
    }
    const bool library = !source.has_value() && !bytecode.has_value() && !manifest.exports.modules.empty();
    if (!source.has_value() && !bytecode.has_value() && !library) {
        log.error([&manifest] {
            return manifest.id + " says its code is at " + manifest.main + " and there is nothing there";
        });
        return false;
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

    const std::string chunkName = "@" + manifest.id + "/" + manifest.main;
    std::unique_ptr<wire::lua::LuaSandbox> first = sandboxFor(*plugin, 0);
    if (first == nullptr) {
        plugin->log.error([&manifest] { return manifest.id + " could not be given a Lua state"; });
        return false;
    }
    std::optional<wire::lua::LuaChunk> prototype;
    if (source.has_value()) {
        std::expected<wire::lua::LuaChunk, wire::lua::LuaFailure> compiled = first->compile(*source, chunkName);
        if (!compiled.has_value()) {
            plugin->log.error(
                [&manifest, &compiled] { return manifest.id + " does not compile: " + compiled.error().message; });
            return false;
        }
        prototype = std::move(*compiled);
    } else if (bytecode.has_value()) {
        prototype = wire::lua::LuaChunk{.chunkName = chunkName, .bytecode = std::move(*bytecode)};
    }

    LoadedPlugin& loaded = *plugin;
    plugins_.push_back(std::move(plugin));
    for (std::int32_t lane = 0; lane < lanes_; lane++) {
        std::unique_ptr<wire::lua::LuaSandbox> sandbox = lane == 0 ? std::move(first) : sandboxFor(loaded, lane);
        if (sandbox == nullptr) {
            continue;
        }
        auto instance = std::make_unique<PluginInstance>();
        instance->lane = lane;
        instance->sandbox = std::move(sandbox);
        PluginInstance& added = *instance;
        loaded.instances.push_back(std::move(instance));
        if (installApi_) {
            installApi_(added, loaded, *this, lanes_);
        }
        if (prototype.has_value()) {
            const std::expected<void, wire::lua::LuaFailure> bound = added.sandbox->bind(*prototype);
            if (!bound.has_value()) {
                report(loaded, bound.error(), "load");
                continue;
            }
            const std::expected<void, wire::lua::LuaFailure> ran = added.sandbox->run(*prototype);
            if (!ran.has_value()) {
                report(loaded, ran.error(), "load");
            }
        }
    }
    if (loaded.failures > 0 || loaded.disabled) {
        loaded.log.error([&manifest] { return manifest.id + " failed while loading and is off"; });
        LoadedPlugin* target = &loaded;
        stop(loaded);
        std::erase_if(plugins_, [target](const std::unique_ptr<LoadedPlugin>& held) { return held.get() == target; });
        return false;
    }
    for (const std::unique_ptr<PluginInstance>& instance : loaded.instances) {
        for (const wire::lua::LuaRef& handler : instance->onEnable) {
            enter(loaded, *instance, handler, "plugin.enable");
        }
    }
    loaded.log.info(
        [&manifest, &pkg] { return manifest.name + " " + manifest.version + " loaded from " + pkg->origin(); });
    return true;
}

void PluginHost::stop(LoadedPlugin& plugin) {
    for (const std::unique_ptr<PluginInstance>& instance : plugin.instances) {
        for (const wire::lua::LuaRef& handler : instance->onDisable) {
            enter(plugin, *instance, handler, "plugin.disable");
        }
    }
    plugin.registrations.retireAll([&plugin](std::string_view what, std::string_view error) {
        plugin.log.warn(
            [&what, &error] { return "could not retire " + std::string{what} + ": " + std::string{error}; });
    });
    events_.removeAll(plugin.manifest.id);
    plugin.instances.clear();
    plugin.commands.clear();
    plugin.log.info([&plugin] { return plugin.manifest.name + " unloaded"; });
}

std::optional<std::vector<std::uint8_t>> PluginHost::moduleSource(LoadedPlugin& plugin, std::string_view requested) {
    const std::optional<std::string> own = wire::plugins::packageLayout::moduleFile(requested);
    if (!own.has_value()) {
        return std::nullopt;
    }
    if (plugin.bytecodeVerified) {
        if (std::optional<std::vector<std::uint8_t>> compiled = plugin.pkg->read(*own + "c"); compiled.has_value()) {
            return compiled;
        }
    }
    if (std::optional<std::vector<std::uint8_t>> found = plugin.pkg->read(*own); found.has_value()) {
        return found;
    }
    const std::size_t dot = requested.find('.');
    if (dot == std::string_view::npos) {
        return std::nullopt;
    }
    const std::string id{requested.substr(0, dot)};
    const std::string_view module = requested.substr(dot + 1);
    const std::optional<std::string> file = wire::plugins::packageLayout::moduleFile(module);
    if (!file.has_value()) {
        return std::nullopt;
    }
    if (id == plugin.manifest.id) {
        return plugin.pkg->read(*file);
    }
    const LoadedPlugin* owner = pluginById(id);
    if (owner == nullptr) {
        return std::nullopt;
    }
    if (!owner->manifest.exports.exportsModule(module)) {
        return std::nullopt;
    }
    return owner->pkg->read(*file);
}
} // namespace basalt::server::plugins
