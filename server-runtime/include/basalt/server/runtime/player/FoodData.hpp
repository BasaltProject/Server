#pragma once

#include <cstdint>
#include <functional>

namespace basalt::server::runtime::player {

/// The four difficulties, in vanilla's own order (the id the protocol uses is the ordinal).
enum class Difficulty : std::uint8_t {
    Peaceful,
    Easy,
    Normal,
    Hard,
};

class FoodData {
public:
    static constexpr std::int32_t kMaxFood = 20;
    static constexpr float kDefaultSaturation = 5.0F;

    [[nodiscard]] std::int32_t foodLevel() const noexcept { return foodLevel_; }

    [[nodiscard]] float saturationLevel() const noexcept { return saturationLevel_; }

    [[nodiscard]] float exhaustionLevel() const noexcept { return exhaustionLevel_; }

    static constexpr std::int32_t kDefaultSaturatedRegenTicks = 10;
    static constexpr std::int32_t kDefaultUnsaturatedRegenTicks = 80;
    static constexpr std::int32_t kDefaultStarveTicks = 80;

    std::int32_t saturatedRegenTicks = kDefaultSaturatedRegenTicks;
    std::int32_t unsaturatedRegenTicks = kDefaultUnsaturatedRegenTicks;
    std::int32_t starveTicks = kDefaultStarveTicks;

    /// Exhaustion per action; vanilla's until the world the player is in says otherwise.
    struct Tuning {
        float sprint = 0.1F;
        float swim = 0.01F;
        float jump = 0.05F;
        float sprintJump = 0.2F;
        float combat = 0.1F;
        float regen = 6.0F;
    };

    Tuning tuning;

    void addExhaustion(float amount);

    void eat(std::int32_t food, float saturation);

    void set(std::int32_t food, float saturation);

    void restore(std::int32_t food, float saturation, float exhaustion);

    void reset();

    void tick(Difficulty difficulty, float health, float maxHealth, const std::function<void(float, bool fast)>& heal,
              const std::function<void(float)>& hurt, bool naturalRegeneration = true);

    [[nodiscard]] float movementExhaustion(double dx, double dy, double dz, bool sprinting, bool onGround,
                                           bool inWater) const;

    [[nodiscard]] float jumpExhaustion(bool sprinting) const;

private:
    std::int32_t foodLevel_ = kMaxFood;
    float saturationLevel_ = kDefaultSaturation;
    float exhaustionLevel_ = 0.0F;
    std::int32_t tickTimer_ = 0;
};

} // namespace basalt::server::runtime::player
