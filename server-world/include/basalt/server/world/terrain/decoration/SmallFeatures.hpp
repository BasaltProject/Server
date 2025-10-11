#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>

#include <set>
#include <string>
#include <string_view>

namespace basalt::server::world::terrain::decoration {

/// The small vanilla features that are one block, one column or one pile: what is left of the
/// `/place feature` registry once the big decoration steps have their own bodies.
///
/// parity: `SimpleBlockFeature`, `BlockColumnFeature` and `BlockPileFeature`, each rolled at the
/// position it was given. The placement modifiers that decide *where* generation puts one are the
/// command's business, not theirs.
namespace smallFeatures {

/// Every vanilla placed feature name this file answers, and what it places.
[[nodiscard]] const std::set<std::string, std::less<>>& placedIds();

bool placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, std::string_view placedFeature);

} // namespace smallFeatures

} // namespace basalt::server::world::terrain::decoration
