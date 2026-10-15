#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>
#include <basalt/server/world/terrain/decoration/SimpleBlockFeature.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <functional>
#include <optional>
#include <vector>

namespace basalt::server::world::terrain::decoration {

/// parity: the `VEGETAL_DECORATION`-and-later portion of a biome's own `BiomeGenerationSettings`.
using BiomeVegetalDecoration = std::function<void(Chunk&, random::RandomSource&)>;

[[nodiscard]] BiomeVegetalDecoration combine(std::vector<BiomeVegetalDecoration> steps);

void runFromChunkCorner(Chunk& chunk, random::RandomSource& random, const std::vector<PlacementModifier>& modifiers,
                        const SimpleBlockFeature& feature);

[[nodiscard]] BiomeVegetalDecoration placedVegetation(std::vector<PlacementModifier> modifiers,
                                                      SimpleBlockFeature feature);

namespace biomeDecoration {

[[nodiscard]] std::optional<BiomeVegetalDecoration> forBiome(const wire::common::Identifier& id);

} // namespace biomeDecoration

} // namespace basalt::server::world::terrain::decoration
