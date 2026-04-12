#pragma once

#include <basalt/server/runtime/world/RealItemTags.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime {
class MapSystem;

namespace player {
class PlayerSession;
}

namespace world {
class LoadedWorld;
}
} // namespace basalt::server::runtime

namespace basalt::server::runtime::world::maps {

/// What a map in a cartography table is, as far as the table cares.
struct CartographyFacts {
    std::int8_t scale = 0;
    bool locked = false;
    /// In `#extendable_maps`.
    bool extendable = false;
};

/// parity: `CartographyTableMenu.setupResultSlot`: paper widens, a glass pane locks, an empty map
/// clones. Nothing when the pair makes nothing. The post-processing happens when it is taken.
[[nodiscard]] std::optional<storage::StoredItemStack> cartographyResult(const storage::StoredItemStack& map,
                                                                        const storage::StoredItemStack& additional,
                                                                        const CartographyFacts& facts);

/// The same, asking the map's own data and the item tags for the facts. Nothing for an unknown map.
[[nodiscard]] std::optional<storage::StoredItemStack> cartographyResult(const storage::StoredItemStack& map,
                                                                        const storage::StoredItemStack& additional,
                                                                        MapSystem& maps,
                                                                        const realItemTags::TagMembers* itemTags);

/// parity: `MapItem.onCraftedPostProcess` over whatever just landed in the bag or on the cursor:
/// the widened or locked copy becomes a map of its own. Answers whether anything did.
bool settleCraftedMaps(player::PlayerSession& session, std::optional<storage::StoredItemStack>* cursor, MapSystem& maps,
                       const LoadedWorld& world);

} // namespace basalt::server::runtime::world::maps
