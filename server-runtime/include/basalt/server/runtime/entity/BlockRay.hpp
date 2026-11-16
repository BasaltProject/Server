#pragma once

#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
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

/// Builds the per-state tables now so the first ray does not pay for them.
void warmTables();

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

/// parity: `BlockGetter.clip`, the first block the line enters whose shape it touches.
[[nodiscard]] std::optional<std::array<std::int32_t, 3>> firstClip(world::WorldState::CachedBlockReader& reader,
                                                                   double fromX, double fromY, double fromZ, double toX,
                                                                   double toY, double toZ, ClipShape shape,
                                                                   const ClipViewer& viewer);

/// The same, reading through a reader the caller holds across several rays.
[[nodiscard]] bool clips(world::WorldState::CachedBlockReader& reader, double fromX, double fromY, double fromZ,
                         double toX, double toY, double toZ, ClipShape shape, const ClipViewer& viewer);

} // namespace blockRay

} // namespace basalt::server::runtime::entity
