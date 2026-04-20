#pragma once

#include <basalt/server/runtime/player/PlayerBlockPlace.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <string_view>

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::player {

struct BreakContext;
class PlayerSession;

bool placeMinecartFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                      const wire::common::Identifier& item, std::int32_t heldSlot, const storage::StoredItemStack& held,
                      const BlockHit& hit);

bool placeBoatFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                  const wire::common::Identifier& item, std::int32_t heldSlot, const storage::StoredItemStack& held,
                  const BlockHit& hit);

bool placeEndCrystalFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                        std::int32_t heldSlot, const storage::StoredItemStack& held, const BlockHit& hit);

bool hangFrameFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                  const wire::common::Identifier& type, std::int32_t heldSlot, const storage::StoredItemStack& held,
                  const BlockHit& hit);

bool placeCushionFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                     std::string_view colour, std::int32_t heldSlot, const storage::StoredItemStack& held,
                     const BlockHit& hit);

bool useSpawnEggFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                    const wire::common::Identifier& egg, std::int32_t heldSlot, const storage::StoredItemStack& held,
                    const BlockHit& hit);

} // namespace basalt::server::runtime::player
