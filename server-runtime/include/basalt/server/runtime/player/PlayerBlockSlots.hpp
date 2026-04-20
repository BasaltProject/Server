#pragma once

#include <basalt/server/runtime/world/RealItemTags.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::player {

struct BreakContext;
class PlayerSession;

struct FaceHit {
    wire::protocol::play::Direction face = wire::protocol::play::Direction::Up;
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

void useBookshelfFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
                     std::int32_t y, std::int32_t z, const FaceHit& hit, std::int32_t heldSlot,
                     const std::optional<storage::StoredItemStack>& held,
                     const world::realItemTags::TagMembers* itemTags);

void useShelfFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
                 std::int32_t y, std::int32_t z, const FaceHit& hit, std::int32_t heldSlot,
                 const std::optional<storage::StoredItemStack>& held);

void usePotFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
               std::int32_t y, std::int32_t z, std::int32_t heldSlot,
               const std::optional<storage::StoredItemStack>& held);

bool useJukeboxFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
                   std::int32_t y, std::int32_t z, std::int32_t heldSlot,
                   const std::optional<storage::StoredItemStack>& held);

bool placeBookOnLecternFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                           std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t heldSlot,
                           const std::optional<storage::StoredItemStack>& held);

} // namespace basalt::server::runtime::player
