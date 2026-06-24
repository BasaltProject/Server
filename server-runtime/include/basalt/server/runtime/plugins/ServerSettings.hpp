#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::config {
struct BasaltConfig;
class ServerListConfig;
} // namespace basalt::server::runtime::config

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::plugins {

/// The settings of the server a plugin can read, and the few it can change while it runs.
class ServerSettings {
public:
    struct Hooks {
        std::function<const config::BasaltConfig&()> config;
        std::function<const config::ServerListConfig&()> serverList;
        std::function<std::vector<world::LoadedWorld*>()> worlds;
        std::function<std::int32_t()> idleTimeout;
        std::function<void(std::int32_t)> setIdleTimeout;
        std::function<bool()> autoSave;
        std::function<void(bool)> setAutoSave;
    };

    void use(Hooks hooks) { hooks_ = std::move(hooks); }

    [[nodiscard]] std::vector<std::string> keys() const;

    [[nodiscard]] std::optional<std::string> get(std::string_view key) const;

    /// False for a key that does not exist, cannot be changed, or was given a value it cannot take.
    bool set(std::string_view key, std::string_view value);

    /// The motd a plugin set, if any; the server list shows it in place of the configured one.
    [[nodiscard]] std::optional<std::string> motdOverride() const;

    [[nodiscard]] std::optional<std::int32_t> maxPlayersOverride() const;

    [[nodiscard]] std::int32_t pauseWhenEmptySeconds() const;

    /// False once a plugin forbids the server to pause.
    [[nodiscard]] bool pauseAllowed() const { return pauseAllowed_.load(std::memory_order_relaxed); }

    [[nodiscard]] std::optional<std::string> respawnWorldOverride() const;

    [[nodiscard]] std::int32_t spawnProtection() const;

private:
    Hooks hooks_;
    mutable std::mutex mutex_;
    std::optional<std::string> motd_;
    std::optional<std::int32_t> maxPlayers_;
    std::atomic<std::int32_t> pauseWhenEmpty_{-1};
    std::atomic<std::int32_t> spawnProtection_{-1};
    std::atomic<bool> pauseAllowed_{true};
    std::optional<std::string> respawnWorld_;
};

} // namespace basalt::server::runtime::plugins
