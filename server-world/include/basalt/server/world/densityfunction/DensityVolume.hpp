#pragma once

#include <basalt/server/world/noise/JavaMath.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace basalt::server::world::densityfunction {

/// parity: `DensityVolume`.
struct DensityVolume {
    static constexpr std::int32_t kNoBlock = -1;

    std::int32_t sizeX = 1;
    std::int32_t sizeY = 1;
    std::int32_t sizeZ = 1;
    std::int32_t minBlockX = 0;
    std::int32_t minBlockY = 0;
    std::int32_t minBlockZ = 0;
    std::int32_t stepBlockX = 1;
    std::int32_t stepBlockY = 1;
    std::int32_t stepBlockZ = 1;

    [[nodiscard]] constexpr std::int32_t indexUnchecked(std::int32_t x, std::int32_t y, std::int32_t z) const noexcept {
        return y + (x + z * sizeX) * sizeY;
    }

    [[nodiscard]] constexpr std::int32_t blockX(std::int32_t x) const noexcept { return minBlockX + x * stepBlockX; }

    [[nodiscard]] constexpr std::int32_t blockY(std::int32_t y) const noexcept { return minBlockY + y * stepBlockY; }

    [[nodiscard]] constexpr std::int32_t blockZ(std::int32_t z) const noexcept { return minBlockZ + z * stepBlockZ; }

    [[nodiscard]] constexpr std::int32_t maxBlockX() const noexcept { return minBlockX + sizeX * stepBlockX - 1; }

    [[nodiscard]] constexpr std::int32_t maxBlockY() const noexcept { return minBlockY + sizeY * stepBlockY - 1; }

    [[nodiscard]] constexpr std::int32_t maxBlockZ() const noexcept { return minBlockZ + sizeZ * stepBlockZ - 1; }

    [[nodiscard]] constexpr std::int32_t size() const noexcept { return sizeX * sizeY * sizeZ; }

    [[nodiscard]] std::int32_t indexOfBlock(std::int32_t x, std::int32_t y, std::int32_t z) const noexcept {
        const std::int32_t rx = x - minBlockX;
        const std::int32_t ry = y - minBlockY;
        const std::int32_t rz = z - minBlockZ;
        if (stepBlockX == 1 && stepBlockY == 1 && stepBlockZ == 1) {
            if (rx >= 0 && ry >= 0 && rz >= 0 && rx < sizeX && ry < sizeY && rz < sizeZ) {
                return indexUnchecked(rx, ry, rz);
            }
            return kNoBlock;
        }
        namespace jm = noise::javaMath;
        if (rx >= 0 && ry >= 0 && rz >= 0 && rx < sizeX * stepBlockX && ry < sizeY * stepBlockY &&
            rz < sizeZ * stepBlockZ && jm::floorMod(rx, stepBlockX) == 0 && jm::floorMod(ry, stepBlockY) == 0 &&
            jm::floorMod(rz, stepBlockZ) == 0) {
            return indexUnchecked(jm::floorDiv(rx, stepBlockX), jm::floorDiv(ry, stepBlockY),
                                  jm::floorDiv(rz, stepBlockZ));
        }
        return kNoBlock;
    }

    friend constexpr bool operator==(const DensityVolume&, const DensityVolume&) = default;
};

/// parity: `DensityBuffer`.
class DensityBuffer {
public:
    DensityBuffer() = default;

    explicit DensityBuffer(std::span<float> values) noexcept : values_(values) {}

    void set(std::int32_t index, float value) noexcept { values_[static_cast<std::size_t>(index)] = value; }

    void setRange(std::int32_t index, std::int32_t count, float value) noexcept {
        std::fill_n(values_.begin() + index, count, value);
    }

    void addTo(std::int32_t index, float value) noexcept { values_[static_cast<std::size_t>(index)] += value; }

    [[nodiscard]] float get(std::int32_t index) const noexcept { return values_[static_cast<std::size_t>(index)]; }

    void fill(float value) noexcept { std::ranges::fill(values_, value); }

    void copyFrom(const DensityBuffer& other) noexcept { std::ranges::copy(other.values_, values_.begin()); }

    [[nodiscard]] std::int32_t size() const noexcept { return static_cast<std::int32_t>(values_.size()); }

    [[nodiscard]] std::span<float> values() const noexcept { return values_; }

private:
    std::span<float> values_;
};

} // namespace basalt::server::world::densityfunction
