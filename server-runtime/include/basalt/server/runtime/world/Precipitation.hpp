#pragma once

#include <cstdint>

namespace basalt::server::runtime::world {

class LoadedWorld;

/// Where rain actually falls.
namespace precipitation {

/// parity: `Level.isRainingAt`: raining, open sky, nothing above in the motion-blocking heightmap,
/// and a biome whose precipitation there is rain rather than snow or none.
[[nodiscard]] bool rainingAt(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `Entity.isInRain`, which asks at the feet and at the top of the box.
[[nodiscard]] bool entityInRain(LoadedWorld& world, double x, double y, double z, double height);

/// parity: the biome's `increased_fire_burnout` attribute there: the wet biomes that eat fire faster.
[[nodiscard]] bool increasedFireBurnout(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

} // namespace precipitation

} // namespace basalt::server::runtime::world
