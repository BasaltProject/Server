#pragma once

#include <expected>
#include <memory>
#include <string>
#include <vector>

namespace basalt::server::plugins {
class PluginHost;
}

namespace basalt::server::bukkit {
class BukkitBridge;
class ShimHost;

struct BukkitOptions {
    std::string libjvm;
    std::string bootClasspath;
    std::string runtimeJar;
    std::vector<std::string> libraries;
    std::string pluginsFolder = "plugins";
    std::string dataFolder = ".basalt/bukkit";
};

class BukkitHost {
    struct Key {};

public:
    explicit BukkitHost(Key);
    BukkitHost(const BukkitHost&) = delete;
    BukkitHost& operator=(const BukkitHost&) = delete;
    BukkitHost(BukkitHost&&) = delete;
    BukkitHost& operator=(BukkitHost&&) = delete;
    ~BukkitHost();

    /// Creates the Shim VM and boots the Java runtime; plugins are enabled later by `enable`.
    [[nodiscard]] static std::expected<std::unique_ptr<BukkitHost>, std::string> start(const BukkitOptions& options,
                                                                                       plugins::PluginHost& plugins);

    /// Loads and enables every plugin; call it on the thread plugins treat as main (the tick thread).
    bool enable();

    struct PluginState {
        std::string name;
        std::string version;
        bool enabled = false;
    };

    /// What the Java plugin manager holds right now.
    [[nodiscard]] std::vector<PluginState> plugins() const;

    void stop();

private:
    std::unique_ptr<ShimHost> shim_;
    std::unique_ptr<BukkitBridge> bridge_;
    bool stopped_ = false;
};
} // namespace basalt::server::bukkit
