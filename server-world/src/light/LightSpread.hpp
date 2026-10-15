#pragma once

#include <basalt/server/world/light/Light.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::world::light {

class LightSpread {
public:
    /// `outside` is the level everything beyond the band is left at.
    LightSpread(const LightWindow& window, std::int32_t bottomY, std::int32_t topY, std::int32_t outside);

    [[nodiscard]] std::int32_t stateAt(std::int32_t x, std::int32_t y, std::int32_t z) const {
        return window_->stateAt(x, y, z);
    }

    [[nodiscard]] std::int32_t opacityAt(std::int32_t x, std::int32_t y, std::int32_t z) const {
        return opacity_[index(x, y, z)];
    }

    void set(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t level) {
        light_[index(x, y, z)] = static_cast<std::uint8_t>(level);
    }

    /// Writes a level and queues it even if it is already there, which `seed` deliberately will not.
    void push(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t level);

    void seed(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t level);

    [[nodiscard]] std::vector<SectionLight> run();

private:
    [[nodiscard]] std::size_t index(std::int32_t x, std::int32_t y, std::int32_t z) const {
        return static_cast<std::size_t>(((y - bottomY_) * LightWindow::kWidth + z) * LightWindow::kWidth + x);
    }

    /// parity: `LightEngine.getOpacity`.
    void spreadTo(std::size_t target, std::int32_t fromLevel);

    [[nodiscard]] std::vector<SectionLight> centreSections() const;

    const LightWindow* window_;
    std::int32_t bottomY_;
    std::int32_t topY_;
    std::int32_t outside_;
    std::vector<std::uint8_t> light_;
    std::vector<std::uint8_t> opacity_;
    std::vector<std::int32_t> queue_;
    std::size_t queueHead_ = 0;
};

} // namespace basalt::server::world::light
