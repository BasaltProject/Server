#pragma once

#include <basalt/server/world/terrain/decoration/BiomeDecoration.hpp>

namespace basalt::server::world::terrain::decoration {

namespace jungleMelonDecoration {

[[nodiscard]] const BiomeVegetalDecoration& normal();
[[nodiscard]] const BiomeVegetalDecoration& sparse();

} // namespace jungleMelonDecoration

/// parity: the shared jungle `vines` placed and configured feature.
namespace jungleVineDecoration {

[[nodiscard]] const BiomeVegetalDecoration& decoration();

bool placeAt(Chunk& chunk, const DecorationPos& origin);

} // namespace jungleVineDecoration

} // namespace basalt::server::world::terrain::decoration
