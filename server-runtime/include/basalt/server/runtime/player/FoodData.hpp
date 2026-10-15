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

    void addExhaustion(float amount);

    void eat(std::int32_t food, float saturation);

    void set(std::int32_t food, float saturation);

    void restore(std::int32_t food, float saturation, float exhaustion);

    void reset();

    void tick(Difficulty difficulty, float health, float maxHealth, const std::function<void(float)>& heal,
              const std::function<void(float)>& hurt, bool naturalRegeneration = true);

    [[nodiscard]] static float movementExhaustion(double dx, double dy, double dz, bool sprinting, bool onGround,
                                                  bool inWater);

    [[nodiscard]] static float jumpExhaustion(bool sprinting);

private:
    std::int32_t foodLevel_ = kMaxFood;
    float saturationLevel_ = kDefaultSaturation;
    float exhaustionLevel_ = 0.0F;
    std::int32_t tickTimer_ = 0;
};

} // namespace basalt::server::runtime::player
