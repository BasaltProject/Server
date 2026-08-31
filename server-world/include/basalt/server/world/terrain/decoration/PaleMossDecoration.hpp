#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>
#include <functional>

namespace basalt::server::world::terrain::decoration {

/// parity: `pale_moss_patch`, including its ground replacement and nested vegetation.
namespace paleMossDecoration {

using PlacementObserver = std::function<void(const DecorationPos&)>;

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
              PlacementObserver observer = nullptr);

/// The vegetal-decoration entry point, for the biome dispatcher that already holds the random.
void decorate(Chunk& chunk, random::RandomSource& random);

/// One patch at one origin. `mossyCarpetRandom` is the region random the carpet draws from, which is
/// not the feature's own.
void placePatchAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
                  random::RandomSource& mossyCarpetRandom);

void placePatchAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

/// parity: `pale_moss_patch`'s own vegetation, at one position, which is what `/place feature` runs.
void placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

} // namespace paleMossDecoration

} // namespace basalt::server::world::terrain::decoration
