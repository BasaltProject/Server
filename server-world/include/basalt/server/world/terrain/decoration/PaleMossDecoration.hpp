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

void decorate(Chunk& chunk, random::RandomSource& random);

void placePatchAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
                  random::RandomSource& mossyCarpetRandom);

void placePatchAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

/// parity: `pale_moss_patch`'s own vegetation.
void placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

} // namespace paleMossDecoration

} // namespace basalt::server::world::terrain::decoration
