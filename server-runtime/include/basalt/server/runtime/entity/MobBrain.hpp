#pragma once

#include <basalt/server/random/RandomSource.hpp>

#include <array>
#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {
class WorldState;
}

namespace basalt::server::runtime::entity {

class ServerEntity;

/// What a mob does when nothing else is happening: wander, and look where it is going.
///
/// parity: `RandomStrollGoal` over `DefaultRandomPos.getPos`: one roll in 120 a tick, ten candidate
/// positions within ten blocks across and seven up, and the best one is walked to. Walking is
/// `MoveControl`: turn towards the target, walk forward, jump at what is in the way.
namespace mobBrain {

/// parity: `RandomStrollGoal`'s own 120 ticks between attempts.
inline constexpr std::int32_t kStrollInterval = 120;

/// parity: `WaterAvoidingRandomStrollGoal`'s ten across and seven up.
inline constexpr std::int32_t kStrollRange = 10;
inline constexpr std::int32_t kStrollHeight = 7;

/// One tick of wandering. `speed` is `Attributes.MOVEMENT_SPEED`, and `modifier` the goal's own
/// multiplier on it.
void tick(world::WorldState& world, ServerEntity& mob, random::RandomSource& random, double speed,
          double modifier = 1.0);

/// Walks whatever path the mob has, a node at a time. Answers false once there is nowhere left to go.
///
/// parity: `PathNavigation.tick`, which follows the path rather than pushing straight at the target:
/// that is what takes a mob round a wall instead of into it.
bool walk(world::WorldState& world, ServerEntity& mob, double speed, double modifier = 1.0);

/// Works out the way to a point and starts walking it. parity: `PathNavigation.moveTo`.
bool pathTo(world::WorldState& world, ServerEntity& mob, double x, double y, double z);

/// parity: `DefaultRandomPos.getPos`: ten tries at a spot within range, and the one standing on the
/// most solid ground wins. A spot with nothing under it is never chosen.
[[nodiscard]] std::optional<std::array<double, 3>> strollTarget(world::WorldState& world, const ServerEntity& mob,
                                                                random::RandomSource& random,
                                                                std::int32_t range = kStrollRange,
                                                                std::int32_t height = kStrollHeight);

/// How far a mob is from a point, for the goals that care.
[[nodiscard]] double distanceTo(const ServerEntity& mob, double x, double y, double z);

} // namespace mobBrain

} // namespace basalt::server::runtime::entity
