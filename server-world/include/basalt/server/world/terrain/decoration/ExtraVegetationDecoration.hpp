#pragma once

#include <basalt/server/world/terrain/decoration/BiomeDecoration.hpp>

namespace basalt::server::world::terrain::decoration {

namespace extraVegetationDecoration {

[[nodiscard]] const BiomeVegetalDecoration& pumpkinOnly();

[[nodiscard]] const BiomeVegetalDecoration& standard();
[[nodiscard]] const BiomeVegetalDecoration& standardWithoutPumpkin();

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
