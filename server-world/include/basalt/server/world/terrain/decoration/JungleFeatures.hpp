#pragma once

#include <basalt/server/world/terrain/decoration/BiomeDecoration.hpp>

namespace basalt::server::world::terrain::decoration {

/// The jungle `melon` simple-block feature, at its normal and sparse rarities.
namespace jungleMelonDecoration {

[[nodiscard]] const BiomeVegetalDecoration& normal();
[[nodiscard]] const BiomeVegetalDecoration& sparse();

} // namespace jungleMelonDecoration

/// parity: the shared jungle `vines` placed and configured feature.
///
/// Distinct from `vinePatchDecoration`, which is the step-9 dispatcher: this one is the body a biome
/// runs directly, over a different height range and with no biome filter of its own.
namespace jungleVineDecoration {

[[nodiscard]] const BiomeVegetalDecoration& decoration();

/// One vine grown from a position rather than scanned over a chunk.
bool placeAt(Chunk& chunk, const DecorationPos& origin);

} // namespace jungleVineDecoration

} // namespace basalt::server::world::terrain::decoration
