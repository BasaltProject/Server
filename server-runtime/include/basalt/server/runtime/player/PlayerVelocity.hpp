#pragma once

#include <cstdint>

namespace basalt::server::runtime::player {

class PlayerSession;

/// parity: `Entity.deltaMovement` on a `ServerPlayer`.
namespace velocity {

/// parity: `aiStep` for a body it does not simulate.
void decay(PlayerSession& player);

[[nodiscard]] bool slidingDown(const PlayerSession& player, std::int32_t blockX, std::int32_t blockY,
                               std::int32_t blockZ);

void slide(PlayerSession& player);

} // namespace velocity

} // namespace basalt::server::runtime::player
