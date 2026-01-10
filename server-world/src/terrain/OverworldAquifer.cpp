#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/terrain/OverworldAquifer.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace basalt::server::world::terrain {
namespace {

namespace mth = wire::common::mth;

// parity: DimensionType.MIN_Y << 4, where the packed 12-bit Y range starts at -2032.
constexpr std::int32_t kWayBelowMinY = -32'512;
constexpr double kFlowingUpdateSimilarity = -0.76;

/// Java's integer division rounds toward zero; vanilla wants the floor.
[[nodiscard]] std::int32_t floorDiv(std::int32_t value, std::int32_t divisor) {
    const std::int32_t quotient = value / divisor;
    return ((value % divisor != 0) && ((value < 0) != (divisor < 0))) ? quotient - 1 : quotient;
}

[[nodiscard]] std::int32_t gridX(std::int32_t block) { return block >> 4; }

[[nodiscard]] std::int32_t gridY(std::int32_t block) { return floorDiv(block, 12); }

[[nodiscard]] std::int32_t gridZ(std::int32_t block) { return block >> 4; }

[[nodiscard]] std::int32_t fromGridX(std::int32_t grid, std::int32_t offset) { return (grid << 4) + offset; }

[[nodiscard]] std::int32_t fromGridY(std::int32_t grid, std::int32_t offset) { return grid * 12 + offset; }

[[nodiscard]] std::int32_t fromGridZ(std::int32_t grid, std::int32_t offset) { return (grid << 4) + offset; }

[[nodiscard]] double similarity(std::int32_t firstDistance, std::int32_t secondDistance) {
    return 1.0 - static_cast<double>(secondDistance - firstDistance) / 25.0;
}

constexpr std::array<std::array<std::int32_t, 2>, 13> kSurfaceSamplingOffsetsInChunks = {{
    {0, 0},
    {-2, -1},
    {-1, -1},
    {0, -1},
    {1, -1},
    {-3, 0},
    {-2, 0},
    {-1, 0},
    {1, 0},
    {-2, 1},
    {-1, 1},
    {0, 1},
    {1, 1},
}};

} // namespace

const registry::BlockState* OverworldAquifer::FluidStatus::at(std::int32_t y) const {
    return y < fluidLevel ? fluidType : &registry::blocks::air();
}

OverworldAquifer::FluidStatus OverworldAquifer::globalFluid(std::int32_t y) {
    return y < -54 ? FluidStatus{-54, &registry::blocks::lava()} : FluidStatus{63, &registry::blocks::water()};
}

OverworldAquifer::OverworldAquifer(const OverworldNoiseRouter& router, std::int64_t seed, std::int32_t chunkX,
                                   std::int32_t chunkZ, std::int32_t minY, std::int32_t height,
                                   densityfunction::DensityCache* densityCache)
    : router_(router), densityCache_(densityCache),
      positionalRandomFactory_(
          random::RandomSource::xoroshiro(seed).forkPositional().fromHashOf("minecraft:aquifer").forkPositional()) {
    const std::int32_t maxGridX = gridX(chunkX * 16 + 15 - 5) + 1;
    const std::int32_t maxGridY = gridY(minY + height + 1) + 1;
    const std::int32_t maxGridZ = gridZ(chunkZ * 16 + 15 - 5) + 1;
    minGridX_ = gridX(chunkX * 16 - 5);
    minGridY_ = gridY(minY + 1) - 1;
    minGridZ_ = gridZ(chunkZ * 16 - 5);
    gridSizeX_ = maxGridX - minGridX_ + 1;
    gridSizeZ_ = maxGridZ - minGridZ_ + 1;
    locationCache_.resize(static_cast<std::size_t>(gridSizeX_) * static_cast<std::size_t>(maxGridY - minGridY_ + 1) *
                          static_cast<std::size_t>(gridSizeZ_));
    statusCache_.resize(locationCache_.size());

    const std::int32_t maxSurface = maxPreliminarySurfaceLevel(fromGridX(minGridX_, 0), fromGridZ(minGridZ_, 0),
                                                               fromGridX(maxGridX, 9), fromGridZ(maxGridZ, 9)) +
                                    8;
    const std::int32_t skipSamplingAboveGridY = gridY(maxSurface + 12) + 1;
    skipSamplingAboveY_ = fromGridY(skipSamplingAboveGridY, 11) - 1;
}

const registry::BlockState* OverworldAquifer::computeSubstance(std::int32_t x, std::int32_t y, std::int32_t z,
                                                               double density) {
    if (density > 0.0) {
        shouldScheduleFluidUpdate_ = false;
        return nullptr;
    }

    const FluidStatus fluid = globalFluid(y);
    if (y > skipSamplingAboveY_) {
        shouldScheduleFluidUpdate_ = false;
        return fluid.at(y);
    }
    if (fluid.at(y) == &registry::blocks::lava()) {
        shouldScheduleFluidUpdate_ = false;
        return &registry::blocks::lava();
    }

    const std::int32_t xAnchor = gridX(x - 5);
    const std::int32_t yAnchor = gridY(y + 1);
    const std::int32_t zAnchor = gridZ(z - 5);
    std::int32_t distance1 = std::numeric_limits<std::int32_t>::max();
    std::int32_t distance2 = distance1;
    std::int32_t distance3 = distance1;
    std::int32_t distance4 = distance1;
    std::int32_t closest1 = 0;
    std::int32_t closest2 = 0;
    std::int32_t closest3 = 0;
    std::int32_t closest4 = 0;

    for (std::int32_t dxCell = 0; dxCell <= 1; ++dxCell) {
        for (std::int32_t dyCell = -1; dyCell <= 1; ++dyCell) {
            for (std::int32_t dzCell = 0; dzCell <= 1; ++dzCell) {
                const std::int32_t cellX = xAnchor + dxCell;
                const std::int32_t cellY = yAnchor + dyCell;
                const std::int32_t cellZ = zAnchor + dzCell;
                const std::int32_t cellIndex = index(cellX, cellY, cellZ);
                std::optional<AquiferPosition>& slot = locationCache_[static_cast<std::size_t>(cellIndex)];
                if (!slot.has_value()) {
                    random::RandomSource random = positionalRandomFactory_.at(cellX, cellY, cellZ);
                    slot = AquiferPosition{fromGridX(cellX, random.nextInt(10)), fromGridY(cellY, random.nextInt(9)),
                                           fromGridZ(cellZ, random.nextInt(10))};
                }
                const std::int32_t deltaX = slot->x - x;
                const std::int32_t deltaY = slot->y - y;
                const std::int32_t deltaZ = slot->z - z;
                const std::int32_t distance = deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ;
                if (distance1 >= distance) {
                    closest4 = closest3;
                    closest3 = closest2;
                    closest2 = closest1;
                    closest1 = cellIndex;
                    distance4 = distance3;
                    distance3 = distance2;
                    distance2 = distance1;
                    distance1 = distance;
                } else if (distance2 >= distance) {
                    closest4 = closest3;
                    closest3 = closest2;
                    closest2 = cellIndex;
                    distance4 = distance3;
                    distance3 = distance2;
                    distance2 = distance;
                } else if (distance3 >= distance) {
                    closest4 = closest3;
                    closest3 = cellIndex;
                    distance4 = distance3;
                    distance3 = distance;
                } else if (distance4 >= distance) {
                    closest4 = cellIndex;
                    distance4 = distance;
                }
            }
        }
    }

    const FluidStatus status1 = status(closest1);
    const double similarity12 = similarity(distance1, distance2);
    const registry::BlockState* fluidState = status1.at(y);
    if (similarity12 <= 0.0) {
        shouldScheduleFluidUpdate_ = similarity12 >= kFlowingUpdateSimilarity ? !(status1 == status(closest2)) : false;
        return fluidState;
    }
    if (fluidState == &registry::blocks::water() && globalFluid(y - 1).at(y - 1) == &registry::blocks::lava()) {
        shouldScheduleFluidUpdate_ = true;
        return fluidState;
    }

    densityfunction::FunctionContext context{x, y, z, densityCache_};
    std::optional<double> barrierNoiseValue;
    const FluidStatus status2 = status(closest2);
    const double barrier12 = similarity12 * calculatePressure(context, barrierNoiseValue, status1, status2);
    if (density + barrier12 > 0.0) {
        shouldScheduleFluidUpdate_ = false;
        return nullptr;
    }

    const FluidStatus status3 = status(closest3);
    const double similarity13 = similarity(distance1, distance3);
    if (similarity13 > 0.0) {
        const double barrier13 =
            similarity12 * similarity13 * calculatePressure(context, barrierNoiseValue, status1, status3);
        if (density + barrier13 > 0.0) {
            shouldScheduleFluidUpdate_ = false;
            return nullptr;
        }
    }

    const double similarity23 = similarity(distance2, distance3);
    if (similarity23 > 0.0) {
        const double barrier23 =
            similarity12 * similarity23 * calculatePressure(context, barrierNoiseValue, status2, status3);
        if (density + barrier23 > 0.0) {
            shouldScheduleFluidUpdate_ = false;
            return nullptr;
        }
    }

    const bool mayFlow12 = !(status1 == status2);
    const bool mayFlow23 = similarity23 >= kFlowingUpdateSimilarity && !(status2 == status3);
    const bool mayFlow13 = similarity13 >= kFlowingUpdateSimilarity && !(status1 == status3);
    if (!mayFlow12 && !mayFlow23 && !mayFlow13) {
        shouldScheduleFluidUpdate_ = similarity13 >= kFlowingUpdateSimilarity &&
                                     similarity(distance1, distance4) >= kFlowingUpdateSimilarity &&
                                     !(status1 == status(closest4));
    } else {
        shouldScheduleFluidUpdate_ = true;
    }
    return fluidState;
}

double OverworldAquifer::calculatePressure(densityfunction::FunctionContext& context,
                                           std::optional<double>& barrierNoiseValue, const FluidStatus& first,
                                           const FluidStatus& second) const {
    const std::int32_t y = context.blockY;
    const registry::BlockState* firstType = first.at(y);
    const registry::BlockState* secondType = second.at(y);
    const registry::BlockState* lava = &registry::blocks::lava();
    const registry::BlockState* water = &registry::blocks::water();
    if ((firstType == lava && secondType == water) || (firstType == water && secondType == lava)) {
        return 2.0;
    }

    const std::int32_t fluidYDifference = std::abs(first.fluidLevel - second.fluidLevel);
    if (fluidYDifference == 0) {
        return 0.0;
    }
    const double averageFluidY = 0.5 * (static_cast<double>(first.fluidLevel) + static_cast<double>(second.fluidLevel));
    const double distanceAboveAverage = static_cast<double>(y) + 0.5 - averageFluidY;
    const double distanceFromEdge = static_cast<double>(fluidYDifference) / 2.0 - std::abs(distanceAboveAverage);
    double gradient = 0.0;
    if (distanceAboveAverage > 0.0) {
        gradient = distanceFromEdge > 0.0 ? distanceFromEdge / 1.5 : distanceFromEdge / 2.5;
    } else {
        const double centerPoint = 3.0 + distanceFromEdge;
        gradient = centerPoint > 0.0 ? centerPoint / 3.0 : centerPoint / 10.0;
    }
    double noiseValue = 0.0;
    if (gradient >= -2.0 && gradient <= 2.0) {
        if (!barrierNoiseValue.has_value()) {
            barrierNoiseValue = router_.barrierNoise->compute(context);
        }
        noiseValue = *barrierNoiseValue;
    }
    return 2.0 * (noiseValue + gradient);
}

const OverworldAquifer::FluidStatus& OverworldAquifer::status(std::int32_t cellIndex) {
    std::optional<FluidStatus>& slot = statusCache_[static_cast<std::size_t>(cellIndex)];
    if (!slot.has_value()) {
        const AquiferPosition& location = *locationCache_[static_cast<std::size_t>(cellIndex)];
        slot = computeFluid(location.x, location.y, location.z);
    }
    return *slot;
}

OverworldAquifer::FluidStatus OverworldAquifer::computeFluid(std::int32_t x, std::int32_t y, std::int32_t z) {
    const FluidStatus fluid = globalFluid(y);
    std::int32_t lowestPreliminarySurface = std::numeric_limits<std::int32_t>::max();
    const std::int32_t topOfAquiferCell = y + 12;
    const std::int32_t bottomOfAquiferCell = y - 12;
    bool surfaceAtCenterIsUnderGlobalFluid = false;

    for (const std::array<std::int32_t, 2>& offset : kSurfaceSamplingOffsetsInChunks) {
        const std::int32_t sampleX = x + offset[0] * 16;
        const std::int32_t sampleZ = z + offset[1] * 16;
        const std::int32_t preliminarySurface = preliminarySurfaceLevel(sampleX, sampleZ);
        const std::int32_t adjustedSurface = preliminarySurface + 8;
        const bool center = offset[0] == 0 && offset[1] == 0;
        if (center && bottomOfAquiferCell > adjustedSurface) {
            return fluid;
        }

        const bool pokesAboveSurface = topOfAquiferCell > adjustedSurface;
        if (pokesAboveSurface || center) {
            const FluidStatus fluidAtSurface = globalFluid(adjustedSurface);
            if (fluidAtSurface.at(adjustedSurface) != &registry::blocks::air()) {
                if (center) {
                    surfaceAtCenterIsUnderGlobalFluid = true;
                }
                if (pokesAboveSurface) {
                    return fluidAtSurface;
                }
            }
        }
        lowestPreliminarySurface = std::min(lowestPreliminarySurface, preliminarySurface);
    }

    const std::int32_t fluidSurface =
        computeSurfaceLevel(x, y, z, fluid, lowestPreliminarySurface, surfaceAtCenterIsUnderGlobalFluid);
    return FluidStatus{fluidSurface, computeFluidType(x, y, z, fluid, fluidSurface)};
}

std::int32_t OverworldAquifer::computeSurfaceLevel(std::int32_t x, std::int32_t y, std::int32_t z,
                                                   const FluidStatus& fluid, std::int32_t lowestPreliminarySurface,
                                                   bool surfaceAtCenterIsUnderGlobalFluid) const {
    densityfunction::FunctionContext context{x, y, z, densityCache_};
    double partiallyFloodedness = 0.0;
    double fullyFloodedness = 0.0;
    // parity: vanilla compares the double against float literals, which widen rather than truncate.
    if (router_.erosion->compute(context) < static_cast<double>(-0.225F) &&
        router_.depth->compute(context) > static_cast<double>(0.9F)) {
        partiallyFloodedness = -1.0;
        fullyFloodedness = -1.0;
    } else {
        const std::int32_t distanceBelowSurface = lowestPreliminarySurface + 8 - y;
        const double floodednessFactor =
            surfaceAtCenterIsUnderGlobalFluid
                ? mth::clampedMap(static_cast<double>(distanceBelowSurface), 0.0, 64.0, 1.0, 0.0)
                : 0.0;
        const double floodednessNoise = mth::clamp(router_.fluidLevelFloodednessNoise->compute(context), -1.0, 1.0);
        const double fullyFloodedThreshold = mth::map(floodednessFactor, 1.0, 0.0, -0.3, 0.8);
        const double partiallyFloodedThreshold = mth::map(floodednessFactor, 1.0, 0.0, -0.8, 0.4);
        partiallyFloodedness = floodednessNoise - partiallyFloodedThreshold;
        fullyFloodedness = floodednessNoise - fullyFloodedThreshold;
    }
    if (fullyFloodedness > 0.0) {
        return fluid.fluidLevel;
    }
    if (partiallyFloodedness > 0.0) {
        return randomizedFluidSurfaceLevel(x, y, z, lowestPreliminarySurface);
    }
    return kWayBelowMinY;
}

std::int32_t OverworldAquifer::randomizedFluidSurfaceLevel(std::int32_t x, std::int32_t y, std::int32_t z,
                                                           std::int32_t lowestPreliminarySurface) const {
    const std::int32_t cellX = floorDiv(x, 16);
    const std::int32_t cellY = floorDiv(y, 40);
    const std::int32_t cellZ = floorDiv(z, 16);
    const std::int32_t cellMiddleY = cellY * 40 + 20;
    densityfunction::FunctionContext context{cellX, cellY, cellZ, densityCache_};
    const double spread = router_.fluidLevelSpreadNoise->compute(context) * 10.0;
    const std::int32_t quantizedSpread = mth::floor(spread / 3.0) * 3;
    return std::min(lowestPreliminarySurface, cellMiddleY + quantizedSpread);
}

const registry::BlockState* OverworldAquifer::computeFluidType(std::int32_t x, std::int32_t y, std::int32_t z,
                                                               const FluidStatus& fluid,
                                                               std::int32_t fluidSurface) const {
    if (fluidSurface <= -10 && fluidSurface != kWayBelowMinY && fluid.fluidType != &registry::blocks::lava()) {
        const std::int32_t cellX = floorDiv(x, 64);
        const std::int32_t cellY = floorDiv(y, 40);
        const std::int32_t cellZ = floorDiv(z, 64);
        densityfunction::FunctionContext context{cellX, cellY, cellZ, densityCache_};
        if (std::abs(router_.lavaNoise->compute(context)) > 0.3) {
            return &registry::blocks::lava();
        }
    }
    return fluid.fluidType;
}

std::int32_t OverworldAquifer::preliminarySurfaceLevel(std::int32_t x, std::int32_t z) {
    const std::int32_t quantizedX = (x >> 2) << 2;
    const std::int32_t quantizedZ = (z >> 2) << 2;
    const std::int64_t key =
        (static_cast<std::int64_t>(quantizedX) & 0xFFFFFFFFLL) | (static_cast<std::int64_t>(quantizedZ) << 32);
    const auto found = preliminarySurfaceCache_.find(key);
    if (found != preliminarySurfaceCache_.end()) {
        return found->second;
    }
    densityfunction::FunctionContext context{quantizedX, 0, quantizedZ, densityCache_};
    const std::int32_t level = mth::floor(router_.preliminarySurfaceLevel->compute(context));
    preliminarySurfaceCache_.emplace(key, level);
    return level;
}

std::int32_t OverworldAquifer::maxPreliminarySurfaceLevel(std::int32_t minX, std::int32_t minZ, std::int32_t maxX,
                                                          std::int32_t maxZ) {
    std::int32_t maximum = std::numeric_limits<std::int32_t>::min();
    for (std::int32_t z = minZ; z <= maxZ; z += 4) {
        for (std::int32_t x = minX; x <= maxX; x += 4) {
            maximum = std::max(maximum, preliminarySurfaceLevel(x, z));
        }
    }
    return maximum;
}

std::int32_t OverworldAquifer::index(std::int32_t x, std::int32_t y, std::int32_t z) const {
    return ((y - minGridY_) * gridSizeZ_ + (z - minGridZ_)) * gridSizeX_ + (x - minGridX_);
}

} // namespace basalt::server::world::terrain
