#pragma once

#include <basalt/server/runtime/entity/ai/BrainKit.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {
class WorldState;
}

namespace basalt::server::runtime::entity {

/// The breeze: sliding about its target, leaping behind it and firing wind charges.
///
/// parity: `Breeze`, `BreezeAi`, `Shoot`, `LongJump`, `ShootWhenStuck`, `Slide` and `BreezeUtil`.
namespace breezes {

/// parity: `Shoot`'s fifteen ticks of charging, four of recovering and ten of cooldown.
inline constexpr std::int64_t kShootCharging = 15;
inline constexpr std::int64_t kShootRecovering = 4;
inline constexpr std::int64_t kShootCooldown = 10;
inline constexpr std::int64_t kShootDuration = kShootCharging + 1 + kShootRecovering;
inline constexpr float kWindChargeSpeed = 0.7F;
/// parity: `LongJump`'s ten ticks of inhaling and its two hundred at most.
inline constexpr std::int64_t kInhaling = 10;
inline constexpr std::int64_t kLongJumpDuration = 200;
/// parity: `Sensor.wasEntityAttackableLastNTicks(body, 100)`.
inline constexpr std::int64_t kRememberTarget = 100;

/// The poses a breeze moves between.
enum class Pose : std::uint8_t { Standing, Sliding, LongJumping, Inhaling, Shooting };

struct BreezeState {
    Pose pose = Pose::Standing;
    ai::brainKit::Motion motion;
    ai::brainKit::Look look;
    /// parity: the last turn the target was attackable.
    std::int64_t targetSeen = 0;
    ai::brainKit::Memory<bool> shoot;
    ai::brainKit::Memory<bool> shootCooldown;
    ai::brainKit::Memory<bool> shootCharging;
    ai::brainKit::Memory<bool> shootRecovering;
    ai::brainKit::Memory<bool> jumpCooldown;
    ai::brainKit::Memory<bool> jumpInhaling;
    std::optional<ai::brainKit::Block> jumpTarget;
    bool leavingWater = false;
    ai::brainKit::Memory<std::int32_t> hurtBy;
    bool shooting = false;
    std::int64_t shootEnd = 0;
    bool jumping = false;
    std::int64_t jumpEnd = 0;
    /// parity: `SlideToTargetSink`, the idle walk with its own pose, twenty to forty ticks at most.
    bool sliding = false;
    std::int64_t slideEnd = 0;
    std::int32_t idleTicks = 0;
    std::int32_t soundTick = 0;
};

[[nodiscard]] BreezeState& stateOf(ServerEntity& breeze);

/// parity: `BreezeUtil.randomPointBehindTarget`: four to eight blocks behind where the enemy looks,
/// give or take a gaussian quarter turn.
[[nodiscard]] ai::brainKit::Vec pointBehind(const ai::brainKit::Vec& enemy, float headYaw,
                                            random::RandomSource& random);

/// parity: `BreezeUtil.hasLineOfSight`, from its feet, fifty blocks at most.
[[nodiscard]] bool lineOfSight(world::WorldState& world, const ServerEntity& breeze, const ai::brainKit::Vec& target);

/// parity: `Entity.lookAt(EYES, target)`: body, head and pitch all turned at once.
void lookAt(ServerEntity& breeze, const ai::brainKit::Vec& target);

} // namespace breezes

} // namespace basalt::server::runtime::entity
