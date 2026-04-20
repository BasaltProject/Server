#pragma once

#include <basalt/server/world/terrain/decoration/BiomeDecoration.hpp>

namespace basalt::server::world::terrain::decoration {

namespace taigaGroundDecoration {

[[nodiscard]] BiomeVegetalDecoration rareBerry();

[[nodiscard]] BiomeVegetalDecoration commonBerry();

[[nodiscard]] BiomeVegetalDecoration largeFern();

[[nodiscard]] BiomeVegetalDecoration normal();

[[nodiscard]] BiomeVegetalDecoration oldGrowth();

[[nodiscard]] BiomeVegetalDecoration oldGrowthDeadBush();

} // namespace taigaGroundDecoration

} // namespace basalt::server::world::terrain::decoration
