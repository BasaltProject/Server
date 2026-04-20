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

/// parity: `PathNavigation`.
namespace navigation {

/// parity: `PathNavigation.moveTo`: works out a path and starts walking it.
bool moveTo(world::WorldState& world, ServerEntity& mob, double x, double y, double z, double speedModifier,
            std::int32_t reachRange = 1);

/// parity: `PathNavigation.createPath` before a goal starts.
[[nodiscard]] bool canReach(world::WorldState& world, ServerEntity& mob, double x, double y, double z);

[[nodiscard]] bool isDone(const ServerEntity& mob);

/// parity: `PathNavigation.moveTo(Path, speed)`, a path already worked out.
bool follow(ServerEntity& mob, std::vector<std::array<std::int32_t, 3>> nodes, const std::array<double, 3>& target,
            double speedModifier);

void stop(ServerEntity& mob);

/// parity: `PathNavigation.tick` over `followThePath`.
void tick(world::WorldState& world, ServerEntity& mob);

/// parity: `PathNavigation.doStuckDetection`.
void detectStuck(ServerEntity& mob, const std::array<double, 3>& position, std::int64_t time);

} // namespace navigation

/// parity: `MoveControl.tick`'s `MOVE_TO`.
namespace moveControl {

void setWanted(ServerEntity& mob, double x, double y, double z, double speedModifier);

/// parity: `CubeMobMoveControl`.
void setForward(ServerEntity& mob, double speedModifier);

/// parity: `MoveControl.strafe`.
void strafe(ServerEntity& mob, double strafe, double forward, double speedModifier);

void stop(ServerEntity& mob);

void tick(world::WorldState& world, ServerEntity& mob, double speed);

/// parity: `SmoothSwimmingMoveControl.tick` with the travel it drives.
void tickSwimming(world::WorldState& world, ServerEntity& mob, double speed);

/// parity: `MoveControl.rotlerp`, the shortest way round capped at `max` degrees.
[[nodiscard]] float rotlerp(float from, float to, float max);

[[nodiscard]] float wrapDegrees(float degrees);

} // namespace moveControl

/// parity: `LookControl`.
namespace lookControl {

void lookAt(ServerEntity& mob, double x, double y, double z, float maxYaw = 10.0F, float maxPitch = 40.0F);

void tick(ServerEntity& mob);

/// parity: `SmoothSwimmingLookControl.tick`.
void tickSwimming(ServerEntity& mob);

} // namespace lookControl

[[nodiscard]] bool swimsSmoothly(const ServerEntity& mob);

/// parity: `Entity.isInWater`, which is the block its feet are in.
[[nodiscard]] bool inWater(world::WorldState& world, const ServerEntity& mob);

[[nodiscard]] bool isPassable(world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

} // namespace basalt::server::runtime::entity::ai
