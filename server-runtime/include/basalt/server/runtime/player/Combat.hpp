#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace basalt::server::runtime::player::combat {

inline constexpr float kMaxArmour = 20.0F;

inline constexpr double kSafeFallDistance = 3.0;

inline constexpr float kCriticalMultiplier = 1.5F;

[[nodiscard]] float afterArmour(float damage, float armour, float toughness, float fractionAdd = 0.0F);

[[nodiscard]] float afterProtection(float damage, float protection);

[[nodiscard]] std::int32_t fallDamage(double fallDistance, double safeFallDistance = kSafeFallDistance);

[[nodiscard]] float attackStrengthScale(std::int32_t ticksSinceSwing, double attackSpeed);

/// parity: `Player.baseDamageScaleFactor`.
[[nodiscard]] float chargeScale(float strength);

/// parity: `Player.hurtServer`.
[[nodiscard]] float afterDifficulty(float damage, std::string_view difficulty);

[[nodiscard]] std::array<double, 3> knockback(double power, double towardsX, double towardsZ, double velocityX,
                                              double velocityY, double velocityZ, bool onGround,
                                              double knockbackResistance = 0.0);

[[nodiscard]] bool withinAttackReach(double playerX, double playerY, double playerZ, double targetX, double targetY,
                                     double targetZ, double targetWidth, double targetHeight);

struct Swing {
    float damage = 0.0F;
    bool fullStrength = false;
    /// parity: `canCriticalAttack`, the falling half of a jump and nothing else.
    bool critical = false;
    bool knockbackAttack = false;
};

[[nodiscard]] Swing swingOf(float strength, float base, float enchanted, bool falling, bool onGround, bool sprinting,
                            float bonus = 0.0F);

[[nodiscard]] float afterResistance(float damage, std::int32_t amplifier);

} // namespace basalt::server::runtime::player::combat
