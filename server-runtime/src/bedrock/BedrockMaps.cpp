#include <basalt/server/registry/MapColors.hpp>
#include <basalt/server/runtime/MapSystem.hpp>
#include <basalt/server/runtime/bedrock/BedrockMaps.hpp>

#include <array>
#include <string_view>
#include <utility>

namespace basalt::server::runtime::bedrock {
namespace {

struct Icon {
    std::uint8_t image = 0;
    std::uint32_t color = 0xFFFFFFFFU;
};

/// The five 26.3 decorations have no Bedrock icon yet and show as the treasure cross.
constexpr std::array<Icon, 40> kIcons{{
    {0},
    {7},
    {2},
    {3},
    {4, 0xFF000000U},
    {5},
    {6},
    {13},
    {14},
    {15},
    {13, 0xFFFFFFFFU},
    {13, 0xFFF9801DU},
    {13, 0xFFC74EBDU},
    {13, 0xFF3AB3DAU},
    {13, 0xFFFED83DU},
    {13, 0xFF80C71FU},
    {13, 0xFFF38BAAU},
    {13, 0xFF474F52U},
    {13, 0xFF9D9D97U},
    {13, 0xFF169C9CU},
    {13, 0xFF8932B8U},
    {13, 0xFF3C44AAU},
    {13, 0xFF835432U},
    {13, 0xFF5E7C16U},
    {13, 0xFFB02E26U},
    {13, 0xFF1D1D21U},
    {4},
    {17},
    {18},
    {19},
    {20},
    {21},
    {22},
    {23},
    {24},
    {4},
    {4},
    {4},
    {4},
    {4},
}};

constexpr std::array<std::pair<std::string_view, std::int32_t>, 15> kExplorerData{{
    {"minecraft:ocean_monument_map", 3},
    {"minecraft:woodland_mansion_map", 4},
    {"minecraft:buried_trial_chambers_map", 14},
    {"minecraft:jungle_pyramid_map", 12},
    {"minecraft:swamp_hut_map", 13},
    {"minecraft:desert_village_map", 11},
    {"minecraft:plains_village_map", 9},
    {"minecraft:savanna_village_map", 10},
    {"minecraft:snowy_village_map", 7},
    {"minecraft:taiga_village_map", 8},
    {"minecraft:buried_treasure_map", 5},
    {"minecraft:buried_ancient_city_map", 23},
    {"minecraft:buried_mineshaft_map", 24},
    {"minecraft:desert_pyramid_map", 25},
    {"minecraft:warm_ocean_ruins_map", 26},
}};

[[nodiscard]] std::uint32_t abgrOf(std::uint8_t packed) {
    const std::uint32_t argb = server::registry::mapColors::argbOf(packed);
    return (argb & 0xFF00FF00U) | ((argb >> 16U) & 0xFFU) | ((argb & 0xFFU) << 16U);
}

} // namespace

wire::bedrock::BedrockMapData bedrockMapOf(const wire::protocol::play::MapItemDataPacket& update,
                                           std::uint8_t dimension) {
    wire::bedrock::BedrockMapData map{.mapId = update.mapId,
                                      .dimension = dimension,
                                      .locked = update.locked,
                                      .scale = update.scale,
                                      .created = {update.mapId}};
    if (update.decorations.has_value()) {
        std::vector<wire::bedrock::BedrockMapDecoration> decorations;
        for (const wire::protocol::play::MapDecorationEntry& entry : *update.decorations) {
            const Icon icon = entry.type >= 0 && static_cast<std::size_t>(entry.type) < kIcons.size()
                                  ? kIcons[static_cast<std::size_t>(entry.type)]
                                  : Icon{4};
            decorations.push_back({.image = icon.image,
                                   .rotation = static_cast<std::uint8_t>(entry.rotation),
                                   .x = entry.x,
                                   .y = entry.y,
                                   .color = icon.color});
        }
        map.decorations = std::move(decorations);
    }
    if (update.patch.has_value() && update.patch->width > 0) {
        const wire::protocol::play::MapColorPatch& patch = *update.patch;
        wire::bedrock::BedrockMapTexture texture{
            .width = patch.width, .height = patch.height, .x = patch.startX, .y = patch.startY};
        texture.colors.reserve(patch.colors.size());
        for (std::int32_t y = 0; y < patch.height; ++y) {
            for (std::int32_t x = 0; x < patch.width; ++x) {
                const auto at = static_cast<std::size_t>(x + (y * patch.width));
                texture.colors.push_back(at < patch.colors.size() ? abgrOf(patch.colors[at]) : 0U);
            }
        }
        map.texture = std::move(texture);
    }
    return map;
}

void decorateMapStack(wire::bedrock::BedrockItemStack& item, const storage::StoredItemStack& stack) {
    if (!MapSystem::isMap(stack.id)) {
        return;
    }
    if (const std::optional<std::int32_t> id = MapSystem::mapIdOf(stack.components); id.has_value()) {
        item.mapId = *id;
    }
    for (const auto& [java, data] : kExplorerData) {
        if (java == stack.id) {
            item.data = data;
        }
    }
}

} // namespace basalt::server::runtime::bedrock
