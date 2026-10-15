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

struct CartographyFacts {
    std::int8_t scale = 0;
    bool locked = false;
    bool extendable = false;
};

/// parity: `CartographyTableMenu.setupResultSlot`.
[[nodiscard]] std::optional<storage::StoredItemStack> cartographyResult(const storage::StoredItemStack& map,
                                                                        const storage::StoredItemStack& additional,
                                                                        const CartographyFacts& facts);

[[nodiscard]] std::optional<storage::StoredItemStack> cartographyResult(const storage::StoredItemStack& map,
                                                                        const storage::StoredItemStack& additional,
                                                                        MapSystem& maps,
                                                                        const realItemTags::TagMembers* itemTags);

/// parity: `MapItem.onCraftedPostProcess`.
bool settleCraftedMaps(player::PlayerSession& session, std::optional<storage::StoredItemStack>* cursor, MapSystem& maps,
                       const LoadedWorld& world);

} // namespace basalt::server::runtime::world::maps
