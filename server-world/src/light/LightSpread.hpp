#pragma once

#include <basalt/server/world/light/Light.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::world::light {

class LightSpread {
public:
    /// `outside` is the level above the band; below it nothing is lit.
    LightSpread(const LightWindow& window, std::int32_t bottomY, std::int32_t topY, std::int32_t outside);

    [[nodiscard]] std::int32_t stateAt(std::int32_t x, std::int32_t y, std::int32_t z) const {
        return window_->stateAt(x, y, z);
    }

    [[nodiscard]] std::int32_t opacityAt(std::int32_t x, std::int32_t y, std::int32_t z) const {
        return cells_[index(x, y, z)] >> 4;
    }

    void set(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t level) {
        std::uint8_t& cell = cells_[index(x, y, z)];
        cell = static_cast<std::uint8_t>((cell & 0xF0) | level);
    }

    /// Writes a level and queues it even if it is already there, which `seed` deliberately will not.
    void push(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t level);

    void seed(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t level);

    [[nodiscard]] std::vector<SectionLight> run();

private:
    [[nodiscard]] std::size_t index(std::int32_t x, std::int32_t y, std::int32_t z) const {
        return static_cast<std::size_t>(((y - bottomY_) * LightWindow::kWidth + z) * LightWindow::kWidth + x);
    }

    /// Each cell packs opacity in the high nibble and light in the low one. parity: `LightEngine.getOpacity`.
    [[gnu::always_inline]] void spreadTo(std::size_t target, std::int32_t fromLevel);

    [[nodiscard]] std::int32_t lightAt(std::size_t at) const { return cells_[at] & 15; }

    [[nodiscard]] std::vector<SectionLight> centreSections() const;

    const LightWindow* window_;
    std::int32_t bottomY_;
    std::int32_t topY_;
    std::int32_t outside_;
    std::vector<std::uint8_t> cells_;
    std::vector<std::int32_t> queue_;
    std::size_t queueHead_ = 0;
};

} // namespace basalt::server::world::light
