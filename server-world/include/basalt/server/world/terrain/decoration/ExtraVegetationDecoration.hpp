#pragma once

#include <basalt/server/world/terrain/decoration/BiomeDecoration.hpp>

namespace basalt::server::world::terrain::decoration {

/// The shared pumpkin, sugar cane, cactus and firefly bush placed features.
///
/// Several biomes appear twice here: once whole, and once as the tail that is left after a feature
/// moved to its own independent seed in the global step. Both shapes are real placements.
namespace extraVegetationDecoration {

[[nodiscard]] const BiomeVegetalDecoration& pumpkinOnly();

[[nodiscard]] const BiomeVegetalDecoration& standard();
[[nodiscard]] const BiomeVegetalDecoration& standardWithoutPumpkin();

/// The near-water firefly tail, once pumpkin and sugar cane have their own seeds.
[[nodiscard]] const BiomeVegetalDecoration& fireflyNearWater();

[[nodiscard]] const BiomeVegetalDecoration& nearWater();

[[nodiscard]] const BiomeVegetalDecoration& desert();
[[nodiscard]] const BiomeVegetalDecoration& desertWithoutPumpkin();
[[nodiscard]] const BiomeVegetalDecoration& desertWithoutPumpkinOrCactus();

[[nodiscard]] const BiomeVegetalDecoration& badlands();
[[nodiscard]] const BiomeVegetalDecoration& badlandsWithoutPumpkin();
[[nodiscard]] const BiomeVegetalDecoration& badlandsWithoutPumpkinOrCactus();

[[nodiscard]] const BiomeVegetalDecoration& swamp();
[[nodiscard]] const BiomeVegetalDecoration& swampWithoutPumpkin();
[[nodiscard]] const BiomeVegetalDecoration& swampFireflies();

[[nodiscard]] const BiomeVegetalDecoration& mangrove();

} // namespace extraVegetationDecoration

} // namespace basalt::server::world::terrain::decoration
