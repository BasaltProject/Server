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

/// Where on a block's face the click landed, which is what decides which of its slots was meant.
struct FaceHit {
    wire::protocol::play::Direction face = wire::protocol::play::Direction::Up;
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

/// One book into a chiselled bookshelf, or the one already in that square back out.
///
/// Shared because both doors put the same books away: the Java session runs it through its own
/// handler and the Bedrock one has no handler at all.
void useBookshelfFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
                     std::int32_t y, std::int32_t z, const FaceHit& hit, std::int32_t heldSlot,
                     const std::optional<storage::StoredItemStack>& held,
                     const world::realItemTags::TagMembers* itemTags);

/// One item into a decorated pot. A hand with nothing it wants only makes the pot rock.
void usePotFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
               std::int32_t y, std::int32_t z, std::int32_t heldSlot,
               const std::optional<storage::StoredItemStack>& held);

} // namespace basalt::server::runtime::player
