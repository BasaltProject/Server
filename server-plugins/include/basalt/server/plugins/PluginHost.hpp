#pragma once

#include <basalt/server/plugins/PluginChannelHost.hpp>
#include <basalt/server/plugins/PluginCommandSpec.hpp>
#include <basalt/server/plugins/PluginEvents.hpp>
#include <basalt/server/plugins/PluginPacketRegistry.hpp>
#include <basalt/server/plugins/PluginPackets.hpp>
#include <basalt/server/plugins/PluginRegistrations.hpp>
#include <basalt/server/plugins/PluginServiceRegistry.hpp>
#include <basalt/server/plugins/PluginServices.hpp>
#include <basalt/server/plugins/PluginSurfaces.hpp>
#include <basalt/server/plugins/PluginText.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/common/toml/Toml.hpp>
#include <basalt/wire/lua/LuaRef.hpp>
#include <basalt/wire/lua/LuaSandbox.hpp>
#include <basalt/wire/plugins/PluginManifest.hpp>
#include <basalt/wire/plugins/PluginPackage.hpp>
#include <basalt/wire/plugins/PluginSignature.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::plugins {
class PluginHost;
class PluginCommandDispatch;
class PluginEventDispatch;

struct PluginInstance {
    std::int32_t lane = 0;
    std::unique_ptr<wire::lua::LuaSandbox> sandbox;
    std::vector<wire::lua::LuaRef> onEnable;
    std::vector<wire::lua::LuaRef> onDisable;

    std::int32_t deciderDepth = 0;
};

struct CommandEntry {
    explicit CommandEntry(PluginCommandSpec commandSpec) : spec(std::move(commandSpec)) {}

    CommandEntry(const CommandEntry&) = delete;
    CommandEntry& operator=(const CommandEntry&) = delete;
    CommandEntry(CommandEntry&&) noexcept = default;
    CommandEntry& operator=(CommandEntry&&) noexcept = default;

    PluginCommandSpec spec;
    std::map<std::int32_t, wire::lua::LuaRef> handlers;
    std::int32_t nextLane = 0;
};

class LoadedPlugin {
public:
    LoadedPlugin(wire::plugins::PluginManifest manifest, std::shared_ptr<wire::plugins::PluginPackage> pkg,
                 wire::common::Logger log, wire::common::toml::TomlTable config, std::string folder,
                 std::map<std::string, std::string, std::less<>> files);

    wire::plugins::PluginManifest manifest;
    std::shared_ptr<wire::plugins::PluginPackage> pkg;
    wire::common::Logger log;
    wire::common::toml::TomlTable config;
    std::string folder;
    std::map<std::string, std::string, std::less<>> files;

    PluginRegistrations registrations;
    PluginText text;
    std::vector<std::unique_ptr<PluginInstance>> instances;
    std::vector<std::pair<std::string, CommandEntry>> commands;

    std::int32_t strikes = 0;

    std::int32_t failures = 0;

    bool disabled = false;

    /// Its bytecode carries a signature from a key the operator trusts.
    bool bytecodeVerified = false;

    [[nodiscard]] CommandEntry* commandNamed(std::string_view name);
};

struct PluginStatus {
    std::string id;
    std::string name;
    std::string version;
    std::string origin;
    bool editable = false;
    std::int32_t lanes = 0;
    std::int32_t registrations = 0;
    std::int32_t errorStreak = 0;
    std::int32_t errors = 0;
    bool disabled = false;
    std::vector<std::string> commands;
};

struct PluginLoadReport {
    std::vector<std::string> loaded;
    std::vector<std::pair<std::string, std::string>> refused;
};

class PluginWatchdog {
public:
    [[nodiscard]] std::int64_t nowMillis() const;

    void mark(std::string_view pluginId);

    void clear();

    std::optional<std::string> interruptIfStuck(std::int64_t limitMillis,
                                                const std::vector<std::unique_ptr<LoadedPlugin>>& plugins);

    void interruptAll(const std::vector<std::unique_ptr<LoadedPlugin>>& plugins);

private:
    std::chrono::steady_clock::time_point started_ = std::chrono::steady_clock::now();
    std::mutex mutex_;
    std::string running_;
    std::int64_t since_ = 0;
};

class PluginAnnouncements {
public:
    void announce(PluginHost& host, LoadedPlugin& plugin, const std::string& name, wire::lua::LuaRef handler);

    std::int32_t emit(PluginHost& host, const std::string& name, const wire::lua::LuaRef& payload);

private:
    struct Listener {
        LoadedPlugin* plugin = nullptr;
        wire::lua::LuaRef handler;
        std::int64_t id = 0;
    };

    std::map<std::string, std::vector<Listener>, std::less<>> listeners_;
    std::int64_t nextId_ = 0;
};

class PluginHost {
public:
    using Allowed = std::function<std::set<wire::plugins::PluginPermission>(const std::string& pluginId)>;

    using InstallApi =
        std::function<void(PluginInstance& instance, LoadedPlugin& plugin, PluginHost& host, std::int32_t lanes)>;

    explicit PluginHost(PluginServices& services, std::int32_t lanes = 1, wire::lua::LuaLimits limits = {},
                        std::int32_t strikeLimit = 3);

    PluginHost(const PluginHost&) = delete;
    PluginHost& operator=(const PluginHost&) = delete;
    PluginHost(PluginHost&&) = delete;
    PluginHost& operator=(PluginHost&&) = delete;
    ~PluginHost();

    [[nodiscard]] PluginServices& services() const noexcept { return *services_; }

    [[nodiscard]] const wire::common::Logger& log() const noexcept { return log_; }

    void setApiInstaller(InstallApi install) { installApi_ = std::move(install); }

    void setAllowed(Allowed allowed) { allowed_ = std::move(allowed); }

    void setTrustedKeys(std::vector<wire::plugins::signature::PublicKey> keys) { trustedKeys_ = std::move(keys); }

    [[nodiscard]] std::set<wire::plugins::PluginPermission> allowed(const std::string& pluginId) const;

    void setOnPluginThread(std::function<bool()> onPluginThread) { onPluginThread_ = std::move(onPluginThread); }

    [[nodiscard]] bool onPluginThread() const { return !onPluginThread_ || onPluginThread_(); }

    [[nodiscard]] PluginSurfaceRegistry& surfaces() noexcept { return surfaces_; }

    [[nodiscard]] PluginEventBus& events() noexcept { return events_; }

    [[nodiscard]] std::vector<PluginStatus> statuses() const;

    [[nodiscard]] bool isLoaded(std::string_view id) const;

    PluginLoadReport loadAll();

    std::vector<std::string> unload(std::string_view id);

    PluginLoadReport reload(std::string_view id);

    void unloadAll();

    [[nodiscard]] std::map<std::string, std::int64_t> orphanedData() const;

    std::optional<std::int64_t> purge(const std::string& pluginId);

    [[nodiscard]] std::optional<std::string> interruptIfStuck(std::int64_t limitMillis);

    void interruptAll();

    [[nodiscard]] LoadedPlugin* pluginById(std::string_view id) const;

    void enter(LoadedPlugin& plugin, PluginInstance& instance, const wire::lua::LuaRef& fn, std::string_view why,
               const std::function<std::int32_t(wire::lua::LuaArgs&)>& pushArguments = {});

    bool ask(LoadedPlugin& plugin, const wire::lua::LuaRef& fn, std::string_view why,
             const std::function<std::int32_t(wire::lua::LuaArgs&)>& pushArguments,
             const std::function<void(wire::lua::LuaArgs&)>& read);

    void report(LoadedPlugin& plugin, const wire::lua::LuaFailure& failure, std::string_view why);

    void resuming(LoadedPlugin& plugin, const std::function<void()>& resume);

    [[nodiscard]] std::string answer(LoadedPlugin& plugin, const wire::lua::LuaRef& fn, PluginPlayerRef* player,
                                     std::string_view why);

    [[nodiscard]] bool asks(LoadedPlugin& plugin, const wire::lua::LuaRef& fn, PluginPlayerRef* player,
                            std::string_view why);

    void clicked(LoadedPlugin& plugin, const wire::lua::LuaRef& handler, PluginPlayerRef& player, std::int32_t slot);

    void formAnswered(LoadedPlugin& plugin, const wire::lua::LuaRef& handler, PluginPlayerRef& player,
                      const PluginForm& form, std::size_t button, const std::map<std::string, std::string>& values);

    void npcClicked(LoadedPlugin& plugin, const wire::lua::LuaRef& handler, const std::string& playerUuid);

    void deliverPacket(LoadedPlugin& plugin, const wire::lua::LuaRef& fn, PluginPacket& packet);

    [[nodiscard]] PluginServiceRegistry& serviceRegistry() noexcept { return services_registry_; }

    [[nodiscard]] PluginPacketRegistry& packets() noexcept { return packets_; }

    void registerCommand(LoadedPlugin& plugin, PluginInstance& instance, const PluginCommandSpec& spec,
                         wire::lua::LuaRef handler);

    void registerEventHandler(LoadedPlugin& plugin, PluginInstance& instance, const PluginEventType& type,
                              PluginEventPriority priority, bool observer, bool wantsCancelled,
                              std::map<std::string, PluginEventValue, std::less<>> filters, wire::lua::LuaRef handler);

    void announce(LoadedPlugin& plugin, const std::string& name, wire::lua::LuaRef handler);

    [[nodiscard]] std::int32_t emit(const std::string& name, const wire::lua::LuaRef& payload);

    void listenOn(LoadedPlugin& plugin, const std::string& channel, wire::lua::LuaRef fn);

    [[nodiscard]] PluginHandle listenNative(const std::string& channel, PluginChannelHost::NativeListener listener) {
        return channels_.listenNative(channel, std::move(listener));
    }

    [[nodiscard]] bool listeningOnChannel(std::string_view channel) const;

    void channelMessage(const std::string& channel, const std::string& playerUuid, std::span<const std::uint8_t> bytes);

    using RecordTime = std::function<void(const std::string& pluginId, std::int64_t nanos)>;

    void setTimings(RecordTime record) { recordTime_ = std::move(record); }

private:
    bool start(const wire::plugins::PluginManifest& manifest, const std::shared_ptr<wire::plugins::PluginPackage>& pkg);

    void stop(LoadedPlugin& plugin);

    [[nodiscard]] std::unique_ptr<wire::lua::LuaSandbox> sandboxFor(LoadedPlugin& plugin, std::int32_t lane);

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> moduleSource(LoadedPlugin& plugin,
                                                                        std::string_view requested);

    bool inside(LoadedPlugin& plugin, std::string_view why,
                const std::function<std::optional<wire::lua::LuaFailure>(PluginInstance&)>& body);

    PluginServices* services_;
    std::int32_t lanes_;
    wire::lua::LuaLimits limits_;
    std::int32_t strikeLimit_;
    wire::common::Logger log_{"plugins"};
    std::vector<std::unique_ptr<LoadedPlugin>> plugins_;
    PluginAnnouncements announcements_;
    PluginServiceRegistry services_registry_;
    PluginSurfaceRegistry surfaces_;
    // Lua states are single threaded: a packet listener on a connection thread waits for the tick thread's call.
    std::recursive_mutex luaMutex_;
    PluginPacketRegistry packets_;
    PluginChannelHost channels_;
    std::unique_ptr<PluginCommandDispatch> commandDispatch_;
    std::unique_ptr<PluginEventDispatch> eventDispatch_;
    PluginEventBus events_;
    PluginWatchdog watchdog_;
    Allowed allowed_;
    std::vector<wire::plugins::signature::PublicKey> trustedKeys_;
    InstallApi installApi_;
    std::function<bool()> onPluginThread_;
    RecordTime recordTime_;
    std::chrono::steady_clock::time_point clock_ = std::chrono::steady_clock::now();
};
} // namespace basalt::server::plugins
