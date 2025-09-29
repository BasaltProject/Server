#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>

#include <set>
#include <string>
#include <string_view>

namespace basalt::server::world::terrain::decoration {

/// The features that are a shape rather than a scattered patch: springs, the dungeon, the End's own
/// platforms, lakes and the lush cave floors, each at the position it was handed.
///
/// Generation reaches them through its own steps; this is the door `/place feature` comes in by. A
/// multi-block body writes the intersection this chunk can own, and block entities stay beside those
/// local blocks.
namespace standaloneFeatures {

[[nodiscard]] const std::set<std::string, std::less<>>& placedIds();

bool placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, std::string_view placedFeature);

} // namespace standaloneFeatures

} // namespace basalt::server::world::terrain::decoration
