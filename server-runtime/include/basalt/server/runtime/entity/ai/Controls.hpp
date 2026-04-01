#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::world {
class WorldState;
}

namespace basalt::server::runtime::entity::ai {

/// The path a mob is walking and how far along it is.
///
/// parity: `PathNavigation`: the path is a list of nodes, the mob steps to the next one when it is
/// close enough to the current one, and what it hands the move control is that node's middle.
namespace navigation {

/// parity: `PathNavigation.moveTo`: works out a path and starts walking it.
bool moveTo(world::WorldState& world, ServerEntity& mob, double x, double y, double z, double speedModifier);

/// Whether a path to there exists at all. parity: `PathNavigation.createPath` before a goal starts.
[[nodiscard]] bool canReach(world::WorldState& world, ServerEntity& mob, double x, double y, double z);

[[nodiscard]] bool isDone(const ServerEntity& mob);

/// parity: `PathNavigation.moveTo(Path, speed)`, a path already worked out. Answers false for an
/// empty one, which leaves the mob standing.
bool follow(ServerEntity& mob, std::vector<std::array<std::int32_t, 3>> nodes, const std::array<double, 3>& target,
            double speedModifier);

void stop(ServerEntity& mob);

/// parity: `PathNavigation.tick` over `followThePath`.
void tick(world::WorldState& world, ServerEntity& mob);

} // namespace navigation

/// The turn and the step a mob takes towards where it wants to be.
///
/// parity: `MoveControl.tick`'s `MOVE_TO` branch: turn at most ninety degrees a tick, walk at the
/// goal's own share of the movement speed, and jump at whatever the step cannot climb.
namespace moveControl {

void setWanted(ServerEntity& mob, double x, double y, double z, double speedModifier);

/// parity: `CubeMobMoveControl`, which only ever sets a speed: the way it faces is the way it goes.
void setForward(ServerEntity& mob, double speedModifier);

void stop(ServerEntity& mob);

void tick(world::WorldState& world, ServerEntity& mob, double speed);

/// parity: `SmoothSwimmingMoveControl.tick` with the travel it drives.
void tickSwimming(world::WorldState& world, ServerEntity& mob, double speed);

/// parity: `MoveControl.rotlerp`, the shortest way round capped at `max` degrees.
[[nodiscard]] float rotlerp(float from, float to, float max);

[[nodiscard]] float wrapDegrees(float degrees);

} // namespace moveControl

/// Where a mob is looking.
///
/// parity: `LookControl`: the head turns towards whatever a goal pointed it at, at most so many
/// degrees a tick, and drifts back to the body when nothing is asking.
namespace lookControl {

void lookAt(ServerEntity& mob, double x, double y, double z, float maxYaw = 10.0F, float maxPitch = 40.0F);

void tick(ServerEntity& mob);

/// parity: `SmoothSwimmingLookControl.tick`.
void tickSwimming(ServerEntity& mob);

} // namespace lookControl

/// Whether it swims with `SmoothSwimmingMoveControl` along a `WaterBoundPathNavigation`.
[[nodiscard]] bool swimsSmoothly(const ServerEntity& mob);

/// parity: `Entity.isInWater`, which is the block its feet are in.
[[nodiscard]] bool inWater(world::WorldState& world, const ServerEntity& mob);

/// Whether the block at these coordinates is one a mob can stand in.
[[nodiscard]] bool isPassable(world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

} // namespace basalt::server::runtime::entity::ai
