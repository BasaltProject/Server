#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/blending/BlendingData.hpp>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <string_view>

namespace basalt::server::world::blending {
namespace {

constexpr std::int32_t kQuartsPerSection = 4;
constexpr std::int32_t kInsideMax = kQuartsPerSection - 1;
constexpr std::int32_t kOutsideMax = kQuartsPerSection;
constexpr std::int32_t kInsideCount = 2 * kInsideMax + 1;
constexpr std::int32_t kOutsideCount = 2 * kOutsideMax + 1;
constexpr std::int32_t kColumnCount = kInsideCount + kOutsideCount;

[[nodiscard]] constexpr std::int32_t insideIndex(std::int32_t x, std::int32_t z) noexcept { return kInsideMax - x + z; }

[[nodiscard]] constexpr std::int32_t outsideIndex(std::int32_t x, std::int32_t z) noexcept {
    return kInsideCount + x + kOutsideMax - z;
}

[[nodiscard]] constexpr std::int32_t zeroIfNegative(std::int32_t value) noexcept { return value & ~(value >> 31); }

[[nodiscard]] constexpr std::int32_t cellXOf(std::int32_t index) noexcept {
    if (index < kInsideCount) {
        return zeroIfNegative(kInsideMax - index);
    }
    return kOutsideMax - zeroIfNegative(kOutsideMax - (index - kInsideCount));
}

[[nodiscard]] constexpr std::int32_t cellZOf(std::int32_t index) noexcept {
    if (index < kInsideCount) {
        return zeroIfNegative(index - kInsideMax);
    }
    return kOutsideMax - zeroIfNegative(index - kInsideCount - kOutsideMax);
}

[[nodiscard]] bool endsWith(std::string_view text, std::string_view suffix) {
    return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

struct StateFlags {
    std::vector<bool> ground;
    std::vector<bool> surface;
};

// parity: `BlendingData.SURFACE_BLOCKS` and `isGround`, with `BlockTags.LOGS` read from the block's name.
[[nodiscard]] const StateFlags& flags() {
    static StateFlags table;
    static std::once_flag once;
    std::call_once(once, [] {
        const std::int32_t count = registry::blocks::stateCount();
        table.ground.assign(static_cast<std::size_t>(count), false);
        table.surface.assign(static_cast<std::size_t>(count), false);
        constexpr std::array<std::string_view, 11> kSurface{"podzol",      "gravel",     "grass_block", "stone",
                                                            "coarse_dirt", "sand",       "red_sand",    "mycelium",
                                                            "snow_block",  "terracotta", "dirt"};
        for (std::int32_t id = 0; id < count; ++id) {
            const std::optional<registry::BlockState> state = registry::blocks::byStateId(id);
            if (!state.has_value()) {
                continue;
            }
            const std::string_view path = state->block.path();
            table.surface[static_cast<std::size_t>(id)] = std::ranges::find(kSurface, path) != kSurface.end();
            const bool log = endsWith(path, "_log") || endsWith(path, "_wood") || endsWith(path, "_stem") ||
                             endsWith(path, "_hyphae");
            const bool ground = !registry::blocks::isAir(id) && !registry::blockStateFamilies::isLeavesState(id) &&
                                !log && path != "brown_mushroom_block" && path != "red_mushroom_block" &&
                                !registry::blockShapes::isEmptyShape(id);
            table.ground[static_cast<std::size_t>(id)] = ground;
        }
    });
    return table;
}

[[nodiscard]] bool isGround(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    if (y < chunk.verticalBounds().minY() || y > chunk.verticalBounds().maxY()) {
        return false;
    }
    return flags().ground[static_cast<std::size_t>(chunk.blockStateId(x, y, z))];
}

[[nodiscard]] float read1(const Chunk& chunk, std::int32_t x, std::int32_t& y, std::int32_t z) {
    --y;
    return isGround(chunk, x, y, z) ? 1.0F : -1.0F;
}

[[nodiscard]] float read7(const Chunk& chunk, std::int32_t x, std::int32_t& y, std::int32_t z) {
    float sum = 0.0F;
    for (std::int32_t i = 0; i < 7; ++i) {
        sum += read1(chunk, x, y, z);
    }
    return sum;
}

} // namespace

BlendingData::BlendingData(std::int32_t minSection, std::int32_t maxSection, std::optional<std::vector<float>> heights)
    : minSection_(minSection), maxSection_(maxSection),
      heights_(heights.has_value() && static_cast<std::int32_t>(heights->size()) == kColumnCount
                   ? std::move(*heights)
                   : std::vector<float>(kColumnCount, kNoValue)),
      densities_(kColumnCount), biomes_(kColumnCount) {}

std::optional<std::vector<float>> BlendingData::packedHeights() const {
    const bool hasHeight = std::ranges::any_of(heights_, [](float value) { return value != kNoValue; });
    return hasHeight ? std::optional{heights_} : std::nullopt;
}

void BlendingData::calculate(const Chunk& chunk, const SideSet& newSides) {
    if (hasCalculatedData_) {
        return;
    }
    const auto has = [&newSides](Side side) { return newSides[static_cast<std::size_t>(side)]; };
    if (has(Side::North) || has(Side::West) || has(Side::NorthWest)) {
        addValuesForColumn(insideIndex(0, 0), chunk, 0, 0);
    }
    if (has(Side::North)) {
        for (std::int32_t i = 1; i < kQuartsPerSection; ++i) {
            addValuesForColumn(insideIndex(i, 0), chunk, 4 * i, 0);
        }
    }
    if (has(Side::West)) {
        for (std::int32_t i = 1; i < kQuartsPerSection; ++i) {
            addValuesForColumn(insideIndex(0, i), chunk, 0, 4 * i);
        }
    }
    if (has(Side::East)) {
        for (std::int32_t i = 1; i < kQuartsPerSection; ++i) {
            addValuesForColumn(outsideIndex(kOutsideMax, i), chunk, 15, 4 * i);
        }
    }
    if (has(Side::South)) {
        for (std::int32_t i = 0; i < kQuartsPerSection; ++i) {
            addValuesForColumn(outsideIndex(i, kOutsideMax), chunk, 4 * i, 15);
        }
    }
    if (has(Side::East) && has(Side::NorthEast)) {
        addValuesForColumn(outsideIndex(kOutsideMax, 0), chunk, 15, 0);
    }
    if (has(Side::East) && has(Side::South) && has(Side::SouthEast)) {
        addValuesForColumn(outsideIndex(kOutsideMax, kOutsideMax), chunk, 15, 15);
    }
    hasCalculatedData_ = true;
}

void BlendingData::addValuesForColumn(std::int32_t index, const Chunk& chunk, std::int32_t blockX,
                                      std::int32_t blockZ) {
    const auto at = static_cast<std::size_t>(index);
    if (heights_[at] == kNoValue) {
        heights_[at] = static_cast<float>(heightAtXz(chunk, blockX, blockZ));
    }
    densities_[at] = densityColumn(chunk, blockX, blockZ, static_cast<std::int32_t>(std::floor(heights_[at])));
    biomes_[at] = biomeColumn(chunk, blockX, blockZ);
}

std::int32_t BlendingData::heightAtXz(const Chunk& chunk, std::int32_t blockX, std::int32_t blockZ) const {
    // parity: the `WORLD_SURFACE_WG` heightmap of an old chunk is never primed, so the scan starts at the top.
    const std::int32_t top = std::min(maxY(), chunk.verticalBounds().maxY());
    const std::int32_t bottom = minY();
    const StateFlags& table = flags();
    for (std::int32_t y = top; y > bottom; --y) {
        if (y >= chunk.verticalBounds().minY() &&
            table.surface[static_cast<std::size_t>(chunk.blockStateId(blockX, y, blockZ))]) {
            return y;
        }
    }
    return bottom;
}

std::vector<float> BlendingData::densityColumn(const Chunk& chunk, std::int32_t x, std::int32_t z,
                                               std::int32_t height) const {
    std::vector<float> densities(static_cast<std::size_t>(cellCountPerColumn()), -1.0F);
    std::int32_t y = maxY() + 1;
    float last7 = read7(chunk, x, y, z);
    for (std::int32_t cellIndex = static_cast<std::int32_t>(densities.size()) - 2; cellIndex >= 0; --cellIndex) {
        const float one = read1(chunk, x, y, z);
        const float current7 = read7(chunk, x, y, z);
        densities[static_cast<std::size_t>(cellIndex)] = (last7 + one + current7) / 15.0F;
        last7 = current7;
    }
    const std::int32_t highestCellWithSurface = cellYIndex(static_cast<std::int32_t>(std::floor(height / 8.0)));
    if (highestCellWithSurface >= 0 && highestCellWithSurface < static_cast<std::int32_t>(densities.size()) - 1) {
        const float inCell = std::fmod(static_cast<float>(height) + 0.5F, 8.0F) / 8.0F;
        const float amplitudeAbove = (1.0F - inCell) / inCell;
        const float max = std::max(amplitudeAbove, 1.0F) * 0.25F;
        densities[static_cast<std::size_t>(highestCellWithSurface) + 1] = -amplitudeAbove / max;
        densities[static_cast<std::size_t>(highestCellWithSurface)] = 1.0F / max;
    }
    return densities;
}

std::vector<std::int32_t> BlendingData::biomeColumn(const Chunk& chunk, std::int32_t blockX,
                                                    std::int32_t blockZ) const {
    std::vector<std::int32_t> biomes(static_cast<std::size_t>(quartCountPerColumn()));
    const std::int32_t chunkQuartMin = chunk.verticalBounds().minY() >> 2;
    const std::int32_t chunkQuartMax = chunkQuartMin + chunk.verticalBounds().height() / 4 - 1;
    for (std::size_t index = 0; index < biomes.size(); ++index) {
        const std::int32_t quartY = static_cast<std::int32_t>(index) + (minY() >> 2);
        const std::int32_t clamped = std::clamp(quartY, chunkQuartMin, chunkQuartMax);
        biomes[index] = chunk.biomeId(blockX >> 2, clamped - chunkQuartMin, blockZ >> 2);
    }
    return biomes;
}

float BlendingData::heightAt(std::int32_t cellX, std::int32_t cellZ) const {
    if (cellX != kOutsideMax && cellZ != kOutsideMax) {
        return cellX != 0 && cellZ != 0 ? kNoValue : heights_[static_cast<std::size_t>(insideIndex(cellX, cellZ))];
    }
    return heights_[static_cast<std::size_t>(outsideIndex(cellX, cellZ))];
}

float BlendingData::densityIn(const std::vector<float>& column, std::int32_t cellY) const {
    if (column.empty()) {
        return kNoValue;
    }
    const std::int32_t yIndex = cellYIndex(cellY);
    return yIndex >= 0 && yIndex < static_cast<std::int32_t>(column.size())
               ? column[static_cast<std::size_t>(yIndex)] * 0.1F
               : kNoValue;
}

float BlendingData::densityAt(std::int32_t cellX, std::int32_t cellY, std::int32_t cellZ) const {
    if (cellY == minSection_ * 2) {
        return 0.1F;
    }
    if (cellX != kOutsideMax && cellZ != kOutsideMax) {
        return cellX != 0 && cellZ != 0
                   ? kNoValue
                   : densityIn(densities_[static_cast<std::size_t>(insideIndex(cellX, cellZ))], cellY);
    }
    return densityIn(densities_[static_cast<std::size_t>(outsideIndex(cellX, cellZ))], cellY);
}

void BlendingData::iterateBiomes(std::int32_t minCellX, std::int32_t quartY, std::int32_t minCellZ,
                                 const BiomeConsumer& consumer) const {
    if (quartY < (minY() >> 2) || quartY > (maxY() >> 2)) {
        return;
    }
    const auto quartIndex = static_cast<std::size_t>(quartY - (minY() >> 2));
    for (std::size_t i = 0; i < biomes_.size(); ++i) {
        const std::vector<std::int32_t>& cell = biomes_[i];
        if (!cell.empty() && quartIndex < cell.size()) {
            consumer(minCellX + cellXOf(static_cast<std::int32_t>(i)), minCellZ + cellZOf(static_cast<std::int32_t>(i)),
                     cell[quartIndex]);
        }
    }
}

void BlendingData::iterateHeights(std::int32_t minCellX, std::int32_t minCellZ, const HeightConsumer& consumer) const {
    for (std::size_t i = 0; i < heights_.size(); ++i) {
        if (heights_[i] != kNoValue) {
            consumer(minCellX + cellXOf(static_cast<std::int32_t>(i)), minCellZ + cellZOf(static_cast<std::int32_t>(i)),
                     heights_[i]);
        }
    }
}

void BlendingData::iterateDensities(std::int32_t minCellX, std::int32_t minCellZ, std::int32_t fromCellY,
                                    std::int32_t toCellY, const DensityConsumer& consumer) const {
    const std::int32_t minCellY = columnMinY();
    const std::int32_t minYIndex = std::max(0, fromCellY - minCellY);
    const std::int32_t maxYIndex = std::min(cellCountPerColumn(), toCellY - minCellY);
    for (std::size_t i = 0; i < densities_.size(); ++i) {
        const std::vector<float>& column = densities_[i];
        if (column.empty()) {
            continue;
        }
        const std::int32_t testCellX = minCellX + cellXOf(static_cast<std::int32_t>(i));
        const std::int32_t testCellZ = minCellZ + cellZOf(static_cast<std::int32_t>(i));
        for (std::int32_t yIndex = minYIndex; yIndex < maxYIndex; ++yIndex) {
            consumer(testCellX, yIndex + minCellY, testCellZ, column[static_cast<std::size_t>(yIndex)] * 0.1F);
        }
    }
}

} // namespace basalt::server::world::blending
