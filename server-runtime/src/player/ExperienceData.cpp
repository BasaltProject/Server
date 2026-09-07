#include <basalt/server/runtime/player/ExperienceData.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>

namespace basalt::server::runtime::player {
namespace {

constexpr std::int32_t kHighLevel = 30;
constexpr std::int32_t kMidLevel = 15;

} // namespace

std::int32_t ExperienceData::xpNeededForNextLevel() const {
    if (level_ >= kHighLevel) {
        return 112 + ((level_ - kHighLevel) * 9);
    }
    if (level_ >= kMidLevel) {
        return 37 + ((level_ - kMidLevel) * 5);
    }
    return 7 + (level_ * 2);
}

void ExperienceData::giveLevels(std::int32_t amount) {
    level_ += amount;
    if (level_ < 0) {
        level_ = 0;
        progress_ = 0.0F;
        total_ = 0;
    }
}

void ExperienceData::givePoints(std::int32_t points) {
    progress_ += static_cast<float>(points) / static_cast<float>(xpNeededForNextLevel());
    const std::int64_t sum = static_cast<std::int64_t>(total_) + points;
    total_ = static_cast<std::int32_t>(
        std::clamp(sum, std::int64_t{0}, static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max())));

    while (progress_ < 0.0F) {
        const float remaining = progress_ * static_cast<float>(xpNeededForNextLevel());
        if (level_ > 0) {
            giveLevels(-1);
            progress_ = 1.0F + (remaining / static_cast<float>(xpNeededForNextLevel()));
        } else {
            giveLevels(-1);
            progress_ = 0.0F;
        }
    }

    while (progress_ >= 1.0F) {
        progress_ = (progress_ - 1.0F) * static_cast<float>(xpNeededForNextLevel());
        giveLevels(1);
        progress_ /= static_cast<float>(xpNeededForNextLevel());
    }
}

void ExperienceData::setLevels(std::int32_t value) { level_ = value; }

void ExperienceData::setPoints(std::int32_t value) {
    const float needed = static_cast<float>(xpNeededForNextLevel());
    progress_ = std::clamp(static_cast<float>(value) / needed, 0.0F, (needed - 1.0F) / needed);
}

void ExperienceData::restore(std::int32_t level, float progress, std::int32_t total) {
    level_ = level;
    progress_ = progress;
    total_ = total;
}

void ExperienceData::reset() {
    level_ = 0;
    progress_ = 0.0F;
    total_ = 0;
}

} // namespace basalt::server::runtime::player
