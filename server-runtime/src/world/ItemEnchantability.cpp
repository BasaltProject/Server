#include <basalt/server/runtime/world/ItemEnchantability.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <utility>

namespace basalt::server::runtime::world::itemEnchantability {
namespace {

constexpr std::array<std::pair<std::string_view, std::int32_t>, 7> kSpecial{{
    {"book", 1},
    {"bow", 1},
    {"crossbow", 1},
    {"fishing_rod", 1},
    {"mace", 15},
    {"trident", 1},
    // parity: `ArmorMaterials.TURTLE_SCUTE`, which is the only armour whose name has no material
    // prefix.
    {"turtle_helmet", 9},
}};

constexpr std::array<std::string_view, 4> kArmourSuffixes{"_helmet", "_chestplate", "_leggings", "_boots"};
constexpr std::array<std::string_view, 6> kToolSuffixes{"_sword", "_shovel", "_pickaxe", "_axe", "_hoe", "_spear"};

/// parity: `ArmorMaterials`' own `enchantmentValue`.
constexpr std::array<std::pair<std::string_view, std::int32_t>, 7> kArmourMaterials{{
    {"leather_", 15},
    {"copper_", 8},
    {"chainmail_", 12},
    {"iron_", 9},
    {"golden_", 25},
    {"diamond_", 10},
    {"netherite_", 15},
}};

/// parity: `ToolMaterial`'s own `enchantmentValue`, which is a different table from the armour's:
/// a golden pickaxe takes enchantments better than a golden helmet does.
constexpr std::array<std::pair<std::string_view, std::int32_t>, 7> kToolMaterials{{
    {"wooden_", 15},
    {"stone_", 5},
    {"copper_", 13},
    {"iron_", 14},
    {"golden_", 22},
    {"diamond_", 10},
    {"netherite_", 15},
}};

[[nodiscard]] bool endsWithAny(std::string_view path, std::span<const std::string_view> suffixes) {
    return std::ranges::any_of(suffixes, [path](std::string_view suffix) { return path.ends_with(suffix); });
}

[[nodiscard]] std::optional<std::int32_t>
byMaterial(std::string_view path, std::span<const std::pair<std::string_view, std::int32_t>> materials) {
    for (const auto& [prefix, value] : materials) {
        if (path.starts_with(prefix)) {
            return value;
        }
    }
    return std::nullopt;
}

} // namespace

std::optional<std::int32_t> valueOf(std::string_view item) {
    std::string_view path = item;
    if (path.starts_with("minecraft:")) {
        path.remove_prefix(std::string_view{"minecraft:"}.size());
    }
    for (const auto& [name, value] : kSpecial) {
        if (path == name) {
            return value;
        }
    }
    if (endsWithAny(path, kArmourSuffixes)) {
        return byMaterial(path, kArmourMaterials);
    }
    if (endsWithAny(path, kToolSuffixes)) {
        return byMaterial(path, kToolMaterials);
    }
    return std::nullopt;
}

} // namespace basalt::server::runtime::world::itemEnchantability
