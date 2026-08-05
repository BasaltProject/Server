#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins {

struct PluginAdvancement {
    std::string id;
    std::optional<std::string> parent;
    std::string root;
    std::vector<std::string> children;
    std::vector<std::string> criteria;
    std::vector<std::vector<std::string>> requirements;
    bool hasDisplay = false;
    /// JSON text components.
    std::string title;
    std::string description;
    std::string iconItem;
    /// "task", "goal" or "challenge".
    std::string frame = "task";
    std::optional<std::string> background;
    bool toast = true;
    bool announce = true;
    bool hidden = false;
    float x = 0.0F;
    float y = 0.0F;
    std::int32_t experience = 0;
    std::vector<std::string> recipes;
    std::vector<std::string> loot;
};

class PluginAdvancements {
public:
    PluginAdvancements() = default;
    PluginAdvancements(const PluginAdvancements&) = delete;
    PluginAdvancements& operator=(const PluginAdvancements&) = delete;
    PluginAdvancements(PluginAdvancements&&) = delete;
    PluginAdvancements& operator=(PluginAdvancements&&) = delete;
    virtual ~PluginAdvancements() = default;

    [[nodiscard]] static PluginAdvancements& unavailable();

    [[nodiscard]] virtual std::vector<std::string> ids() { return {}; }

    [[nodiscard]] virtual std::optional<PluginAdvancement> advancement(std::string_view id) {
        static_cast<void>(id);
        return std::nullopt;
    }

    /// The criteria the player has, with when each was obtained in epoch milliseconds.
    [[nodiscard]] virtual std::map<std::string, std::int64_t> obtained(std::string_view uuid, std::string_view id) {
        static_cast<void>(uuid);
        static_cast<void>(id);
        return {};
    }

    virtual bool grant(std::string_view uuid, std::string_view id, std::string_view criterion) {
        static_cast<void>(uuid);
        static_cast<void>(id);
        static_cast<void>(criterion);
        return false;
    }

    virtual bool revoke(std::string_view uuid, std::string_view id, std::string_view criterion) {
        static_cast<void>(uuid);
        static_cast<void>(id);
        static_cast<void>(criterion);
        return false;
    }
};

inline PluginAdvancements& PluginAdvancements::unavailable() {
    static PluginAdvancements none;
    return none;
}

} // namespace basalt::server::plugins
