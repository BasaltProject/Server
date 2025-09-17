#pragma once

#include <basalt/server/world/light/Light.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::world::light {

/// The part both light engines share: a working grid over one window, the flood fill, and cutting
/// the middle chunk back out of it.
///
/// parity: `LayerLightEngine`. What changes between sky and block light is only where the light
/// starts, which is why the seeding is the caller's.
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

    /// Writes a level without queueing it: for light that has nowhere new to go.
    void set(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t level) {
        light_[index(x, y, z)] = static_cast<std::uint8_t>(level);
    }

    /// Writes a level and queues it even if it is already there, which `seed` deliberately will not.
    void push(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t level);

    void seed(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t level);

    /// Runs the fill and answers the middle chunk's light, one array per section.
    [[nodiscard]] std::vector<SectionLight> run();

private:
    [[nodiscard]] std::size_t index(std::int32_t x, std::int32_t y, std::int32_t z) const {
        return static_cast<std::size_t>(((y - bottomY_) * LightWindow::kWidth + z) * LightWindow::kWidth + x);
    }

    /// parity: `LightEngine.getOpacity`, max(1, dampening): air costs a level, and so does water.
    void spreadTo(std::size_t target, std::int32_t fromLevel);

    [[nodiscard]] std::vector<SectionLight> centreSections() const;

    const LightWindow* window_;
    std::int32_t bottomY_;
    std::int32_t topY_;
    std::int32_t outside_;
    std::vector<std::uint8_t> light_;
    std::vector<std::uint8_t> opacity_;
    /// A plain growable index queue: a deque of boxed integers would allocate per node, and there
    /// are hundreds of thousands of them.
    std::vector<std::int32_t> queue_;
    std::size_t queueHead_ = 0;
};

} // namespace basalt::server::world::light
