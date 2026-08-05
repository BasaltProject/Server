#pragma once

#include <basalt/server/bukkit/BukkitHost.hpp>
#include <basalt/server/bukkit/ShimHost.hpp>
#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::bukkit {
class BukkitBridge {
public:
    BukkitBridge(ShimHost& shim, plugins::PluginHost& plugins, BukkitOptions options);

    BukkitBridge(const BukkitBridge&) = delete;
    BukkitBridge& operator=(const BukkitBridge&) = delete;
    ~BukkitBridge();

    [[nodiscard]] static BukkitBridge& current();
    [[nodiscard]] static BukkitBridge* live();
    static void seal();
    static void replyToCommand(const std::string& message);
    [[nodiscard]] static JNIEnv* liveEnv();

    [[nodiscard]] std::expected<void, std::string> boot();
    bool enable();
    bool shutdown();

    [[nodiscard]] std::vector<BukkitHost::PluginState> javaPlugins();

    [[nodiscard]] plugins::PluginHost& plugins() noexcept { return *plugins_; }

    [[nodiscard]] plugins::PluginServices& services() noexcept { return plugins_->services(); }

    [[nodiscard]] const wire::common::Logger& log() const noexcept { return log_; }

    [[nodiscard]] JNIEnv* env() const { return shim_->env(); }

    [[nodiscard]] bool stopping() const noexcept { return stopping_.load(); }

    void reportFailure(JNIEnv* env, std::string_view where) { reportJavaFailure(env, where); }

    [[nodiscard]] const plugins::PluginEventType* typeNamed(const std::string& name) const;
    void scheduleAfter(std::int64_t id, std::int32_t delay);
    void scheduleEvery(std::int64_t id, std::int32_t delay, std::int32_t period);
    void cancelTask(std::int64_t id);
    bool subscribe(const std::string& event, std::int32_t priority);
    void registerCommand(const std::string& plugin, const std::string& name, const std::string& permission);
    void unregisterCommands(const std::string& plugin);
    void listenChannel(const std::string& channel);

private:
    void runTask(std::int64_t id);

    struct Snapshot {
        const plugins::PluginEventType* type = nullptr;
        std::vector<plugins::PluginEventValue> values;
        std::int64_t generation = 0;
        bool cancelled = false;
    };

    bool deliver(const std::string& event, std::int32_t priority, Snapshot& snapshot);
    bool runCommand(const std::string& name, plugins::PluginCommandCall& call);
    void reportJavaFailure(JNIEnv* env, std::string_view where);

    ShimHost* shim_;
    plugins::PluginHost* plugins_;
    BukkitOptions options_;
    wire::common::Logger log_{"Bukkit"};
    jclass runtime_ = nullptr;
    jmethodID bootMethod_ = nullptr;
    jmethodID enableMethod_ = nullptr;
    jmethodID shutdownMethod_ = nullptr;
    jmethodID taskMethod_ = nullptr;
    jmethodID eventMethod_ = nullptr;
    jmethodID commandMethod_ = nullptr;
    jmethodID channelMethod_ = nullptr;
    jmethodID listMethod_ = nullptr;
    std::mutex tasksMutex_;
    std::map<std::int64_t, plugins::PluginHandle> tasks_;
    std::set<std::pair<std::string, std::int32_t>> subscribed_;
    std::multimap<std::string, plugins::PluginHandle> commands_;
    std::vector<plugins::PluginHandle> eventHandles_;
    std::map<std::string, plugins::PluginHandle> channels_;
    bool warnedAboutThread_ = false;
    std::atomic<bool> stopping_{false};
};

void registerNatives(JNIEnv* env, jclass natives);
void registerPacketNatives(JNIEnv* env, jclass natives);
void registerWorldNatives(JNIEnv* env, jclass natives);
void registerInventoryNatives(JNIEnv* env, jclass natives);
void registerEntityNatives(JNIEnv* env, jclass natives);
void registerMiscNatives(JNIEnv* env, jclass natives);
void registerPlayerNatives(JNIEnv* env, jclass natives);
void registerWorldEnvNatives(JNIEnv* env, jclass natives);
void registerWorldEnvMoreNatives(JNIEnv* env, jclass natives);
void registerLeftoverNatives(JNIEnv* env, jclass natives);
void registerRestNatives(JNIEnv* env, jclass natives);

[[nodiscard]] std::string toStd(JNIEnv* env, jstring text);
[[nodiscard]] jstring toJava(JNIEnv* env, std::string_view text);
} // namespace basalt::server::bukkit
