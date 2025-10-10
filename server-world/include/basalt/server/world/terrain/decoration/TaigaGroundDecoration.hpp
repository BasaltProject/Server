#pragma once

#include <basalt/server/world/terrain/decoration/BiomeDecoration.hpp>

namespace basalt::server::world::terrain::decoration {

/// Taiga large ferns, the two taiga grass and fern ground covers, and the sweet berry patches.
namespace taigaGroundDecoration {

/// `patch_berry_rare`, appended after extra vegetation in snowy taiga.
[[nodiscard]] BiomeVegetalDecoration rareBerry();

/// `patch_berry_common`, appended after extra vegetation in the other three taigas.
[[nodiscard]] BiomeVegetalDecoration commonBerry();

/// `patch_large_fern`, placed before trees in all four taiga variants.
[[nodiscard]] BiomeVegetalDecoration largeFern();

/// `patch_grass_taiga_2`, one outer attempt in taiga and snowy taiga.
[[nodiscard]] BiomeVegetalDecoration normal();

/// Seven `patch_grass_taiga` attempts followed by `patch_dead_bush`.
[[nodiscard]] BiomeVegetalDecoration oldGrowth();

/// The old-growth `patch_dead_bush` tail, used after exact grass seeding.
[[nodiscard]] BiomeVegetalDecoration oldGrowthDeadBush();

} // namespace taigaGroundDecoration

} // namespace basalt::server::world::terrain::decoration
