#include <basalt/server/runtime/world/MobLocalDifficulty.hpp>

#include <algorithm>
#include <cstdint>

namespace basalt::server::runtime::world::mobLocalDifficulty {
namespace {

constexpr float kGlobalTimeOffset = 72000.0F;
constexpr float kMaxGlobalTime = 1440000.0F;
constexpr float kMaxLocalTime = 3600000.0F;
constexpr float kBaseScale = 0.75F;
constexpr float kGlobalTimeWeight = 0.25F;
constexpr float kMoonWeight = 0.25F;

[[nodiscard]] std::int32_t difficultyId(std::string_view difficulty) {
    if (difficulty == "peaceful") {
        return 0;
    }
    if (difficulty == "normal") {
        return 2;
    }
    if (difficulty == "hard") {
        return 3;
    }
    // Easy, and anything nobody recognizes: a word that is not a difficulty is not a reason to
    // make the world harder than it was.
    return 1;
}

} // namespace

float effectiveDifficulty(std::string_view difficulty, std::int64_t gameTime, std::int64_t inhabitedTime,
                          float moonBrightness) {
    const std::int32_t id = difficultyId(difficulty);
    // Multiplying by zero would answer the same; peaceful leaves before the arithmetic because
    // that is what the rule says, not because the numbers happen to agree.
    if (id == 0) {
        return 0.0F;
    }

    const float globalBonus =
        std::clamp((static_cast<float>(gameTime) - kGlobalTimeOffset) / kMaxGlobalTime, 0.0F, 1.0F) * kGlobalTimeWeight;
    float localScale = std::clamp(static_cast<float>(inhabitedTime) / kMaxLocalTime, 0.0F, 1.0F) *
                       (difficulty == "hard" ? 1.0F : 0.75F);
    // parity: the moon's share is capped by the global bonus, so a full moon on a young world is
    // worth nothing at all.
    localScale += std::clamp(moonBrightness * kMoonWeight, 0.0F, globalBonus);
    if (difficulty == "easy") {
        localScale *= 0.5F;
    }
    return static_cast<float>(id) * (kBaseScale + globalBonus + localScale);
}

float specialMultiplier(std::string_view difficulty, std::int64_t gameTime, std::int64_t inhabitedTime,
                        float moonBrightness) {
    const float effective = effectiveDifficulty(difficulty, gameTime, inhabitedTime, moonBrightness);
    if (effective < 2.0F) {
        return 0.0F;
    }
    if (effective > 4.0F) {
        return 1.0F;
    }
    return (effective - 2.0F) / 2.0F;
}

} // namespace basalt::server::runtime::world::mobLocalDifficulty
