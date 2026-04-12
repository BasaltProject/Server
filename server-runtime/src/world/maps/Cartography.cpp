#include <basalt/server/runtime/MapSystem.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/maps/Cartography.hpp>

namespace basalt::server::runtime::world::maps {
namespace {

constexpr std::string_view kPostProcessing = "minecraft:map_post_processing";
constexpr std::int8_t kMaxScale = 4;

[[nodiscard]] storage::StoredItemStack withCount(const storage::StoredItemStack& stack, std::int32_t count) {
    storage::StoredItemStack copy = stack;
    copy.count = count;
    return copy;
}

} // namespace

std::optional<storage::StoredItemStack> cartographyResult(const storage::StoredItemStack& map,
                                                          const storage::StoredItemStack& additional,
                                                          const CartographyFacts& facts) {
    if (additional.id == "minecraft:paper" && facts.extendable && !facts.locked && facts.scale < kMaxScale) {
        storage::StoredItemStack made = withCount(map, 1);
        made.components.putString(kPostProcessing, "scale");
        return made;
    }
    if (additional.id == "minecraft:glass_pane" && !facts.locked) {
        storage::StoredItemStack made = withCount(map, 1);
        made.components.putString(kPostProcessing, "lock");
        return made;
    }
    if (additional.id == "minecraft:map") {
        return withCount(map, 2);
    }
    return std::nullopt;
}

std::optional<storage::StoredItemStack> cartographyResult(const storage::StoredItemStack& map,
                                                          const storage::StoredItemStack& additional, MapSystem& maps,
                                                          const realItemTags::TagMembers* itemTags) {
    const std::optional<std::int32_t> id = MapSystem::mapIdOf(map.components);
    const std::optional<MapSystem::Summary> data = id.has_value() ? maps.summaryOf(*id) : std::nullopt;
    if (!data.has_value()) {
        return std::nullopt;
    }
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> item =
        wire::common::Identifier::parse(map.id);
    const bool extendable = item.has_value() && itemTags != nullptr &&
                            realItemTags::contains(*itemTags, "minecraft:extendable_maps", *item);
    return cartographyResult(map, additional, {.scale = data->scale, .locked = data->locked, .extendable = extendable});
}

bool settleCraftedMaps(player::PlayerSession& session, std::optional<storage::StoredItemStack>* cursor, MapSystem& maps,
                       const LoadedWorld& world) {
    bool settledAny = false;
    if (cursor != nullptr && cursor->has_value() && (*cursor)->components.contains(kPostProcessing)) {
        maps.postProcessCrafted((*cursor)->components, world);
        settledAny = true;
    }
    for (std::int32_t slot = 0; slot < storage::StoredInventory::kPlayerSlots; ++slot) {
        const std::optional<storage::StoredItemStack>& stack = session.inventory.at(slot);
        if (stack.has_value() && stack->components.contains(kPostProcessing)) {
            storage::StoredItemStack settled = *stack;
            maps.postProcessCrafted(settled.components, world);
            session.inventory.set(slot, std::move(settled));
            settledAny = true;
        }
    }
    return settledAny;
}

} // namespace basalt::server::runtime::world::maps
