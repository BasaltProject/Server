#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace basalt::server::runtime::player::combat {

/// How much of a hit actually lands: armour first, then resistance and protection.
///
/// parity: `CombatRules` plus `LivingEntity.getDamageAfterArmorAbsorb`/`getDamageAfterMagicAbsorb`,
/// in that order, which is what makes armour worth less against a big hit than against a small one.
inline constexpr float kMaxArmour = 20.0F;

/// parity: `Attributes.SAFE_FALL_DISTANCE`.
inline constexpr double kSafeFallDistance = 3.0;

/// parity: `Player.attack`, where a critical hit is worth half again.
inline constexpr float kCriticalMultiplier = 1.5F;

/// parity: `CombatRules.getDamageAfterAbsorb`.
[[nodiscard]] float afterArmour(float damage, float armour, float toughness);

/// parity: `CombatRules.getDamageAfterMagicAbsorb`, the enchantment half of the same idea.
[[nodiscard]] float afterProtection(float damage, float protection);

/// What a fall of this many blocks costs.
///
/// parity: `LivingEntity.calculateFallDamage` over `calculateFallPower`: the distance minus the
/// three blocks of `Attributes.SAFE_FALL_DISTANCE`, floored, with vanilla's own epsilon so a fall
/// of exactly four blocks hurts.
[[nodiscard]] std::int32_t fallDamage(double fallDistance, double safeFallDistance = kSafeFallDistance);

/// How much of a swing has recharged, from zero to one.
///
/// parity: `Player.getAttackStrengthScale(0.5)`, whose delay is twenty ticks over the attack speed:
/// a hand swings four times a second, an iron sword one and a half.
[[nodiscard]] float attackStrengthScale(std::int32_t ticksSinceSwing, double attackSpeed);

/// parity: `Player.baseDamageScaleFactor`: a swing that has not recharged still lands a fifth of
/// the damage, and the rest comes back with the square of the charge.
[[nodiscard]] float chargeScale(float strength);

/// What the difficulty leaves of a hit dealt by something that is not a player.
///
/// parity: `Player.hurtServer`: peaceful takes nothing, easy halves it and adds one back (never
/// more than the hit itself), normal is the hit, and hard is half again.
[[nodiscard]] float afterDifficulty(float damage, std::string_view difficulty);

/// parity: `LivingEntity.knockback`: the speed keeps half of what it had, the push is taken off it,
/// and on the ground it also gets a lift capped at 0.4.
[[nodiscard]] std::array<double, 3> knockback(double power, double towardsX, double towardsZ, double velocityX,
                                              double velocityY, double velocityZ, bool onGround,
                                              double knockbackResistance = 0.0);

/// parity: `LivingEntity.getDamageAfterMagicAbsorb`'s resistance case, which counts in fifths and
/// floors at zero rather than going negative at level four and above.
[[nodiscard]] float afterResistance(float damage, std::int32_t amplifier);

} // namespace basalt::server::runtime::player::combat
