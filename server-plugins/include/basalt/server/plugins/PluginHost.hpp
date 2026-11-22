#pragma once

#include <basalt/server/plugins/PluginChannelHost.hpp>
#include <basalt/server/plugins/PluginCommandSpec.hpp>
#include <basalt/server/plugins/PluginEvents.hpp>
#include <basalt/server/plugins/PluginPacketRegistry.hpp>
#include <basalt/server/plugins/PluginPackets.hpp>
#include <basalt/server/plugins/PluginRegistrations.hpp>
#include <basalt/server/plugins/PluginServices.hpp>
#include <basalt/server/plugins/PluginSurfaces.hpp>
#include <basalt/server/plugins/PluginText.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/common/timings/Timings.hpp>
#include <basalt/wire/common/toml/Toml.hpp>
#include <basalt/wire/plugins/PluginFailure.hpp>
#include <basalt/wire/plugins/PluginManifest.hpp>
#include <basalt/wire/plugins/PluginPackage.hpp>
#include <basalt/wire/plugins/PluginSignature.hpp>
#include <basalt/wire/wasm/Engine.hpp>
#include <basalt/wire/wasm/Error.hpp>

#include <atomic>
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
class WasmPlugin;

class LoadedPlugin {
public:
    LoadedPlugin(wire::plugins::PluginManifest manifest, std::shared_ptr<wire::plugins::PluginPackage> pkg,
                 wire::common::Logger log, wire::common::toml::TomlTable config, std::string folder,
                 std::map<std::string, std::string, std::less<>> files);
    ~LoadedPlugin();

    wire::plugins::PluginManifest manifest;
    std::shared_ptr<wire::plugins::PluginPackage> pkg;
    wire::common::Logger log;
    wire::common::toml::TomlTable config;
    std::string folder;
    std::map<std::string, std::string, std::less<>> files;

    PluginRegistrations registrations;
    PluginText text;
    std::unique_ptr<WasmPlugin> wasm;
    std::vector<std::pair<std::string, PluginCommandSpec>> commands;

    std::int32_t strikes = 0;

    std::int32_t failures = 0;

    bool disabled = false;

    /// Its package carries a signature from a key the operator trusts.
    bool bytecodeVerified = false;

    [[nodiscard]] const PluginCommandSpec* commandNamed(std::string_view name) const;
};

struct PluginStatus {
    std::string id;
    std::string name;
    std::string version;
    std::string origin;
    std::string runtime = "WASM";
    bool editable = false;
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

class PluginHost {
public:
    using Allowed = std::function<std::set<wire::plugins::PluginPermission>(const std::string& pluginId)>;

    explicit PluginHost(PluginServices& services, std::int64_t memoryBytes = 32LL * 1024 * 1024,
                        std::int32_t strikeLimit = 3);

    PluginHost(const PluginHost&) = delete;
    PluginHost& operator=(const PluginHost&) = delete;
    PluginHost(PluginHost&&) = delete;
    PluginHost& operator=(PluginHost&&) = delete;
    ~PluginHost();

    [[nodiscard]] PluginServices& services() const noexcept { return *services_; }

    [[nodiscard]] const wire::common::Logger& log() const noexcept { return log_; }

    void setAllowed(Allowed allowed) { allowed_ = std::move(allowed); }

    /// Where compiled WebAssembly modules are kept between runs; empty compiles every time.
    void setWasmCacheDirectory(std::string directory) { wasmCache_ = std::move(directory); }

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

    /// Server work that is slow by nature (making a world) is not charged to the plugin's turn.
    void pauseWatchdog();

    void resumeWatchdog(std::string_view pluginId);

    [[nodiscard]] LoadedPlugin* pluginById(std::string_view id) const;

    /// The one lock every plugin call runs under; a WebAssembly instance is driven from one thread at a time.
    [[nodiscard]] std::recursive_mutex& turnMutex() noexcept { return turnMutex_; }

    void beginWasmTurn(LoadedPlugin& plugin);

    void endWasmTurn(LoadedPlugin& plugin, std::int64_t nanos);

    void report(LoadedPlugin& plugin, const wire::plugins::PluginFailure& failure, std::string_view why);

    void reportWasm(LoadedPlugin& plugin, const wire::wasm::Failure& failure, std::string_view why);

    [[nodiscard]] PluginPacketRegistry& packets() noexcept { return packets_; }

    [[nodiscard]] PluginHandle listenNative(const std::string& channel, PluginChannelHost::NativeListener listener) {
        return channels_.listenNative(channel, std::move(listener));
    }

    [[nodiscard]] bool listeningOnChannel(std::string_view channel) const;

    void channelMessage(const std::string& channel, const std::string& playerUuid, std::span<const std::uint8_t> bytes);

    using RecordTime = std::function<void(const std::string& pluginId, std::int64_t nanos)>;

    void setTimings(RecordTime record) { recordTime_ = std::move(record); }

    /// Where code that runs outside a plugin, such as the Bukkit bridge, opens its own sections; measures nothing until
    /// set.
    void setProfiler(wire::common::Timings* timings) { profiler_ = timings == nullptr ? &idleProfiler_ : timings; }

    [[nodiscard]] wire::common::Timings& profiler() const noexcept { return *profiler_; }

    /// Counts time spent for a plugin that does not run as a WebAssembly module, such as a Bukkit one.
    void recordTime(const std::string& pluginId, std::int64_t nanos) const {
        if (recordTime_) {
            recordTime_(pluginId, nanos);
        }
    }

private:
    bool start(const wire::plugins::PluginManifest& manifest, const std::shared_ptr<wire::plugins::PluginPackage>& pkg);

    bool startWasm(std::unique_ptr<LoadedPlugin> plugin, bool verified);

    void stop(LoadedPlugin& plugin);

    PluginServices* services_;
    std::int64_t memoryBytes_;
    std::string wasmCache_;
    std::optional<wire::wasm::Engine> wasmEngine_;
    std::int32_t strikeLimit_;
    wire::common::Logger log_{"plugins"};
    std::vector<std::unique_ptr<LoadedPlugin>> plugins_;
    PluginSurfaceRegistry surfaces_;
    std::recursive_mutex turnMutex_;
    PluginPacketRegistry packets_;
    PluginChannelHost channels_;
    PluginEventBus events_;
    PluginWatchdog watchdog_;
    Allowed allowed_;
    std::vector<wire::plugins::signature::PublicKey> trustedKeys_;
    std::function<bool()> onPluginThread_;
    RecordTime recordTime_;
    wire::common::Timings idleProfiler_;
    wire::common::Timings* profiler_ = &idleProfiler_;
    std::chrono::steady_clock::time_point clock_ = std::chrono::steady_clock::now();
};
} // namespace basalt::server::plugins
