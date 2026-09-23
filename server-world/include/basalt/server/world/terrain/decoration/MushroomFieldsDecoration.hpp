#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/BiomeDecoration.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::decoration {

/// Mushroom Fields `vegetal_decoration`, in `addMushroomFieldVegetation` order:
/// `mushroom_island_vegetation`, one position per chunk selecting between the huge red and brown
/// mushrooms, then the two taiga mushroom patches.
namespace mushroomFieldsDecoration {

inline constexpr std::int32_t kMushroomIslandVegetationIndex = 82;

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment);

/// The biome's own vegetal entry, huge mushrooms followed by the taiga patches.
[[nodiscard]] BiomeVegetalDecoration vegetalDecoration();

/// `mushroom_island_vegetation` without the independently seeded small mushrooms.
[[nodiscard]] BiomeVegetalDecoration hugeOnly();

/// parity: `mushroom_island_vegetation`, one huge mushroom at one position.
void placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

} // namespace mushroomFieldsDecoration

} // namespace basalt::server::world::terrain::decoration
