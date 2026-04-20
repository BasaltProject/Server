#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/Explosions.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `WindCharge.explode`.
namespace windBursts {

inline constexpr float kRadius = 1.2F;
inline constexpr float kBreezeRadius = 3.0F;

/// parity: `AbstractWindCharge.onHitBlock`.
inline constexpr double kBackOff = 0.25;

/// `stops` is the `blocks_wind_charge_explosions` tag, the only thing that shortens the gust.
[[nodiscard]] std::vector<BlockChange> burst(WorldState& world, std::int32_t minY, std::int32_t maxY, double x,
                                             double y, double z, std::int64_t gameTime, random::RandomSource& random,
                                             const explosions::Spares& stops = {}, float radius = kRadius);

} // namespace windBursts

} // namespace basalt::server::runtime::world
