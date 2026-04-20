#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>

#include <set>
#include <string>
#include <string_view>

namespace basalt::server::world::terrain::decoration {

/// parity: `SimpleBlockFeature`.
namespace smallFeatures {

[[nodiscard]] const std::set<std::string, std::less<>>& placedIds();

bool placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, std::string_view placedFeature);

} // namespace smallFeatures

} // namespace basalt::server::world::terrain::decoration
