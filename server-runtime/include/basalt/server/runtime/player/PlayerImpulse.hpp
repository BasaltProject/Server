#pragma once

#include <cstdint>

namespace basalt::server::runtime::player {

class PlayerSession;

/// The impulse a player is riding, which spares the fall it causes.
///
/// parity: `LivingEntity.currentImpulseImpactPos` and `currentImpulseContextResetGraceTime`: a wind
/// charge's gust spares the fall below where it caught them, for as long as they have not landed.
namespace impulse {

/// parity: `setIgnoreFallDamageFromCurrentImpulse(true, ...)`'s forty ticks of grace.
inline constexpr std::int32_t kGraceTicks = 40;

/// parity: `ServerPlayer.onExplosionHit`: a player's own wind charge starts one, anything else only
/// ends the grace.
void onExplosionHit(PlayerSession& player, bool windCharge);

/// parity: `LivingEntity.tick`, the grace running out.
void tick(PlayerSession& player);

/// parity: `tryResetCurrentImpulseContext`, which waits out the grace.
void tryReset(PlayerSession& player);

/// parity: `resetCurrentImpulseContext`.
void reset(PlayerSession& player);

/// parity: `LivingEntity.causeFallDamage`'s first half: the fall counted only down to where the
/// impulse caught them, and the impulse let go of once they land at or above that.
[[nodiscard]] double effectiveFallDistance(PlayerSession& player, double fallDistance, double landingY);

} // namespace impulse

} // namespace basalt::server::runtime::player
