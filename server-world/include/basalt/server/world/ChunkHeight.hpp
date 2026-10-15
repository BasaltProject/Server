#pragma once

#include <cstdint>

namespace basalt::server::world {

/// parity: `NoiseSettings.OVERWORLD_NOISE_SETTINGS`.
class ChunkHeight {
public:
    constexpr ChunkHeight(std::int32_t minY, std::int32_t height) : minY_(minY), height_(height) {}

    [[nodiscard]] constexpr std::int32_t minY() const noexcept { return minY_; }

    [[nodiscard]] constexpr std::int32_t height() const noexcept { return height_; }

    [[nodiscard]] constexpr std::int32_t maxY() const noexcept { return minY_ + height_ - 1; }

    [[nodiscard]] constexpr std::int32_t minSection() const noexcept { return minY_ >> 4; }

    [[nodiscard]] constexpr std::int32_t sectionCount() const noexcept { return height_ / 16; }

    [[nodiscard]] constexpr std::int32_t maxSection() const noexcept { return minSection() + sectionCount() - 1; }

    [[nodiscard]] constexpr bool isValid() const noexcept {
        return minY_ % 16 == 0 && height_ > 0 && height_ % 16 == 0 && minSection() >= -128 && minSection() <= 127 &&
               maxSection() >= -128 && maxSection() <= 127;
    }

    friend bool operator==(const ChunkHeight&, const ChunkHeight&) = default;

private:
    std::int32_t minY_;
    std::int32_t height_;
};

inline constexpr ChunkHeight kOverworldHeight{-64, 384};
inline constexpr ChunkHeight kNetherHeight{0, 256};
inline constexpr ChunkHeight kEndHeight{0, 256};

static_assert(kOverworldHeight.isValid());
static_assert(kOverworldHeight.sectionCount() == 24);
static_assert(kOverworldHeight.maxY() == 319);

} // namespace basalt::server::world
