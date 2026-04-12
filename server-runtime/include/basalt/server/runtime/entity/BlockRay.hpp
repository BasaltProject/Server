#pragma once

#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
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
/// parity: `BlockGetter.clip` over `AABB.clip`, walked block by block rather than sampled: a fast
/// arrow has to hit the first block it crosses in a tick, not the one it lands in.
namespace blockRay {

/// How many blocks one cast may cross before it gives up, which is more than a tick can move.
inline constexpr std::int32_t kMaxSteps = 256;

[[nodiscard]] std::optional<BlockHit> cast(world::WorldState& world, double fromX, double fromY, double fromZ,
                                           double toX, double toY, double toZ);

/// Whether the line meets a block the predicate matches, whatever its shape (a pane of wool stops a
/// vibration). parity: `BlockGetter.isBlockInLine` over a `ClipBlockStateContext`: the squares of
/// `clip`, asking a predicate instead of clipping boxes.
[[nodiscard]] bool crosses(world::WorldState& world, double fromX, double fromY, double fromZ, double toX, double toY,
                           double toZ, const std::function<bool(std::int32_t stateId)>& matches);

/// Which shape of each block the line is stopped by. parity: `ClipContext.Block`.
enum class ClipShape : std::uint8_t {
    /// parity: `COLLIDER`, the collision shape.
    Collider,
    /// parity: `VISUAL`, what the eye is stopped by: glass, panes, bars and powder snow let it through.
    Visual,
};

/// The entity the clip is asked for, which scaffolding and powder snow read their shape from.
/// parity: `EntityCollisionContext`.
struct ClipViewer {
    double feetY = 0.0;
    /// parity: `Entity.isDescending`, a player holding sneak.
    bool descending = false;
    double fallDistance = 0.0;
    /// parity: `PowderSnowBlock.canEntityWalkOnPowderSnow`.
    bool walksOnPowderSnow = false;
};

/// Whether the line meets a block at all, fluids ignored. parity: `BlockGetter.clip` with
/// `ClipContext.Fluid.NONE`: `traverseBlocks` with both ends nudged by 1.0E-7, each block tested with
/// `VoxelShape.clip`, which also counts a line that starts inside the shape.
[[nodiscard]] bool clips(world::WorldState& world, double fromX, double fromY, double fromZ, double toX, double toY,
                         double toZ, ClipShape shape, const ClipViewer& viewer);

} // namespace blockRay

} // namespace basalt::server::runtime::entity
