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

/// Runs each step against the same chunk and random, which is how a biome runs its vegetal features
/// one after another over a single shared stream.
[[nodiscard]] BiomeVegetalDecoration combine(std::vector<BiomeVegetalDecoration> steps);

/// Runs a placed feature's modifiers from the chunk corner, which is where a decoration step starts.
void runFromChunkCorner(Chunk& chunk, random::RandomSource& random, const std::vector<PlacementModifier>& modifiers,
                        const SimpleBlockFeature& feature);

/// A placed feature that scatters one simple block, the shape most vegetal decorations have.
[[nodiscard]] BiomeVegetalDecoration placedVegetation(std::vector<PlacementModifier> modifiers,
                                                      SimpleBlockFeature feature);

/// Retains the biome-to-decoration table used by tests and `/place`. Every implemented step-9
/// surface feature has its own independently seeded dispatcher, so production no longer consults
/// one centre biome. An unknown biome resolves to nothing rather than borrowing another's features.
namespace biomeDecoration {

[[nodiscard]] std::optional<BiomeVegetalDecoration> forBiome(const wire::common::Identifier& id);

} // namespace biomeDecoration

} // namespace basalt::server::world::terrain::decoration
