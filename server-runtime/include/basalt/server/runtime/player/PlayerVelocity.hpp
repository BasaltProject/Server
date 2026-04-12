#pragma once

#include <cstdint>

namespace basalt::server::runtime::player {

class PlayerSession;

/// The motion the server itself keeps for a player, which only its own pushes set.
///
/// parity: `Entity.deltaMovement` on a `ServerPlayer`, which the client moves and the server never
/// simulates: knockback and impulses land in it, and `LivingEntity.aiStep` wears it away.
namespace velocity {

/// parity: `aiStep` for a body it does not simulate: 0.98 a tick, then the crumbs dropped.
void decay(PlayerSession& player);

/// parity: `HoneyBlock.isSlidingDown` against the square at `blockX, blockY, blockZ`.
[[nodiscard]] bool slidingDown(const PlayerSession& player, std::int32_t blockX, std::int32_t blockY,
                               std::int32_t blockZ);

/// parity: `HoneyBlock.doSlideMovement` on the server's motion.
void slide(PlayerSession& player);

} // namespace velocity

} // namespace basalt::server::runtime::player
