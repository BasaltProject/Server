#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {
class WorldState;
}

namespace basalt::server::runtime::player {

/// A spear: the jab of a left click and the charge of holding it up.
///
/// parity: `Item.Properties.spear` and the `kinetic_weapon`, `piercing_weapon` and `attack_range`
/// components it sets.
namespace spear {

/// parity: `KineticWeapon.Condition.test`.
struct Condition {
    std::int32_t maxDurationTicks = 0;
    float minSpeed = 0.0F;
    float minRelativeSpeed = 0.0F;

    [[nodiscard]] bool test(std::int32_t ticksUsed, double attackerSpeed, double relativeSpeed,
                            double entityFactor) const;
};

/// One spear's default components.
struct Spear {
    std::string_view item;
    std::int32_t contactCooldownTicks = 10;
    std::int32_t delayTicks = 0;
    Condition dismount;
    Condition knockback;
    Condition damage;
    float damageMultiplier = 1.0F;
    std::string_view useSound;
    std::string_view hitSound;
    std::string_view attackSound;
};

/// parity: the `AttackRange(2.0F, 4.5F, 2.0F, 6.5F, 0.125F, 0.5F)` every spear carries.
struct Range {
    float minReach = 2.0F;
    float maxReach = 4.5F;
    float minCreativeReach = 2.0F;
    float maxCreativeReach = 6.5F;
    float hitboxMargin = 0.125F;
};

inline constexpr Range kRange{};

/// parity: `MINIMUM_ATTACK_CHARGE` of 1.0 and the five ticks of slack `handlePlayerAction` gives it.
inline constexpr float kMinimumAttackCharge = 1.0F;
inline constexpr std::int32_t kJabTolerance = 5;

/// parity: `Item.getUseDuration` for a kinetic weapon.
inline constexpr std::int32_t kUseDuration = 72000;

/// parity: `KineticWeapon.damageEntities`, a mob's charge counts a fifth.
inline constexpr float kPlayerFactor = 1.0F;

/// The spear this item is, or nothing.
[[nodiscard]] const Spear* of(std::string_view itemId);

/// parity: `Player.cannotAttackWithItem`: the swing must be charged, give or take the tolerance.
[[nodiscard]] bool tooSoon(std::int32_t ticksSinceSwing, double attackSpeed, std::int32_t tolerance);

/// parity: `Entity.calculateViewVector`.
[[nodiscard]] std::array<double, 3> lookOf(float yaw, float pitch);

struct Box {
    double minX = 0.0;
    double minY = 0.0;
    double minZ = 0.0;
    double maxX = 0.0;
    double maxY = 0.0;
    double maxZ = 0.0;
};

struct Candidate {
    std::int32_t id = 0;
    Box box;
};

/// Who a thrust from the eyes along the look reaches, in the order given.
///
/// parity: `ProjectileUtil.getHitEntitiesAlong` with the spear's range: nothing when a block is closer
/// than the near reach, the far reach stretched by the movement along the look, and a margin that
/// only counts where no block stands between it and the box.
[[nodiscard]] std::vector<std::int32_t> hitsAlong(world::WorldState& world, const std::array<double, 3>& eye,
                                                  const std::array<double, 3>& look, double movementAlongLook,
                                                  bool creative, std::span<const Candidate> candidates);

} // namespace spear

} // namespace basalt::server::runtime::player
