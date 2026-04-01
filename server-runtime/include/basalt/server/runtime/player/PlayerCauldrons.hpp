#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::player {

struct BreakContext;
class PlayerSession;

/// parity: `AbstractCauldronBlock.useItemOn` over `CauldronInteractions`: buckets and bottles in and
/// out, and dye, banner patterns and shulker colours washed off. Answers whether the cauldron took
/// the click; one that did not leaves the item to be used as usual.
bool useCauldronFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
                    std::int32_t y, std::int32_t z, std::int32_t heldSlot,
                    const std::optional<storage::StoredItemStack>& held);

} // namespace basalt::server::runtime::player
