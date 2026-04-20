#pragma once

#include <cstdint>

namespace basalt::server::runtime::world {

class LoadedWorld;

namespace precipitation {

/// parity: `Level.isRainingAt`.
[[nodiscard]] bool rainingAt(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `Entity.isInRain`, which asks at the feet and at the top of the box.
[[nodiscard]] bool entityInRain(LoadedWorld& world, double x, double y, double z, double height);

/// parity: the biome's `increased_fire_burnout` attribute there.
[[nodiscard]] bool increasedFireBurnout(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

} // namespace precipitation

} // namespace basalt::server::runtime::world
