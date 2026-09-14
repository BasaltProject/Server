#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::player {

struct BreakContext;
class PlayerSession;

/// Whether a plugin lets this bucket be filled or emptied here. Unset means nobody objects.
using BucketGate = std::function<bool(const PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z,
                                      const std::string& item, bool filling)>;

/// Uses a bucket on what the player is looking at. Answers whether the item in hand was a bucket.
/// A Java client sends a use and the server traces the look; a Bedrock one names block and face.
bool useBucketFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                  const wire::common::Identifier& item, std::int32_t heldSlot, const storage::StoredItemStack& held,
                  float yaw, float pitch, const BucketGate& allowed);

/// The same, on the block and face the client named.
bool useBucketOnFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                    const wire::common::Identifier& item, std::int32_t heldSlot, const storage::StoredItemStack& held,
                    std::int32_t x, std::int32_t y, std::int32_t z, wire::protocol::play::Direction face,
                    const BucketGate& allowed);

} // namespace basalt::server::runtime::player
