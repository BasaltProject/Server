#include <basalt/server/world/densityfunction/DensityCache.hpp>
#include <basalt/server/world/densityfunction/DensityFunction.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <stdexcept>

namespace basalt::server::world::densityfunction {
namespace {

/// The same 26/12/26 bit layout as a block position. Every legal dimension Y fits in twelve bits.
[[nodiscard]] std::int64_t packBlockPos(std::int32_t x, std::int32_t y, std::int32_t z) {
    return ((static_cast<std::int64_t>(x) & 0x3FFFFFF) << 38) | ((static_cast<std::int64_t>(z) & 0x3FFFFFF) << 12) |
           (static_cast<std::int64_t>(y) & 0xFFF);
}

/// Floor division, which is what a negative coordinate needs: truncation would put the cell of -1
/// and of 1 on the same side of zero.
[[nodiscard]] std::int32_t floorDiv(std::int32_t value, std::int32_t divisor) {
    const std::int32_t quotient = value / divisor;
    return value % divisor != 0 && ((value < 0) != (divisor < 0)) ? quotient - 1 : quotient;
}

} // namespace

DensityCache::DensityCache(std::int32_t chunkX, std::int32_t chunkZ, std::size_t slotCount)
    : firstQuartX_(chunkX * kQuartsPerChunk), firstQuartZ_(chunkZ * kQuartsPerChunk), firstBlockX_(chunkX * 16),
      firstBlockZ_(chunkZ * 16), flatValues_(slotCount), flatPresent_(slotCount), lastSeen2d_(slotCount, 0),
      lastPos2d_(slotCount, 0), lastValue2d_(slotCount, 0.0), lastSeenOnce_(slotCount, 0), lastOnceX_(slotCount, 0),
      lastOnceY_(slotCount, 0), lastOnceZ_(slotCount, 0), lastOnceValue_(slotCount, 0.0),
      interpolatedValues_(slotCount), interpolatedPresent_(slotCount), distantValues_(slotCount),
      lastCellKey_(slotCount, 0), lastCellValid_(slotCount, 0), lastCorners_(slotCount * kCornersPerCell, 0.0) {}

void DensityCache::configureInterpolation(std::int32_t cellWidth, std::int32_t cellHeight, std::int32_t minY,
                                          std::int32_t height) {
    if (cellWidth <= 0 || cellHeight <= 0) {
        throw std::invalid_argument{"interpolation cells must have a positive size"};
    }
    if (cellWidth_ == 0) {
        cellWidth_ = cellWidth;
        cellHeight_ = cellHeight;
        interpolationMinY_ = minY;
        gridX_ = 16 / cellWidth + 1;
        gridY_ = height / cellHeight + 1;
        return;
    }
    if (cellWidth_ != cellWidth || cellHeight_ != cellHeight) {
        throw std::invalid_argument{"a density cache cannot change its cell shape once corners are in it"};
    }
}

double DensityCache::flatCache(std::size_t slot, const FunctionContext& context, const DensityFunction& function) {
    FunctionContext live = context;
    const std::int32_t gridX = (context.blockX >> 2) - firstQuartX_;
    const std::int32_t gridZ = (context.blockZ >> 2) - firstQuartZ_;
    if (slot >= flatValues_.size() || gridX < 0 || gridZ < 0 || gridX >= kGridSize || gridZ >= kGridSize) {
        return function.compute(live);
    }
    if (flatValues_[slot].empty()) {
        flatValues_[slot].assign(kGridSize * kGridSize, 0.0);
        flatPresent_[slot].assign(kGridSize * kGridSize, 0);
    }
    const auto index = static_cast<std::size_t>(gridX + gridZ * kGridSize);
    if (flatPresent_[slot][index] == 0) {
        FunctionContext quantised{(firstQuartX_ + gridX) << 2, 0, (firstQuartZ_ + gridZ) << 2, this};
        flatValues_[slot][index] = function.compute(quantised);
        flatPresent_[slot][index] = 1;
    }
    return flatValues_[slot][index];
}

double DensityCache::cache2d(std::size_t slot, const FunctionContext& context, const DensityFunction& function) {
    FunctionContext live = context;
    if (slot >= lastPos2d_.size()) {
        return function.compute(live);
    }
    const std::int64_t packed =
        (static_cast<std::int64_t>(context.blockX) & 0xFFFFFFFF) | (static_cast<std::int64_t>(context.blockZ) << 32);
    if (lastSeen2d_[slot] != 0 && lastPos2d_[slot] == packed) {
        return lastValue2d_[slot];
    }
    const double value = function.compute(live);
    lastSeen2d_[slot] = 1;
    lastPos2d_[slot] = packed;
    lastValue2d_[slot] = value;
    return value;
}

double DensityCache::cacheOnce(std::size_t slot, const FunctionContext& context, const DensityFunction& function) {
    FunctionContext live = context;
    if (slot >= lastOnceValue_.size()) {
        return function.compute(live);
    }
    if (lastSeenOnce_[slot] != 0 && lastOnceX_[slot] == context.blockX && lastOnceY_[slot] == context.blockY &&
        lastOnceZ_[slot] == context.blockZ) {
        return lastOnceValue_[slot];
    }
    const double value = function.compute(live);
    lastSeenOnce_[slot] = 1;
    lastOnceX_[slot] = context.blockX;
    lastOnceY_[slot] = context.blockY;
    lastOnceZ_[slot] = context.blockZ;
    lastOnceValue_[slot] = value;
    return value;
}

double DensityCache::interpolated(std::size_t slot, const FunctionContext& context, const DensityFunction& function) {
    FunctionContext live = context;
    if (slot >= interpolatedValues_.size() || cellWidth_ == 0 || cellHeight_ == 0) {
        return function.compute(live);
    }

    const std::int32_t minX = floorDiv(context.blockX, cellWidth_) * cellWidth_;
    const std::int32_t minY = floorDiv(context.blockY, cellHeight_) * cellHeight_;
    const std::int32_t minZ = floorDiv(context.blockZ, cellWidth_) * cellWidth_;

    // Read before the corners are filled: filling them re-enters the function, and the caller's
    // context is reused across blocks rather than rebuilt for each one.
    const double fractionX = static_cast<double>(context.blockX - minX) / cellWidth_;
    const double fractionY = static_cast<double>(context.blockY - minY) / cellHeight_;
    const double fractionZ = static_cast<double>(context.blockZ - minZ) / cellWidth_;

    // The terrain fill walks a whole cell before moving to the next, so all but one of a cell's 128
    // blocks find the eight corners already in hand and only pay the lerp.
    const std::int64_t key = packBlockPos(minX, minY, minZ);
    const std::size_t base = slot * kCornersPerCell;
    if (lastCellValid_[slot] == 0 || lastCellKey_[slot] != key) {
        lastCorners_[base] = corner(slot, function, minX, minY, minZ);
        lastCorners_[base + 1] = corner(slot, function, minX + cellWidth_, minY, minZ);
        lastCorners_[base + 2] = corner(slot, function, minX, minY + cellHeight_, minZ);
        lastCorners_[base + 3] = corner(slot, function, minX + cellWidth_, minY + cellHeight_, minZ);
        lastCorners_[base + 4] = corner(slot, function, minX, minY, minZ + cellWidth_);
        lastCorners_[base + 5] = corner(slot, function, minX + cellWidth_, minY, minZ + cellWidth_);
        lastCorners_[base + 6] = corner(slot, function, minX, minY + cellHeight_, minZ + cellWidth_);
        lastCorners_[base + 7] = corner(slot, function, minX + cellWidth_, minY + cellHeight_, minZ + cellWidth_);
        lastCellKey_[slot] = key;
        lastCellValid_[slot] = 1;
    }

    return wire::common::mth::lerp3(fractionX, fractionY, fractionZ, lastCorners_[base], lastCorners_[base + 1],
                                    lastCorners_[base + 2], lastCorners_[base + 3], lastCorners_[base + 4],
                                    lastCorners_[base + 5], lastCorners_[base + 6], lastCorners_[base + 7]);
}

double DensityCache::corner(std::size_t slot, const DensityFunction& function, std::int32_t x, std::int32_t y,
                            std::int32_t z) {
    // Corners of this chunk come off a dense grid; the aquifer also asks for positions well outside
    // it, and those keep the map.
    const std::int32_t gridX = (x - firstBlockX_) / cellWidth_;
    const std::int32_t gridY = (y - interpolationMinY_) / cellHeight_;
    const std::int32_t gridZ = (z - firstBlockZ_) / cellWidth_;
    if (gridX < 0 || gridX >= gridX_ || gridY < 0 || gridY >= gridY_ || gridZ < 0 || gridZ >= gridX_) {
        return distantCorner(slot, function, x, y, z);
    }
    const auto size =
        static_cast<std::size_t>(gridX_) * static_cast<std::size_t>(gridY_) * static_cast<std::size_t>(gridX_);
    if (interpolatedValues_[slot].empty()) {
        interpolatedValues_[slot].assign(size, 0.0);
        interpolatedPresent_[slot].assign(size, 0);
    }
    const auto index = static_cast<std::size_t>((gridX * gridY_ + gridY) * gridX_ + gridZ);
    if (interpolatedPresent_[slot][index] == 0) {
        FunctionContext at{x, y, z, this};
        interpolatedValues_[slot][index] = function.compute(at);
        interpolatedPresent_[slot][index] = 1;
    }
    return interpolatedValues_[slot][index];
}

double DensityCache::distantCorner(std::size_t slot, const DensityFunction& function, std::int32_t x, std::int32_t y,
                                   std::int32_t z) {
    const std::int64_t key = packBlockPos(x, y, z);
    const auto found = distantValues_[slot].find(key);
    if (found != distantValues_[slot].end()) {
        return found->second;
    }
    FunctionContext at{x, y, z, this};
    const double value = function.compute(at);
    distantValues_[slot].emplace(key, value);
    return value;
}

} // namespace basalt::server::world::densityfunction
