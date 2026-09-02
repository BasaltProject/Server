#pragma once

#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::entity {

/// Where a ray met a block: the point, the block and the face it came in through.
struct BlockHit {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    std::int32_t blockX = 0;
    std::int32_t blockY = 0;
    std::int32_t blockZ = 0;
    registry::BlockFace face = registry::BlockFace::Up;
};

/// A straight line through the blocks, stopping at the first collision shape it meets.
///
/// parity: `BlockGetter.clip` over `AABB.clip`. The walk is the same one vanilla does, block by block
/// along the line rather than sampling it: a fast arrow crosses several blocks in a tick and has to
/// hit the first of them, not the one it happens to land in.
namespace blockRay {

/// How many blocks one cast may cross before it gives up, which is more than a tick can move.
inline constexpr std::int32_t kMaxSteps = 256;

[[nodiscard]] std::optional<BlockHit> cast(world::WorldState& world, double fromX, double fromY, double fromZ,
                                           double toX, double toY, double toZ);

} // namespace blockRay

} // namespace basalt::server::runtime::entity
