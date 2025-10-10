#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/BiomeDecoration.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>

#include <set>
#include <string>
#include <string_view>

namespace basalt::server::world::terrain::decoration {

/// The ocean floor vegetation the seven non-frozen ocean variants share. Kelp age, sea pickle count
/// and both tall seagrass halves keep their exact block states.
namespace aquaticDecoration {

[[nodiscard]] BiomeVegetalDecoration ocean();
[[nodiscard]] BiomeVegetalDecoration deepOcean();
[[nodiscard]] BiomeVegetalDecoration coldOcean();
[[nodiscard]] BiomeVegetalDecoration deepColdOcean();
[[nodiscard]] BiomeVegetalDecoration lukewarmOcean();
[[nodiscard]] BiomeVegetalDecoration deepLukewarmOcean();
[[nodiscard]] BiomeVegetalDecoration river();
[[nodiscard]] BiomeVegetalDecoration swamp();
[[nodiscard]] BiomeVegetalDecoration warmOcean();

/// The vanilla placed feature names this step draws.
[[nodiscard]] const std::set<std::string, std::less<>>& placedIds();

/// One of them at one position, which is what `/place feature` runs.
bool placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, std::string_view placedFeature);

} // namespace aquaticDecoration

} // namespace basalt::server::world::terrain::decoration
