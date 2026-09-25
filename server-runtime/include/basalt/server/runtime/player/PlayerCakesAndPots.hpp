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

/// parity: `CakeBlock.useItemOn`/`useWithoutItem` and `CandleCakeBlock`'s: a candle on an untouched
/// cake, a slice for anyone hungry or in creative, and the candle given back when a candle cake is
/// eaten. `clickY` is where on the block the click landed, which is how a candle is told from the
/// cake under it. Answers whether the click was spent on the cake.
bool useCakeFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
                std::int32_t y, std::int32_t z, std::int32_t heldSlot,
                const std::optional<storage::StoredItemStack>& held, float clickY);

/// parity: `FlowerPotBlock.useItemOn` and `useWithoutItem`: a plant goes into an empty pot, and
/// comes back out of a full one. Answers whether the click was spent on the pot.
bool useFlowerPotFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
                     std::int32_t y, std::int32_t z, std::int32_t heldSlot,
                     const std::optional<storage::StoredItemStack>& held);

} // namespace basalt::server::runtime::player
