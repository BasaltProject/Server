#include <basalt/server/runtime/player/FoodData.hpp>

#include <algorithm>
#include <cmath>

namespace basalt::server::runtime::player {
namespace {

constexpr float kMaxExhaustion = 40.0F;
constexpr float kExhaustionPerPoint = 4.0F;
constexpr std::int32_t kRegenFoodThreshold = 18;
constexpr std::int32_t kRegenTicks = 80;
constexpr float kRegenExhaustion = 6.0F;
constexpr std::int32_t kSaturatedRegenTicks = 10;
constexpr float kSaturatedRegenCost = 6.0F;
constexpr std::int32_t kStarveTicks = 80;
constexpr float kStarveFloorEasy = 10.0F;
constexpr float kStarveFloorHard = 1.0F;

constexpr float kSprintExhaustionPerCm = 0.1F;
constexpr float kSwimExhaustionPerCm = 0.01F;
constexpr float kJumpExhaustion = 0.05F;
constexpr float kSprintJumpExhaustion = 0.2F;

/// parity: `Math.round(float)`, which is a floor of the value plus a half and not a round to even.
[[nodiscard]] std::int32_t rounded(float value) { return static_cast<std::int32_t>(std::floor(value + 0.5F)); }

} // namespace

void FoodData::addExhaustion(float amount) { exhaustionLevel_ = std::min(exhaustionLevel_ + amount, kMaxExhaustion); }

void FoodData::eat(std::int32_t food, float saturation) {
    foodLevel_ = std::clamp(foodLevel_ + food, 0, kMaxFood);
    saturationLevel_ = std::clamp(saturationLevel_ + saturation, 0.0F, static_cast<float>(foodLevel_));
}

void FoodData::set(std::int32_t food, float saturation) {
    foodLevel_ = food;
    saturationLevel_ = saturation;
}

void FoodData::restore(std::int32_t food, float saturation, float exhaustion) {
    foodLevel_ = food;
    saturationLevel_ = saturation;
    exhaustionLevel_ = exhaustion;
    tickTimer_ = 0;
}

void FoodData::reset() {
    foodLevel_ = kMaxFood;
    saturationLevel_ = kDefaultSaturation;
    exhaustionLevel_ = 0.0F;
    tickTimer_ = 0;
}

void FoodData::tick(Difficulty difficulty, float health, float maxHealth, const std::function<void(float)>& heal,
                    const std::function<void(float)>& hurt) {
    if (exhaustionLevel_ > kExhaustionPerPoint) {
        exhaustionLevel_ -= kExhaustionPerPoint;
        if (saturationLevel_ > 0.0F) {
            saturationLevel_ = std::max(saturationLevel_ - 1.0F, 0.0F);
        } else if (difficulty != Difficulty::Peaceful) {
            foodLevel_ = std::max(foodLevel_ - 1, 0);
        }
    }

    const bool hurtAlready = health < maxHealth;
    if (saturationLevel_ > 0.0F && hurtAlready && foodLevel_ >= kMaxFood) {
        tickTimer_++;
        if (tickTimer_ >= kSaturatedRegenTicks) {
            const float spent = std::min(saturationLevel_, kSaturatedRegenCost);
            heal(spent / kSaturatedRegenCost);
            addExhaustion(spent);
            tickTimer_ = 0;
        }
        return;
    }
    if (foodLevel_ >= kRegenFoodThreshold && hurtAlready) {
        tickTimer_++;
        if (tickTimer_ >= kRegenTicks) {
            heal(1.0F);
            addExhaustion(kRegenExhaustion);
            tickTimer_ = 0;
        }
        return;
    }
    if (foodLevel_ <= 0) {
        tickTimer_++;
        if (tickTimer_ >= kStarveTicks) {
            const bool mayStarve = health > kStarveFloorEasy || difficulty == Difficulty::Hard ||
                                   (health > kStarveFloorHard && difficulty == Difficulty::Normal);
            if (mayStarve) {
                hurt(1.0F);
            }
            tickTimer_ = 0;
        }
        return;
    }
    tickTimer_ = 0;
}

float FoodData::movementExhaustion(double dx, double dy, double dz, bool sprinting, bool onGround, bool inWater) {
    // parity: `Player.checkMovementStatistics` charges for swimming in three dimensions and for
    // sprinting in two, and for nothing else a player does on their own feet.
    const double distance = inWater ? std::sqrt((dx * dx) + (dy * dy) + (dz * dz)) : std::sqrt((dx * dx) + (dz * dz));
    const std::int32_t centimetres = rounded(static_cast<float>(distance) * 100.0F);
    if (centimetres <= 0) {
        return 0.0F;
    }
    if (inWater) {
        return kSwimExhaustionPerCm * static_cast<float>(centimetres) * 0.01F;
    }
    if (onGround && sprinting) {
        return kSprintExhaustionPerCm * static_cast<float>(centimetres) * 0.01F;
    }
    return 0.0F;
}

float FoodData::jumpExhaustion(bool sprinting) { return sprinting ? kSprintJumpExhaustion : kJumpExhaustion; }

} // namespace basalt::server::runtime::player
