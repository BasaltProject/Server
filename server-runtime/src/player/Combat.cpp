#include <basalt/server/runtime/player/Combat.hpp>

#include <algorithm>
#include <cmath>

namespace basalt::server::runtime::player::combat {
namespace {

constexpr float kProtectionDivider = 25.0F;
constexpr float kBaseToughness = 2.0F;
constexpr float kMinArmourRatio = 0.2F;
constexpr float kToughnessDivider = 4.0F;
constexpr std::int32_t kResistancePerLevel = 5;

/// parity: the epsilon `calculateFallPower` adds, without which a fall of exactly four blocks would
/// not hurt.
constexpr double kFallEpsilon = 1.0E-6;

/// parity: `getAttackStrengthScale(0.5)`'s own half tick, and the tick rate it divides.
constexpr double kSwingHalfTick = 0.5;
constexpr double kTicksPerSecond = 20.0;
constexpr float kMinChargeDamage = 0.2F;

constexpr double kKnockbackEpsilon = 1.0E-5;
constexpr double kMaxKnockbackLift = 0.4;

} // namespace

float afterArmour(float damage, float armour, float toughness) {
    const float scale = kBaseToughness + (toughness / kToughnessDivider);
    const float effective = std::clamp(armour - (damage / scale), armour * kMinArmourRatio, kMaxArmour);
    return damage * (1.0F - (effective / kProtectionDivider));
}

float afterProtection(float damage, float protection) {
    return damage * (1.0F - (std::clamp(protection, 0.0F, kMaxArmour) / kProtectionDivider));
}

std::int32_t fallDamage(double fallDistance, double safeFallDistance) {
    const double power = fallDistance + kFallEpsilon - safeFallDistance;
    return power <= 0.0 ? 0 : static_cast<std::int32_t>(std::floor(power));
}

float attackStrengthScale(std::int32_t ticksSinceSwing, double attackSpeed) {
    if (attackSpeed <= 0.0) {
        return 1.0F;
    }
    const double delay = 1.0 / attackSpeed * kTicksPerSecond;
    return std::clamp(static_cast<float>((static_cast<double>(ticksSinceSwing) + kSwingHalfTick) / delay), 0.0F, 1.0F);
}

float chargeScale(float strength) { return kMinChargeDamage + (strength * strength * (1.0F - kMinChargeDamage)); }

float afterDifficulty(float damage, std::string_view difficulty) {
    if (difficulty == "peaceful") {
        return 0.0F;
    }
    if (difficulty == "easy") {
        return std::min((damage / 2.0F) + 1.0F, damage);
    }
    if (difficulty == "hard") {
        return damage * 3.0F / 2.0F;
    }
    return damage;
}

std::array<double, 3> knockback(double power, double towardsX, double towardsZ, double velocityX, double velocityY,
                                double velocityZ, bool onGround, double knockbackResistance) {
    const std::array<double, 3> unchanged{velocityX, velocityY, velocityZ};
    const double strength = power * (1.0 - knockbackResistance);
    if (strength <= 0.0) {
        return unchanged;
    }
    const double length = std::sqrt((towardsX * towardsX) + (towardsZ * towardsZ));
    if (length < kKnockbackEpsilon) {
        return unchanged;
    }
    const double pushX = towardsX / length * strength;
    const double pushZ = towardsZ / length * strength;
    return {(velocityX / 2.0) - pushX, onGround ? std::min(kMaxKnockbackLift, (velocityY / 2.0) + strength) : velocityY,
            (velocityZ / 2.0) - pushZ};
}

float afterResistance(float damage, std::int32_t amplifier) {
    const std::int32_t level = (amplifier + 1) * kResistancePerLevel;
    const std::int32_t remaining = std::max(static_cast<std::int32_t>(kProtectionDivider) - level, 0);
    return std::max(damage * static_cast<float>(remaining) / kProtectionDivider, 0.0F);
}

} // namespace basalt::server::runtime::player::combat
