#pragma once

#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::runtime::entity {

struct BlockHit {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    std::int32_t blockX = 0;
    std::int32_t blockY = 0;
    std::int32_t blockZ = 0;
    registry::BlockFace face = registry::BlockFace::Up;
};

/// parity: `BlockGetter.clip`.
namespace blockRay {

inline constexpr std::int32_t kMaxSteps = 256;

[[nodiscard]] std::optional<BlockHit> cast(world::WorldState& world, double fromX, double fromY, double fromZ,
                                           double toX, double toY, double toZ);

/// parity: `BlockGetter.isBlockInLine`.
[[nodiscard]] bool crosses(world::WorldState& world, double fromX, double fromY, double fromZ, double toX, double toY,
                           double toZ, const std::function<bool(std::int32_t stateId)>& matches);

/// parity: `ClipContext.Block`.
enum class ClipShape : std::uint8_t {
    /// parity: `COLLIDER`, the collision shape.
    Collider,
    /// parity: `VISUAL`.
    Visual,
};

/// parity: `EntityCollisionContext`.
struct ClipViewer {
    double feetY = 0.0;
    /// parity: `Entity.isDescending`, a player holding sneak.
    bool descending = false;
    double fallDistance = 0.0;
    /// parity: `PowderSnowBlock.canEntityWalkOnPowderSnow`.
    bool walksOnPowderSnow = false;
};

/// parity: `BlockGetter.clip`.
[[nodiscard]] bool clips(world::WorldState& world, double fromX, double fromY, double fromZ, double toX, double toY,
                         double toZ, ClipShape shape, const ClipViewer& viewer);

} // namespace blockRay

} // namespace basalt::server::runtime::entity
