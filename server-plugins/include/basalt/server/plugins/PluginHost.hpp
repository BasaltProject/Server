#pragma once

#include <basalt/server/lua/LuaRef.hpp>
#include <basalt/server/lua/LuaSandbox.hpp>
#include <basalt/server/plugins/PluginCommandSpec.hpp>
#include <basalt/server/plugins/PluginEvents.hpp>
#include <basalt/server/plugins/PluginManifest.hpp>
#include <basalt/server/plugins/PluginPackage.hpp>
#include <basalt/server/plugins/PluginPackets.hpp>
#include <basalt/server/plugins/PluginRegistrations.hpp>
#include <basalt/server/plugins/PluginServices.hpp>
#include <basalt/server/plugins/PluginSurfaces.hpp>
#include <basalt/server/plugins/PluginText.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/common/toml/Toml.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::plugins {

class PluginHost;

/// One Lua state of one plugin: its lane.
struct PluginInstance {
    std::int32_t lane = 0;
    std::unique_ptr<lua::LuaSandbox> sandbox;
    std::vector<lua::LuaRef> onEnable;
    std::vector<lua::LuaRef> onDisable;

    /// How deep this lane is inside a decider right now.
    ///
    /// A decider answers a question the server is holding open, so it cannot pause: `wait` looks at
    /// this and refuses rather than ending the turn and leaving the server with no answer.
    std::int32_t deciderDepth = 0;
};

/// A command of a plugin, with the function each lane answers it with.
struct CommandEntry {
    PluginCommandSpec spec;
    std::map<std::int32_t, lua::LuaRef> handlers;
    std::int32_t nextLane = 0;
};

/// One plugin, loaded: its package, its states, and everything it has registered.
class LoadedPlugin {
public:
    LoadedPlugin(PluginManifest manifest, std::shared_ptr<PluginPackage> pkg, wire::common::Logger log,
                 wire::common::toml::TomlTable config, std::string folder,
                 std::map<std::string, std::string, std::less<>> files);

    PluginManifest manifest;
    std::shared_ptr<PluginPackage> pkg;
    wire::common::Logger log;
    wire::common::toml::TomlTable config;
    /// `plugins/<id>/`: what the plugin reads and writes, in whatever shape it decided on.
    std::string folder;
    /// What was in that folder when it loaded, by path relative to it.
    std::map<std::string, std::string, std::less<>> files;

    PluginRegistrations registrations;
    /// This plugin's own `lang/`, and the substitution that goes with it.
    PluginText text;
    std::vector<std::unique_ptr<PluginInstance>> instances;
    std::vector<std::pair<std::string, CommandEntry>> commands;

    /// Consecutive failures. Reset by a turn that works, so an old mistake does not add up forever.
    std::int32_t strikes = 0;

    /// Every failure since it loaded, which is what an operator wants to see even after it recovers.
    std::int32_t failures = 0;

    /// Stopped after too many failures in a row: still loaded, still listed, no longer run.
    bool disabled = false;

    [[nodiscard]] CommandEntry* commandNamed(std::string_view name);
};

/// What is loaded, for `/plugins` and for anything that has to report.
struct PluginStatus {
    std::string id;
    std::string name;
    std::string version;
    std::string origin;
    bool editable = false;
    std::int32_t lanes = 0;
    std::int32_t registrations = 0;
    /// Failures in a row right now: this is what turns a plugin off when it reaches the limit.
    std::int32_t errorStreak = 0;
    /// Failures since it loaded, which do not go away when it recovers.
    std::int32_t errors = 0;
    bool disabled = false;
    std::vector<std::string> commands;
};

/// The outcome of a load pass: what came up, and what did not and why.
struct PluginLoadReport {
    std::vector<std::string> loaded;
    std::vector<std::pair<std::string, std::string>> refused;
};

/// Whoever is inside a plugin right now, and since when.
///
/// Written by the thread that runs plugins and read by the watchdog on another, which is why it is
/// two atomics and not a struct: nobody has to see them agree.
class PluginWatchdog {
public:
    [[nodiscard]] std::int64_t nowMillis() const;

    /// Marks `pluginId` as the one running, now.
    void mark(std::string_view pluginId);

    void clear();

    /// Cuts off a plugin that has been inside the server for longer than `limitMillis`.
    ///
    /// The instruction ceiling catches one spinning in Lua; this catches one stuck in something the
    /// host is doing for it, which the counter never sees. Answers who was cut off, so the caller
    /// can say it out loud: a plugin cannot lag the server without the server naming it.
    std::optional<std::string> interruptIfStuck(std::int64_t limitMillis,
                                                const std::vector<std::unique_ptr<LoadedPlugin>>& plugins);

    void interruptAll(const std::vector<std::unique_ptr<LoadedPlugin>>& plugins);

private:
    std::chrono::steady_clock::time_point started_ = std::chrono::steady_clock::now();
    std::mutex mutex_;
    std::string running_;
    std::int64_t since_ = 0;
};

/// Events a plugin made up, by name. Nothing declares them: whoever emits one decides it exists.
class PluginAnnouncements {
public:
    /// `on("arena:round_start", fn)` for a name no event type has.
    ///
    /// The loosest of the ways to talk: the one emitting does not know whether anybody is there, and
    /// the one listening does not need the other to be loaded, or ever to exist.
    void announce(PluginHost& host, LoadedPlugin& plugin, const std::string& name, lua::LuaRef handler);

    /// `emit("arena:round_start", { ... })`. Answers how many listeners heard it.
    std::int32_t emit(PluginHost& host, const std::string& name, const lua::LuaRef& payload);

private:
    struct Listener {
        LoadedPlugin* plugin = nullptr;
        lua::LuaRef handler;
        std::int64_t id = 0;
    };

    std::map<std::string, std::vector<Listener>, std::less<>> listeners_;
    std::int64_t nextId_ = 0;
};

/// Loads plugins, runs them, and takes them back out again.
///
/// Three things it will not do, and they are the design rather than omissions. It never runs plugin
/// code anywhere but on an owner the server gave it, so a plugin never has to think about threads.
/// It never lets a plugin register anything it cannot retire, which is what makes unloading whole.
/// And it never lets a plugin's mistake past its own turn: an error is logged against the plugin and
/// the tick carries on.
class PluginHost {
public:
    /// What the operator has allowed each plugin beyond its own sandbox.
    using Allowed = std::function<std::set<PluginPermission>(const std::string& pluginId)>;

    /// Everything a plugin's globals are built from, which the object model fills in.
    ///
    /// A callback rather than a direct call: the bindings live above this file and reach down, and
    /// the host has no business knowing what a global is called.
    using InstallApi =
        std::function<void(PluginInstance& instance, LoadedPlugin& plugin, PluginHost& host, std::int32_t lanes)>;

    /// `lanes` is how many states each plugin gets. One in the default configuration, which is what
    /// makes the lane invisible there.
    explicit PluginHost(PluginServices& services, std::int32_t lanes = 1, lua::LuaLimits limits = {},
                        std::int32_t strikeLimit = 3);

    PluginHost(const PluginHost&) = delete;
    PluginHost& operator=(const PluginHost&) = delete;
    PluginHost(PluginHost&&) = delete;
    PluginHost& operator=(PluginHost&&) = delete;
    ~PluginHost();

    [[nodiscard]] PluginServices& services() const noexcept { return *services_; }

    [[nodiscard]] const wire::common::Logger& log() const noexcept { return log_; }

    /// What a plugin's globals are built from. Set once by whoever owns the object model.
    void setApiInstaller(InstallApi install) { installApi_ = std::move(install); }

    /// Asked for in the manifest and granted here, never the other way around: a plugin cannot talk
    /// its way into one at runtime.
    void setAllowed(Allowed allowed) { allowed_ = std::move(allowed); }

    [[nodiscard]] std::set<PluginPermission> allowed(const std::string& pluginId) const;

    /// Whether plugin code may run on the calling thread right now.
    ///
    /// Set by the server to point at the owner plugins run on. A decider has to answer where the
    /// question was asked; an observer can be handed to the owner and run later.
    void setOnPluginThread(std::function<bool()> onPluginThread) { onPluginThread_ = std::move(onPluginThread); }

    [[nodiscard]] bool onPluginThread() const { return !onPluginThread_ || onPluginThread_(); }

    /// Who is drawing the sidebar, and who is next if they let go.
    [[nodiscard]] PluginSurfaceRegistry& surfaces() noexcept { return surfaces_; }

    /// Who is listening to what. The server fires; nothing else touches it.
    [[nodiscard]] PluginEventBus& events() noexcept { return events_; }

    [[nodiscard]] std::vector<PluginStatus> statuses() const;

    [[nodiscard]] bool isLoaded(std::string_view id) const;

    /// Loads everything in `plugins/` that is not loaded yet, in dependency order.
    ///
    /// Called at startup, and again for a hot-plug: a package dropped into the folder while the
    /// server is full comes up the same way, which is why there is no separate path for it.
    PluginLoadReport loadAll();

    /// Unloads `id`, and whatever depends on it first. Answers what came out, in the order it did.
    std::vector<std::string> unload(std::string_view id);

    /// Reloads `id` from where it came from. A folder is read again from disk, so saving a file and
    /// reloading is the whole loop.
    PluginLoadReport reload(std::string_view id);

    /// Everything out, dependents first, for a shutdown that leaves nothing running.
    void unloadAll();

    /// The data of plugins that are no longer installed.
    ///
    /// Deleting it would be losing somebody's work because a folder was moved for an afternoon, so
    /// this only reports.
    [[nodiscard]] std::map<std::string, std::int64_t> orphanedData() const;

    /// Deletes what one uninstalled plugin kept, and answers how many rows went. Refuses while the
    /// plugin is loaded, because purging under a running plugin is deleting what it is still using.
    std::optional<std::int64_t> purge(const std::string& pluginId);

    [[nodiscard]] std::optional<std::string> interruptIfStuck(std::int64_t limitMillis);

    void interruptAll();

    [[nodiscard]] LoadedPlugin* pluginById(std::string_view id) const;

    /// Runs one thing a plugin asked for, and keeps its failure inside its own turn.
    void enter(LoadedPlugin& plugin, PluginInstance& instance, const lua::LuaRef& fn, std::string_view why,
               const std::function<std::int32_t(lua::LuaArgs&)>& pushArguments = {});

    /// Calls a plugin function that has to answer *now*, and answers whether it did.
    ///
    /// For a placeholder or a surface, both of which the server renders in the middle of drawing
    /// something: there is nowhere to pause to. A plugin that fails here gets a strike and the thing
    /// renders without it, rather than the whole screen failing. The results are left on the state's
    /// stack for `read` to take.
    bool ask(LoadedPlugin& plugin, const lua::LuaRef& fn, std::string_view why,
             const std::function<std::int32_t(lua::LuaArgs&)>& pushArguments,
             const std::function<void(lua::LuaArgs&)>& read);

    void report(LoadedPlugin& plugin, const lua::LuaFailure& failure, std::string_view why);

    /// Where a plugin's time is recorded, so the timings report names it beside the server's own
    /// work. A plugin that costs a millisecond a tick should be as visible as the chunk loader.
    using RecordTime = std::function<void(const std::string& pluginId, std::int64_t nanos)>;

    void setTimings(RecordTime record) { recordTime_ = std::move(record); }

private:
    /// Loads one package, or answers false and says why in the log.
    bool start(const PluginManifest& manifest, const std::shared_ptr<PluginPackage>& pkg);

    void stop(LoadedPlugin& plugin);

    [[nodiscard]] std::unique_ptr<lua::LuaSandbox> sandboxFor(LoadedPlugin& plugin, std::int32_t lane);

    /// Where `require` looks: this plugin's own `src/`, then another plugin's exported modules.
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> moduleSource(LoadedPlugin& plugin,
                                                                        std::string_view requested);

    /// Marks a plugin as running, times what it does, and answers what went wrong.
    ///
    /// The one shape every synchronous entry shares: refill the budget, count the decider depth,
    /// name the plugin for the watchdog, and record the time against it whatever happens.
    bool inside(LoadedPlugin& plugin, std::string_view why,
                const std::function<std::optional<lua::LuaFailure>(PluginInstance&)>& body);

    PluginServices* services_;
    std::int32_t lanes_;
    lua::LuaLimits limits_;
    std::int32_t strikeLimit_;
    wire::common::Logger log_{"plugins"};
    /// Held by pointer so a plugin's address survives the list growing: everything registered by one
    /// captures it.
    std::vector<std::unique_ptr<LoadedPlugin>> plugins_;
    PluginAnnouncements announcements_;
    PluginSurfaceRegistry surfaces_;
    PluginEventBus events_;
    PluginWatchdog watchdog_;
    Allowed allowed_;
    InstallApi installApi_;
    std::function<bool()> onPluginThread_;
    RecordTime recordTime_;
    std::chrono::steady_clock::time_point clock_ = std::chrono::steady_clock::now();
};

} // namespace basalt::server::plugins
