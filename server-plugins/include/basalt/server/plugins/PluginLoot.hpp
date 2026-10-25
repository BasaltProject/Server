#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins {

struct PluginLootItem {
    std::string item;
    std::int32_t count = 0;
    /// The stack's components as SNBT.
    std::string components;
};

struct PluginLootRoll {
    std::string world;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    float luck = 0.0F;
    /// Zero rolls without a fixed seed.
    std::int64_t seed = 0;
    /// The uuid of the player the loot is for, or empty.
    std::string looter;
};

struct PluginLootEnchant {
    std::string world;
    /// An enchantment id, or a tag with a leading #, the pick is made among.
    std::string options = "#minecraft:in_enchanting_table";
    std::int32_t levels = 0;
    /// Zero rolls without a fixed seed.
    std::int64_t seed = 0;
};

class PluginLoot {
public:
    PluginLoot() = default;
    PluginLoot(const PluginLoot&) = delete;
    PluginLoot& operator=(const PluginLoot&) = delete;
    PluginLoot(PluginLoot&&) = delete;
    PluginLoot& operator=(PluginLoot&&) = delete;
    virtual ~PluginLoot() = default;

    [[nodiscard]] static PluginLoot& unavailable();

    [[nodiscard]] virtual std::vector<std::string> tables() { return {}; }

    [[nodiscard]] virtual bool has(std::string_view table) {
        static_cast<void>(table);
        return false;
    }

    /// The stack with enchantments rolled as `enchant_with_levels` does; nothing when the world is unknown.
    [[nodiscard]] virtual std::optional<PluginLootItem> enchant(const PluginLootItem& stack,
                                                                const PluginLootEnchant& how) {
        static_cast<void>(stack);
        static_cast<void>(how);
        return std::nullopt;
    }

    /// What the table gives for this roll; nothing when the table or the world is unknown.
    [[nodiscard]] virtual std::optional<std::vector<PluginLootItem>> roll(std::string_view table,
                                                                          const PluginLootRoll& roll) {
        static_cast<void>(table);
        static_cast<void>(roll);
        return std::nullopt;
    }
};

inline PluginLoot& PluginLoot::unavailable() {
    static PluginLoot none;
    return none;
}

} // namespace basalt::server::plugins
