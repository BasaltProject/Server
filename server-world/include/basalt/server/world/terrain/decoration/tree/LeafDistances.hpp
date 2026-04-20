#pragma once

#include <basalt/server/world/Chunk.hpp>

#include <cstdint>
#include <functional>

namespace basalt::server::world::terrain::decoration::tree {

/// parity: vanilla's per-tree `updateLeaves` passes converge on the real distance from a leaf to a.
namespace leafDistances {

using BlockAt = std::function<std::int32_t(std::int32_t, std::int32_t, std::int32_t)>;

void settle(Chunk& chunk, const BlockAt& blockAt);

} // namespace leafDistances

} // namespace basalt::server::world::terrain::decoration::tree
