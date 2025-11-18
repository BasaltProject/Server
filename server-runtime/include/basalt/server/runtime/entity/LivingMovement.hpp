#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// How something alive walks.
///
/// parity: `LivingEntity.travelInAir`, which is where the whole feel of walking comes from: the speed
/// an entity asks for is divided by the cube of the friction under it, and then that same friction is
/// taken back off the movement afterwards. On ice the two do not cancel, which is why it slides.
namespace livingMovement {

/// parity: `LivingEntity.DEFAULT_BASE_GRAVITY`.
inline constexpr double kGravity = 0.08;

/// parity: `BASE_JUMP_POWER`, what one jump is worth before any block or effect changes it.
inline constexpr double kJumpPower = 0.42;

/// parity: `Attributes.STEP_HEIGHT`'s default, which is what a mob climbs without jumping.
inline constexpr double kStepHeight = 0.6;

/// One tick of walking. `forward` and `strafe` are the input, in the entity's own facing, and `speed`
/// is `Attributes.MOVEMENT_SPEED`.
void travel(world::WorldState& world, ServerEntity& entity, double forward, double strafe, double speed,
            double gravity = kGravity);

/// One tick of swimming.
///
/// parity: `LivingEntity.travelInWater`: the input is worth a fiftieth of a block, the water takes
/// eight tenths of the speed back, and what is left of gravity is a slow sink.
void travelInWater(world::WorldState& world, ServerEntity& entity, double forward, double strafe);

/// parity: `LivingEntity.jumpFromGround`, plus the block's own jump factor.
void jump(world::WorldState& world, ServerEntity& entity);

} // namespace livingMovement

} // namespace basalt::server::runtime::entity
