#pragma once

#include <basalt/server/world/Chunk.hpp>

#include <cstdint>
#include <functional>

namespace basalt::server::world::terrain::decoration::tree {

/// parity: vanilla's per-tree `updateLeaves` passes converge on the real distance from a leaf to a
/// log, 98 % of the time measured on the jar. Ours leave a stale 7 wherever a later tree covered a
/// leaf its own pass could not reach, so this settles the whole chunk once its trees are down.
namespace leafDistances {

/// The world around the chunk, so a leaf whose trunk stands next door is measured from it. Measured from
/// logs only, so the answer is the same whatever order the chunks arrive in.
using BlockAt = std::function<std::int32_t(std::int32_t, std::int32_t, std::int32_t)>;

void settle(Chunk& chunk, const BlockAt& blockAt);

} // namespace leafDistances

} // namespace basalt::server::world::terrain::decoration::tree
