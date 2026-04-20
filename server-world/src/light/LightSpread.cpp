#include "LightSpread.hpp"

#include <algorithm>
#include <cstdint>

namespace basalt::server::world::light {
namespace {

constexpr std::size_t kBlocksPerSection = 16 * 16 * 16;
constexpr std::size_t kInitialQueue = 1U << 16;

} // namespace

LightSpread::LightSpread(const LightWindow& window, std::int32_t bottomY, std::int32_t topY, std::int32_t outside)
    : window_(&window), bottomY_(bottomY), topY_(topY), outside_(outside) {
    const auto cells = static_cast<std::size_t>(LightWindow::kWidth) * LightWindow::kWidth *
                       static_cast<std::size_t>(topY - bottomY + 1);
    light_.assign(cells, 0);
    opacity_.assign(cells, 0);
    queue_.reserve(kInitialQueue);
    window.fillOpacity(opacity_, bottomY, topY, skyLight::opacityTable());
}

void LightSpread::push(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t level) {
    const std::size_t at = index(x, y, z);
    light_[at] = static_cast<std::uint8_t>(level);
    queue_.push_back(static_cast<std::int32_t>(at));
}

void LightSpread::seed(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t level) {
    const std::size_t at = index(x, y, z);
    if (level <= light_[at]) {
        return;
    }
    light_[at] = static_cast<std::uint8_t>(level);
    queue_.push_back(static_cast<std::int32_t>(at));
}

void LightSpread::spreadTo(std::size_t target, std::int32_t fromLevel) {
    const std::int32_t next = fromLevel - std::max(1, static_cast<std::int32_t>(opacity_[target]));
    if (next <= light_[target]) {
        return;
    }
    light_[target] = static_cast<std::uint8_t>(next);
    queue_.push_back(static_cast<std::int32_t>(target));
}

std::vector<SectionLight> LightSpread::run() {
    constexpr std::size_t width = LightWindow::kWidth;
    constexpr std::size_t layer = width * width;
    const std::size_t cells = light_.size();
    while (queueHead_ < queue_.size()) {
        const auto at = static_cast<std::size_t>(queue_[queueHead_++]);
        const std::int32_t level = light_[at];
        if (level <= 1) {
            continue;
        }
        const std::size_t x = at % width;
        const std::size_t z = (at / width) % width;
        if (x > 0) {
            spreadTo(at - 1, level);
        }
        if (x < width - 1) {
            spreadTo(at + 1, level);
        }
        if (z > 0) {
            spreadTo(at - width, level);
        }
        if (z < width - 1) {
            spreadTo(at + width, level);
        }
        if (at >= layer) {
            spreadTo(at - layer, level);
        }
        if (at + layer < cells) {
            spreadTo(at + layer, level);
        }
    }
    return centreSections();
}

std::vector<SectionLight> LightSpread::centreSections() const {
    const Chunk& chunk = window_->centre();
    std::vector<SectionLight> sections;
    sections.reserve(chunk.sectionCount());
    for (std::size_t index = 0; index < chunk.sectionCount(); ++index) {
        SectionLight values(kBlocksPerSection, outside_);
        const std::int32_t sectionMinY = chunk.verticalBounds().minY() + static_cast<std::int32_t>(index) * 16;
        for (std::int32_t y = 0; y < 16; ++y) {
            const std::int32_t worldY = sectionMinY + y;
            if (worldY < bottomY_ || worldY > topY_) {
                continue;
            }
            for (std::int32_t z = 0; z < 16; ++z) {
                for (std::int32_t x = 0; x < 16; ++x) {
                    values[static_cast<std::size_t>((y * 16 + z) * 16 + x)] =
                        light_[this->index(x + 16, worldY, z + 16)];
                }
            }
        }
        sections.push_back(std::move(values));
    }
    return sections;
}

} // namespace basalt::server::world::light
