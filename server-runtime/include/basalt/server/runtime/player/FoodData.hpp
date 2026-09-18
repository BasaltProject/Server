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

/// Hunger, saturation and exhaustion, ticked once per server tick per player. parity: `FoodData`.
/// The numbers here are the mechanic, not tuning, and farms are built on their exact timing.
///
/// `heal` and `hurt` are handed in rather than reached for, because health lives on the player.
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

    /// Puts back exactly what was persisted, exhaustion included: a rejoin should not reset the
    /// hunger clock.
    void restore(std::int32_t food, float saturation, float exhaustion);

    void reset();

    /// One tick. `health` is the player's current health, `heal` restores it, `hurt` applies
    /// starvation damage; all three keep this class free of the rest of the player.
    ///
    /// parity: `FoodData.tick`, with the world's `natural_health_regeneration` passed in.
    void tick(Difficulty difficulty, float health, float maxHealth, const std::function<void(float)>& heal,
              const std::function<void(float)>& hurt, bool naturalRegeneration = true);

    /// What moving this far costs in exhaustion.
    ///
    /// parity: `Player.checkMovementStatistics`. Walking and crouching really are free in 26.2
    /// (their multiplier is literally `0.0F` in the jar); sprinting costs 0.1 per block. Swimming
    /// costs 0.01 and a jump 0.05, or 0.2 while sprinting, both charged from here.
    [[nodiscard]] static float movementExhaustion(double dx, double dy, double dz, bool sprinting, bool onGround,
                                                  bool inWater);

    /// parity: `ServerPlayer.jumpFromGround`, which costs four times as much while sprinting.
    [[nodiscard]] static float jumpExhaustion(bool sprinting);

private:
    std::int32_t foodLevel_ = kMaxFood;
    float saturationLevel_ = kDefaultSaturation;
    float exhaustionLevel_ = 0.0F;
    std::int32_t tickTimer_ = 0;
};

} // namespace basalt::server::runtime::player
