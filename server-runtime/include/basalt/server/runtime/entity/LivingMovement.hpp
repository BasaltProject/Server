#pragma once

#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

namespace basalt::server::runtime::entity {

class ServerEntity;

namespace livingMovement {

inline constexpr double kGravity = 0.08;

/// parity: `BASE_JUMP_POWER`.
inline constexpr double kJumpPower = 0.42;

/// parity: `Attributes.STEP_HEIGHT`'s default.
inline constexpr double kStepHeight = 0.6;

struct Input {
    double strafe = 0.0;
    double up = 0.0;
    double forward = 0.0;
};

void travelAir(world::WorldState& world, ServerEntity& entity, const Input& input, double speed, double gravity,
               bool omnidirectional, bool noPhysics = false);

void travelFlying(world::WorldState& world, ServerEntity& entity, const Input& input, float airSpeed,
                  world::Submersion in, bool noPhysics = false);

void travel(world::WorldState& world, ServerEntity& entity, double forward, double strafe, double speed,
            double gravity = kGravity);

void travelInWater(world::WorldState& world, ServerEntity& entity, double forward, double strafe);

/// parity: `Entity.move(MoverType.SELF, getDeltaMovement())`, with no input, drag or gravity.
void moveByVelocity(world::WorldState& world, ServerEntity& entity);

/// parity: `AbstractFish.travelInWater`, a hundredth of input, 0.9 drag and a sinking of 0.005 without a target.
void travelInWaterFish(world::WorldState& world, ServerEntity& entity, double forward, bool noTarget);

/// parity: `Guardian.travelInWater`, a tenth of input, 0.9 drag and a sinking of 0.005 idle and without a target.
void travelInWaterGuardian(world::WorldState& world, ServerEntity& entity, double forward, bool moving, bool noTarget);

/// parity: `moveRelative` with an upward input, then `move(SELF)`, and nothing else.
void swimStep(world::WorldState& world, ServerEntity& entity, double forward, double up, double speed);

void travelSwimming(world::WorldState& world, ServerEntity& entity, double forward, double up, double speed,
                    bool sinks);

void jump(world::WorldState& world, ServerEntity& entity);

} // namespace livingMovement

} // namespace basalt::server::runtime::entity
