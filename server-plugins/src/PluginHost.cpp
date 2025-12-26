#include <basalt/server/lua/LuaCall.hpp>
#include <basalt/server/plugins/PluginConfig.hpp>
#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/plugins/PluginFolder.hpp>
#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/server/plugins/PluginOrder.hpp>

#include <algorithm>
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

LoadedPlugin::LoadedPlugin(PluginManifest manifest, std::shared_ptr<PluginPackage> pkg, wire::common::Logger log,
                           wire::common::toml::TomlTable config, std::string folder,
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

PluginHost::PluginHost(PluginServices& services, std::int32_t lanes, lua::LuaLimits limits, std::int32_t strikeLimit)
    : services_(&services), lanes_(lanes), limits_(limits), strikeLimit_(strikeLimit),
      surfaces_([this](const std::string& message) { log_.warn([&message] { return message; }); }) {
    for (const PluginEventType* type : pluginEventTypes::all()) {
        events_.declare(*type);
    }
}

PluginHost::~PluginHost() { unloadAll(); }

std::set<PluginPermission> PluginHost::allowed(const std::string& pluginId) const {
    return allowed_ ? allowed_(pluginId) : std::set<PluginPermission>{};
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
    std::vector<std::pair<std::shared_ptr<PluginPackage>, PluginManifest>> candidates;
    PluginLoadReport report;
    std::expected<std::vector<std::unique_ptr<PluginPackage>>, PluginLoadError> discovered =
        discoverPackages(services_->paths().packages());
    if (!discovered.has_value()) {
        return PluginLoadReport{.loaded = {}, .refused = {{"plugins/", discovered.error().message}}};
    }
    for (std::unique_ptr<PluginPackage>& owned : *discovered) {
        const std::shared_ptr<PluginPackage> pkg{std::move(owned)};
        const std::optional<std::string> text = pkg->readText(packageLayout::kManifest);
        if (!text.has_value()) {
            report.refused.emplace_back(pkg->origin(), "it has no " + std::string{packageLayout::kManifest});
            continue;
        }
        const std::expected<PluginManifest, ManifestError> manifest = PluginManifest::parse(*text);
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

    std::vector<PluginManifest> manifests;
    manifests.reserve(candidates.size());
    for (const auto& [pkg, manifest] : candidates) {
        manifests.push_back(manifest);
    }
    for (const PluginOrdering& decision : pluginOrder::resolve(manifests)) {
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
    std::vector<PluginManifest> manifests;
    manifests.reserve(plugins_.size());
    for (const std::unique_ptr<LoadedPlugin>& plugin : plugins_) {
        manifests.push_back(plugin->manifest);
    }
    std::vector<std::string> order = pluginOrder::dependents(id, manifests);
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

std::unique_ptr<lua::LuaSandbox> PluginHost::sandboxFor(LoadedPlugin& plugin, std::int32_t lane) {
    return lua::LuaSandbox::create(
        plugin.manifest.id, limits_, seedFor(plugin.manifest.id, lane),
        [this, &plugin](std::string_view name) { return moduleSource(plugin, name); },
        [&plugin](std::string_view line) { plugin.log.info([&line] { return std::string{line}; }); });
}

bool PluginHost::start(const PluginManifest& manifest, const std::shared_ptr<PluginPackage>& pkg) {
    wire::common::Logger log{"plugin/" + manifest.id};
    const std::optional<std::string> source = pkg->readText(manifest.main);
    // A package that only exports modules is a library, and a library has nothing to run.
    const bool library = !source.has_value() && !manifest.exports.modules.empty();
    if (!source.has_value() && !library) {
        log.error([&manifest] {
            return manifest.id + " says its code is at " + manifest.main + " and there is nothing there";
        });
        return false;
    }
    const std::string folderPath = pkg->workingFolder().value_or(services_->paths().folder(manifest.id));
    const PluginFolder folder{*pkg, manifest, folderPath};
    folder.materialize([&log](std::string_view message) { log.info([&message] { return std::string{message}; }); });
    const std::expected<std::map<std::string, std::string, std::less<>>, PluginLoadError> files = folder.index();
    if (!files.has_value()) {
        log.error([&manifest, &files] { return manifest.id + " did not load: " + files.error().message; });
        return false;
    }
    const std::expected<wire::common::toml::TomlTable, PluginLoadError> config = pluginConfig::load(*files);
    if (!config.has_value()) {
        log.error([&manifest, &config] { return manifest.id + " did not load: " + config.error().message; });
        return false;
    }
    auto plugin = std::make_unique<LoadedPlugin>(manifest, pkg, std::move(log), *config, folderPath, *files);

    // Said once, at load, and not at the first call: a plugin that asked for something it did not
    // get should be visible to the operator before it misbehaves, and the plugin itself simply does
    // not get the global.
    const std::set<PluginPermission> granted = allowed(manifest.id);
    std::vector<std::string> missing;
    for (const PluginPermission asked : manifest.capabilities.asks()) {
        if (!granted.contains(asked)) {
            missing.emplace_back(permissionName(asked));
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

    // Compiled once and bound into each lane: a prototype is finished code and shared safely, and it
    // is the compile that costs, not the libraries.
    const std::string chunkName = "@" + manifest.id + "/" + manifest.main;
    std::unique_ptr<lua::LuaSandbox> first = sandboxFor(*plugin, 0);
    if (first == nullptr) {
        plugin->log.error([&manifest] { return manifest.id + " could not be given a Lua state"; });
        return false;
    }
    std::optional<lua::LuaChunk> prototype;
    if (source.has_value()) {
        std::expected<lua::LuaChunk, lua::LuaFailure> compiled = first->compile(*source, chunkName);
        if (!compiled.has_value()) {
            plugin->log.error(
                [&manifest, &compiled] { return manifest.id + " does not compile: " + compiled.error().message; });
            return false;
        }
        prototype = std::move(*compiled);
    }

    LoadedPlugin& loaded = *plugin;
    plugins_.push_back(std::move(plugin));
    for (std::int32_t lane = 0; lane < lanes_; lane++) {
        std::unique_ptr<lua::LuaSandbox> sandbox = lane == 0 ? std::move(first) : sandboxFor(loaded, lane);
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
            const std::expected<void, lua::LuaFailure> bound = added.sandbox->bind(*prototype);
            if (!bound.has_value()) {
                report(loaded, bound.error(), "load");
                continue;
            }
            const std::expected<void, lua::LuaFailure> ran = added.sandbox->run(*prototype);
            if (!ran.has_value()) {
                report(loaded, ran.error(), "load");
            }
        }
    }
    // A plugin whose own file failed while running has not loaded, whatever it managed to register
    // before it fell over: half a plugin is worse than none, and the log already says what went
    // wrong.
    if (loaded.failures > 0 || loaded.disabled) {
        loaded.log.error([&manifest] { return manifest.id + " failed while loading and is off"; });
        LoadedPlugin* target = &loaded;
        stop(loaded);
        std::erase_if(plugins_, [target](const std::unique_ptr<LoadedPlugin>& held) { return held.get() == target; });
        return false;
    }
    for (const std::unique_ptr<PluginInstance>& instance : loaded.instances) {
        for (const lua::LuaRef& handler : instance->onEnable) {
            enter(loaded, *instance, handler, "plugin.enable");
        }
    }
    loaded.log.info(
        [&manifest, &pkg] { return manifest.name + " " + manifest.version + " loaded from " + pkg->origin(); });
    return true;
}

void PluginHost::stop(LoadedPlugin& plugin) {
    for (const std::unique_ptr<PluginInstance>& instance : plugin.instances) {
        for (const lua::LuaRef& handler : instance->onDisable) {
            enter(plugin, *instance, handler, "plugin.disable");
        }
    }
    plugin.registrations.retireAll([&plugin](std::string_view what, std::string_view error) {
        plugin.log.warn(
            [&what, &error] { return "could not retire " + std::string{what} + ": " + std::string{error}; });
    });
    // The ledger above already took them out one by one; this is the belt to its braces, so a
    // handler cannot outlive its plugin even if a registration was written wrong.
    events_.removeAll(plugin.manifest.id);
    plugin.instances.clear();
    plugin.commands.clear();
    plugin.log.info([&plugin] { return plugin.manifest.name + " unloaded"; });
}

std::optional<std::vector<std::uint8_t>> PluginHost::moduleSource(LoadedPlugin& plugin, std::string_view requested) {
    const std::string path{requested};
    if (std::optional<std::vector<std::uint8_t>> own = plugin.pkg->read(std::string{packageLayout::kSource} + path);
        own.has_value()) {
        return own;
    }
    const std::size_t slash = path.find('/');
    if (slash == std::string::npos) {
        return std::nullopt;
    }
    const std::string id = path.substr(0, slash);
    const std::string rest = path.substr(slash + 1);
    // Its own code under its own id: `require("me.util")` from inside `me` is the same file as
    // `require("util")`, and a library that names its own modules the way everyone else does should
    // not have to write them differently.
    if (id == plugin.manifest.id) {
        return plugin.pkg->read(std::string{packageLayout::kSource} + rest);
    }
    const LoadedPlugin* owner = pluginById(id);
    if (owner == nullptr) {
        return std::nullopt;
    }
    std::string module = rest;
    if (module.ends_with(".lua")) {
        module.resize(module.size() - 4);
    }
    if (!owner->manifest.exports.exportsModule(module)) {
        // Not "no such module": it is there and it is private, and saying which is the difference
        // between a one-line fix and an afternoon.
        return std::nullopt;
    }
    return owner->pkg->read(std::string{packageLayout::kSource} + rest);
}

} // namespace basalt::server::plugins
