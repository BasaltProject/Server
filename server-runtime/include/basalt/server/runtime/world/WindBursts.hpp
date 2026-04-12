#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/Explosions.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {

/// The gust a wind charge leaves where it lands.
///
/// parity: `WindCharge.explode`, an explosion that breaks nothing and damages nobody: its whole job
/// is `Explosion.canTriggerBlocks`, the flag every `onExplosionHit` below reads to press a button,
/// pull a lever, open a door or blow a candle out.
namespace windBursts {

/// parity: `WindCharge.RADIUS` and `BreezeWindCharge.RADIUS`.
inline constexpr float kRadius = 1.2F;
inline constexpr float kBreezeRadius = 3.0F;

/// parity: `AbstractWindCharge.onHitBlock`, which goes off a quarter of a block back along the face
/// it came in through rather than where it stopped.
inline constexpr double kBackOff = 0.25;

/// Knocks on everything the gust reaches. The blocks are already written; what the caller has left
/// to do is send them out and let their neighbours hear.
///
/// `stops` is the `blocks_wind_charge_explosions` tag, the only thing that shortens the gust.
[[nodiscard]] std::vector<BlockChange> burst(WorldState& world, std::int32_t minY, std::int32_t maxY, double x,
                                             double y, double z, std::int64_t gameTime, random::RandomSource& random,
                                             const explosions::Spares& stops = {}, float radius = kRadius);

} // namespace windBursts

} // namespace basalt::server::runtime::world
