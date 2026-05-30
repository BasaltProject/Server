#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <vector>

namespace basalt::server::world {
class Chunk;
}

namespace basalt::server::world::blending {

/// parity: `CompositeDirection.Direction8`, in the order vanilla declares them.
enum class Side : std::uint8_t { North, NorthEast, East, SouthEast, South, SouthWest, West, NorthWest };

inline constexpr std::int32_t kSideCount = 8;

[[nodiscard]] constexpr std::int32_t sideStepX(Side side) noexcept {
    constexpr std::array<std::int32_t, kSideCount> kSteps{0, 1, 1, 1, 0, -1, -1, -1};
    return kSteps[static_cast<std::size_t>(side)];
}

[[nodiscard]] constexpr std::int32_t sideStepZ(Side side) noexcept {
    constexpr std::array<std::int32_t, kSideCount> kSteps{-1, -1, 0, 1, 1, 1, 0, -1};
    return kSteps[static_cast<std::size_t>(side)];
}

using SideSet = std::array<bool, kSideCount>;

/// parity: `BlendingData`, what an old chunk tells the new ones around it about its terrain.
class BlendingData {
public:
    static constexpr float kNoValue = std::numeric_limits<float>::max();

    BlendingData(std::int32_t minSection, std::int32_t maxSection, std::optional<std::vector<float>> heights);

    [[nodiscard]] std::int32_t minSection() const noexcept { return minSection_; }

    /// Exclusive, as `Packed.maxSection`.
    [[nodiscard]] std::int32_t maxSection() const noexcept { return maxSection_; }

    [[nodiscard]] std::int32_t minY() const noexcept { return minSection_ * 16; }

    [[nodiscard]] std::int32_t maxY() const noexcept { return maxSection_ * 16 - 1; }

    [[nodiscard]] std::int32_t areaHeight() const noexcept { return (maxSection_ - minSection_) * 16; }

    /// parity: `Packed.heights`, present once any column has one.
    [[nodiscard]] std::optional<std::vector<float>> packedHeights() const;

    /// parity: `BlendingData.calculateData`, which runs once.
    void calculate(const Chunk& chunk, const SideSet& newSides);

    [[nodiscard]] bool hasCalculated() const noexcept { return hasCalculatedData_; }

    [[nodiscard]] float heightAt(std::int32_t cellX, std::int32_t cellZ) const;

    [[nodiscard]] float densityAt(std::int32_t cellX, std::int32_t cellY, std::int32_t cellZ) const;

    using BiomeConsumer = std::function<void(std::int32_t cellX, std::int32_t cellZ, std::int32_t biome)>;
    using HeightConsumer = std::function<void(std::int32_t cellX, std::int32_t cellZ, float height)>;
    using DensityConsumer =
        std::function<void(std::int32_t cellX, std::int32_t cellY, std::int32_t cellZ, float density)>;

    void iterateBiomes(std::int32_t minCellX, std::int32_t quartY, std::int32_t minCellZ,
                       const BiomeConsumer& consumer) const;
    void iterateHeights(std::int32_t minCellX, std::int32_t minCellZ, const HeightConsumer& consumer) const;
    void iterateDensities(std::int32_t minCellX, std::int32_t minCellZ, std::int32_t fromCellY, std::int32_t toCellY,
                          const DensityConsumer& consumer) const;

private:
    void addValuesForColumn(std::int32_t index, const Chunk& chunk, std::int32_t blockX, std::int32_t blockZ);
    [[nodiscard]] std::int32_t heightAtXz(const Chunk& chunk, std::int32_t blockX, std::int32_t blockZ) const;
    [[nodiscard]] std::vector<float> densityColumn(const Chunk& chunk, std::int32_t x, std::int32_t z,
                                                   std::int32_t height) const;
    [[nodiscard]] std::vector<std::int32_t> biomeColumn(const Chunk& chunk, std::int32_t blockX,
                                                        std::int32_t blockZ) const;
    [[nodiscard]] float densityIn(const std::vector<float>& column, std::int32_t cellY) const;

    [[nodiscard]] std::int32_t cellCountPerColumn() const noexcept { return (maxSection_ - minSection_) * 2; }

    [[nodiscard]] std::int32_t quartCountPerColumn() const noexcept { return (maxSection_ - minSection_) * 4; }

    [[nodiscard]] std::int32_t columnMinY() const noexcept { return minSection_ * 2 + 1; }

    [[nodiscard]] std::int32_t cellYIndex(std::int32_t cellY) const noexcept { return cellY - columnMinY(); }

    std::int32_t minSection_;
    std::int32_t maxSection_;
    bool hasCalculatedData_ = false;
    std::vector<float> heights_;
    std::vector<std::vector<float>> densities_;
    std::vector<std::vector<std::int32_t>> biomes_;
};

} // namespace basalt::server::world::blending
