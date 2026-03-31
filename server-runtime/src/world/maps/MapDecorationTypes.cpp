#include <basalt/server/runtime/world/maps/MapDecorationTypes.hpp>

#include <array>
#include <cstddef>
#include <string>

namespace basalt::server::runtime::world::maps::decorationTypes {
namespace {

/// parity: `MapDecorationTypes`, in registration order.
constexpr std::array<MapDecorationType, 40> kTypes{{
    {"player", false, true},
    {"frame", true, true},
    {"red_marker", false, true},
    {"blue_marker", false, true},
    {"target_x", true, false},
    {"target_point", true, false},
    {"player_off_map", false, true},
    {"player_off_limits", false, true},
    {"mansion", true, false},
    {"monument", true, false},
    {"banner_white", true, true},
    {"banner_orange", true, true},
    {"banner_magenta", true, true},
    {"banner_light_blue", true, true},
    {"banner_yellow", true, true},
    {"banner_lime", true, true},
    {"banner_pink", true, true},
    {"banner_gray", true, true},
    {"banner_light_gray", true, true},
    {"banner_cyan", true, true},
    {"banner_purple", true, true},
    {"banner_blue", true, true},
    {"banner_brown", true, true},
    {"banner_green", true, true},
    {"banner_red", true, true},
    {"banner_black", true, true},
    {"red_x", true, false},
    {"village_desert", true, false},
    {"village_plains", true, false},
    {"village_savanna", true, false},
    {"village_snowy", true, false},
    {"village_taiga", true, false},
    {"jungle_temple", true, false},
    {"swamp_hut", true, false},
    {"trial_chambers", true, false},
    {"abandoned_camp", true, false},
    {"ancient_city", true, false},
    {"desert_pyramid", true, false},
    {"mineshaft", true, false},
    {"ocean_ruin_warm", true, false},
}};

constexpr std::string_view kNamespace = "minecraft:";

} // namespace

const MapDecorationType* byId(std::int32_t id) {
    return id < 0 || static_cast<std::size_t>(id) >= kTypes.size() ? nullptr : &kTypes[static_cast<std::size_t>(id)];
}

std::optional<std::int32_t> idOf(std::string_view name) {
    if (name.starts_with(kNamespace)) {
        name.remove_prefix(kNamespace.size());
    }
    for (std::size_t id = 0; id < kTypes.size(); ++id) {
        if (kTypes[id].name == name) {
            return static_cast<std::int32_t>(id);
        }
    }
    return std::nullopt;
}

std::optional<std::int32_t> bannerOf(std::string_view color) { return idOf("banner_" + std::string{color}); }

} // namespace basalt::server::runtime::world::maps::decorationTypes
