#pragma once

#include <basalt/server/world/Chunk.hpp>

#include <cstdint>

namespace basalt::server::world {

/// Writes into every leaf how far it stands from a trunk, once, when the chunk arrives.
///
/// parity: `TreeFeature.updateLeaves`, which vanilla runs when it grows a tree: a breadth-first walk
/// out of the logs that stamps the `distance` property on each leaf it reaches. Everything after
/// that only ever reads the stored value, which is why a random tick on leaves costs six reads in
/// vanilla and not a search of the canopy.
///
/// Trees are placed inside a single chunk, so a chunk-wide pass gives the same answer the per-tree
/// one does, and it also fixes a chunk saved before this existed.
namespace leafDistances {

/// Stamps the distance on the leaves of `chunk`, and answers how many it had to correct.
std::int32_t assign(Chunk& chunk);

} // namespace leafDistances

} // namespace basalt::server::world
