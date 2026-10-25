#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins {

struct PluginBossBar {
    /// A namespaced key for a kept bar (it shows in /bossbar and survives a restart), else a generated id.
    std::string id;
    bool keyed = false;
    /// A JSON text component.
    std::string title;
    /// "pink", "blue", "red", "green", "yellow", "purple" or "white".
    std::string color = "purple";
    /// "progress", "notched_6", "notched_10", "notched_12" or "notched_20".
    std::string style = "progress";
    bool darkenSky = false;
    bool bossMusic = false;
    bool fog = false;
    double progress = 1.0;
    bool visible = true;
};

/// Bars drawn on the top of a player's screen: ones plugins keep for a moment and ones the server keeps by name.
class PluginBossBars {
public:
    PluginBossBars() = default;
    PluginBossBars(const PluginBossBars&) = delete;
    PluginBossBars& operator=(const PluginBossBars&) = delete;
    PluginBossBars(PluginBossBars&&) = delete;
    PluginBossBars& operator=(PluginBossBars&&) = delete;
    virtual ~PluginBossBars() = default;

    [[nodiscard]] static PluginBossBars& unavailable();

    /// The id the bar has, or nothing when a kept bar of that key exists.
    virtual std::optional<std::string> create(const PluginBossBar& bar) {
        static_cast<void>(bar);
        return std::nullopt;
    }

    [[nodiscard]] virtual std::optional<PluginBossBar> get(std::string_view id) {
        static_cast<void>(id);
        return std::nullopt;
    }

    [[nodiscard]] virtual std::vector<std::string> kept() { return {}; }

    virtual bool update(const PluginBossBar& bar) {
        static_cast<void>(bar);
        return false;
    }

    virtual bool remove(std::string_view id) {
        static_cast<void>(id);
        return false;
    }

    virtual bool addPlayer(std::string_view id, std::string_view uuid) {
        static_cast<void>(id);
        static_cast<void>(uuid);
        return false;
    }

    virtual bool removePlayer(std::string_view id, std::string_view uuid) {
        static_cast<void>(id);
        static_cast<void>(uuid);
        return false;
    }

    virtual void clearPlayers(std::string_view id) { static_cast<void>(id); }

    [[nodiscard]] virtual std::vector<std::string> players(std::string_view id) {
        static_cast<void>(id);
        return {};
    }
};

inline PluginBossBars& PluginBossBars::unavailable() {
    static PluginBossBars none;
    return none;
}

} // namespace basalt::server::plugins
