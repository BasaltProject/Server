#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/OverworldAquifer.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRules.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::carver {

/// parity: `NoiseBasedChunkGenerator.applyCarvers` plus the builtin `cave`,
/// `cave_extra_underground` and `canyon` configured carvers every 26.2 Overworld biome uses.
///
/// Applies all three from every source chunk in vanilla's 8-chunk neighbourhood.
void carve(Chunk& chunk, std::int64_t seed, OverworldAquifer& aquifer,
           const surface::SurfaceRuleEnvironment& environment, const surface::RuleSource& surfaceRule);

} // namespace basalt::server::world::terrain::carver
