#pragma once

#include <cstdint>

namespace basalt::server::runtime::player {

class ExperienceData {
public:
    [[nodiscard]] std::int32_t level() const noexcept { return level_; }

    [[nodiscard]] float progress() const noexcept { return progress_; }

    [[nodiscard]] std::int32_t total() const noexcept { return total_; }

    [[nodiscard]] std::int32_t xpNeededForNextLevel() const;

    [[nodiscard]] std::int32_t score() const noexcept { return score_; }

    void setScore(std::int32_t value) noexcept { score_ = value; }

    void giveLevels(std::int32_t amount);

    void givePoints(std::int32_t points);

    void setLevels(std::int32_t value);

    /// parity: `ServerPlayer.setExperiencePoints`.
    void setPoints(std::int32_t value);

    void restore(std::int32_t level, float progress, std::int32_t total);

    void reset();

private:
    std::int32_t level_ = 0;
    float progress_ = 0.0F;
    std::int32_t total_ = 0;
    std::int32_t score_ = 0;
};

} // namespace basalt::server::runtime::player
