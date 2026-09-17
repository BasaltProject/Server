#pragma once

#include <basalt/server/runtime/player/PlayerBlockPlace.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::player {

struct BreakContext;
class PlayerSession;

/// Puts a minecart on the rail that was clicked, if it was a rail at all.
///
/// Shared because the cart sits the same way through both doors: the Java session runs it through
/// its own handler and the Bedrock one has no handler at all.
bool placeMinecartFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                      const wire::common::Identifier& item, std::int32_t heldSlot, const storage::StoredItemStack& held,
                      const BlockHit& hit);

/// Puts a boat down on the face that was clicked.
bool placeBoatFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                  const wire::common::Identifier& item, std::int32_t heldSlot, const storage::StoredItemStack& held,
                  const BlockHit& hit);

/// Sets an end crystal on the obsidian or bedrock that was clicked, if the two blocks above are
/// clear. parity: `EndCrystalItem.useOn`.
bool placeEndCrystalFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                        std::int32_t heldSlot, const storage::StoredItemStack& held, const BlockHit& hit);

/// Hangs an item frame or a painting on the face that was clicked, if it will hold one.
bool hangFrameFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                  const wire::common::Identifier& type, std::int32_t heldSlot, const storage::StoredItemStack& held,
                  const BlockHit& hit);

/// Lets a spawn egg out onto the block that was clicked.
bool useSpawnEggFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                    const wire::common::Identifier& egg, std::int32_t heldSlot, const storage::StoredItemStack& held,
                    const BlockHit& hit);

} // namespace basalt::server::runtime::player
