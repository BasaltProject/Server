#pragma once

#include <cstdint>

namespace basalt::server::runtime::player {

class PlayerSession;

namespace impulse {

inline constexpr std::int32_t kGraceTicks = 40;

/// parity: `ServerPlayer.onExplosionHit`.
void onExplosionHit(PlayerSession& player, bool windCharge);

void tick(PlayerSession& player);

void tryReset(PlayerSession& player);

void reset(PlayerSession& player);

/// parity: `LivingEntity.causeFallDamage`'s first half.
[[nodiscard]] double effectiveFallDistance(PlayerSession& player, double fallDistance, double landingY);

} // namespace impulse

} // namespace basalt::server::runtime::player
