#pragma once

#include <basalt/server/world/Chunk.hpp>

#include <cstdint>
#include <functional>

namespace basalt::server::world::terrain::decoration::tree {

/// parity: vanilla's per-tree `updateLeaves` passes leave a stale 7 where a later tree covered a leaf; this settles it.
namespace leafDistances {

/// The world around the chunk, so a leaf is measured from a trunk next door, from logs only so order never matters.
using BlockAt = std::function<std::int32_t(std::int32_t, std::int32_t, std::int32_t)>;

void settle(Chunk& chunk, const BlockAt& blockAt);

} // namespace leafDistances

} // namespace basalt::server::world::terrain::decoration::tree
