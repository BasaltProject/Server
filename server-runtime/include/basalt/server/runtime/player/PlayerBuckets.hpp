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

using BucketGate = std::function<bool(const PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z,
                                      const std::string& item, bool filling)>;

bool useBucketFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                  const wire::common::Identifier& item, std::int32_t heldSlot, const storage::StoredItemStack& held,
                  float yaw, float pitch, const BucketGate& allowed);

bool useBucketOnFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                    const wire::common::Identifier& item, std::int32_t heldSlot, const storage::StoredItemStack& held,
                    std::int32_t x, std::int32_t y, std::int32_t z, wire::protocol::play::Direction face,
                    const BucketGate& allowed);

} // namespace basalt::server::runtime::player
