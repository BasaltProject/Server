#pragma once

#include <cstdint>

namespace basalt::server::runtime::player {

/// The experience bar: a level, a fraction of the way to the next one, and the running total.
/// parity: `Player.giveExperiencePoints`/`giveExperienceLevels`/`getXpNeededForNextLevel`: the cost
/// curve (7 + 2n below 15, 37 + 5n below 30, 112 + 9n above) and the overflow loop's odd two-step
/// renormalisation are kept so the same points always land on the same level.
class ExperienceData {
public:
    [[nodiscard]] std::int32_t level() const noexcept { return level_; }

    [[nodiscard]] float progress() const noexcept { return progress_; }

    [[nodiscard]] std::int32_t total() const noexcept { return total_; }

    [[nodiscard]] std::int32_t xpNeededForNextLevel() const;

    /// parity: `Player.getScore`, every point ever given, shown on the death screen.
    [[nodiscard]] std::int32_t score() const noexcept { return score_; }

    void setScore(std::int32_t value) noexcept { score_ = value; }

    void giveLevels(std::int32_t amount);

    void givePoints(std::int32_t points);

    /// parity: `ServerPlayer.setExperienceLevels`, which moves the level and leaves the bar where it
    /// is.
    void setLevels(std::int32_t value);

    /// parity: `ServerPlayer.setExperiencePoints`. It keeps the level and moves only the bar, and
    /// clamps to `(needed - 1) / needed` so setting exactly one level's worth of points never rolls
    /// the bar over into the next level.
    void setPoints(std::int32_t value);

    /// Puts back exactly what was persisted. Unlike `setLevels` this also restores the running
    /// total.
    void restore(std::int32_t level, float progress, std::int32_t total);

    void reset();

private:
    std::int32_t level_ = 0;
    float progress_ = 0.0F;
    std::int32_t total_ = 0;
    std::int32_t score_ = 0;
};

} // namespace basalt::server::runtime::player
