#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/noise/JavaMath.hpp>
#include <basalt/server/world/terrain/NoiseChunk.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace basalt::server::world::terrain {
namespace {

namespace jm = noise::javaMath;
using densityfunction::DensityBuffer;
using densityfunction::DensityVolume;

/// parity: `DimensionType.WAY_BELOW_MIN_Y`.
constexpr std::int32_t kWayBelowMinY = -32'512;
constexpr std::int64_t kNoLocation = std::numeric_limits<std::int64_t>::max();

[[nodiscard]] double similarity(std::int32_t first, std::int32_t second) {
    return 1.0 - static_cast<double>(second - first) / 25.0;
}

const double kFlowingUpdateSimilarity = similarity(100, 144);

[[nodiscard]] std::int32_t gridX(std::int32_t block) { return block >> 4; }

[[nodiscard]] std::int32_t gridY(std::int32_t block) { return jm::floorDiv(block, 12); }

[[nodiscard]] std::int32_t gridZ(std::int32_t block) { return block >> 4; }

[[nodiscard]] std::int32_t fromGridX(std::int32_t grid, std::int32_t offset) { return (grid << 4) + offset; }

[[nodiscard]] std::int32_t fromGridY(std::int32_t grid, std::int32_t offset) { return grid * 12 + offset; }

[[nodiscard]] std::int32_t fromGridZ(std::int32_t grid, std::int32_t offset) { return (grid << 4) + offset; }

/// parity: `BlockPos.asLong` and its three getters.
[[nodiscard]] std::int64_t packBlock(std::int32_t x, std::int32_t y, std::int32_t z) {
    const auto ux = static_cast<std::uint64_t>(static_cast<std::int64_t>(x)) & 0x3FFFFFFULL;
    const auto uy = static_cast<std::uint64_t>(static_cast<std::int64_t>(y)) & 0xFFFULL;
    const auto uz = static_cast<std::uint64_t>(static_cast<std::int64_t>(z)) & 0x3FFFFFFULL;
    return static_cast<std::int64_t>((ux << 38) | (uz << 12) | uy);
}

[[nodiscard]] std::int32_t unpackX(std::int64_t packed) { return static_cast<std::int32_t>(packed >> 38); }

[[nodiscard]] std::int32_t unpackY(std::int64_t packed) {
    return static_cast<std::int32_t>(static_cast<std::int64_t>(static_cast<std::uint64_t>(packed) << 52) >> 52);
}

[[nodiscard]] std::int32_t unpackZ(std::int64_t packed) {
    return static_cast<std::int32_t>(static_cast<std::int64_t>(static_cast<std::uint64_t>(packed) << 26) >> 38);
}

[[nodiscard]] std::int64_t packColumn(std::int32_t x, std::int32_t z) {
    return static_cast<std::int64_t>(static_cast<std::uint32_t>(x)) |
           static_cast<std::int64_t>(static_cast<std::uint64_t>(static_cast<std::uint32_t>(z)) << 32);
}

constexpr std::array<std::array<std::int32_t, 2>, 13> kSurfaceSamplingOffsets = {{
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

std::int32_t Aquifer::FluidStatus::at(std::int32_t y) const noexcept {
    return y < fluidLevel ? fluidType : registry::blocks::air().stateId;
}

Aquifer::FluidStatus Aquifer::globalFluid(std::int32_t y) const noexcept {
    // parity: `NoiseBasedChunkGenerator.createFluidPicker`.
    if (y < std::min(-54, router_->seaLevel)) {
        return FluidStatus{-54, registry::blocks::lava().stateId};
    }
    return FluidStatus{router_->seaLevel, router_->defaultFluid};
}

float Aquifer::sample(const densityfunction::Sampler& sampler, std::int32_t x, std::int32_t y, std::int32_t z) const {
    return sampler->sampleValue(*samplers_, x, y, z);
}

Aquifer::Aquifer(const NoiseRouter& router, densityfunction::SamplerContext& samplers, const DensityVolume& volume)
    : router_(&router), samplers_(&samplers), enabled_(router.aquifers.has_value()) {
    if (!enabled_) {
        return;
    }
    densityfunction::RandomState& state = *router.state;
    barrier_ = state.sampler(router.aquifers->barrier);
    floodedness_ = state.sampler(router.aquifers->fluidLevelFloodedness);
    spread_ = state.sampler(router.aquifers->fluidLevelSpread);
    lava_ = state.sampler(router.aquifers->lava);
    exclusion_ = state.sampler(router.aquifers->exclusion);
    surface_ = state.sampler(router.aquifers->surfaceLevel);
    random_ = state.randomFactory("minecraft:aquifer");
    minGridX_ = gridX(volume.minBlockX - 5);
    const std::int32_t maxGridX = gridX(volume.maxBlockX() - 5) + 1;
    gridSizeX_ = maxGridX - minGridX_ + 1;
    minGridY_ = gridY(volume.minBlockY + 1) - 1;
    const std::int32_t maxGridY = gridY(volume.maxBlockY() + 1) + 1;
    const std::int32_t gridSizeY = maxGridY - minGridY_ + 1;
    minGridZ_ = gridZ(volume.minBlockZ - 5);
    const std::int32_t maxGridZ = gridZ(volume.maxBlockZ() - 5) + 1;
    gridSizeZ_ = maxGridZ - minGridZ_ + 1;
    const auto total = static_cast<std::size_t>(gridSizeX_) * static_cast<std::size_t>(gridSizeY) *
                       static_cast<std::size_t>(gridSizeZ_);
    locations_.assign(total, kNoLocation);
    statuses_.assign(total, std::nullopt);
    const std::int32_t maxAdjusted = maxSurfaceLevel(fromGridX(minGridX_, 0), fromGridZ(minGridZ_, 0),
                                                     fromGridX(maxGridX, 9), fromGridZ(maxGridZ, 9)) +
                                     8;
    skipSamplingAboveY_ = fromGridY(gridY(maxAdjusted + 12) + 1, 11) - 1;
}

std::int32_t Aquifer::surfaceLevel(std::int32_t x, std::int32_t z) {
    const std::int32_t quantizedX = (x >> 2) << 2;
    const std::int32_t quantizedZ = (z >> 2) << 2;
    const std::int64_t key = packColumn(quantizedX, quantizedZ);
    if (const auto found = surfaceCache_.find(key); found != surfaceCache_.end()) {
        return found->second;
    }
    const std::int32_t level = jm::floor(sample(surface_, quantizedX, 0, quantizedZ));
    surfaceCache_.emplace(key, level);
    return level;
}

std::int32_t Aquifer::maxSurfaceLevel(std::int32_t minX, std::int32_t minZ, std::int32_t maxX, std::int32_t maxZ) {
    const std::int32_t minQuartX = minX >> 2;
    const std::int32_t maxQuartX = maxX >> 2;
    const std::int32_t minQuartZ = minZ >> 2;
    const std::int32_t maxQuartZ = maxZ >> 2;
    const DensityVolume volume{
        maxQuartX - minQuartX + 1, 1, maxQuartZ - minQuartZ + 1, minQuartX << 2, 0, minQuartZ << 2, 4, 1, 4};
    densityfunction::ScopedBuffer buffer = samplers_->acquire(volume.size());
    surface_->sampleVolume(*samplers_, buffer.buffer(), volume);
    std::int32_t maxY = std::numeric_limits<std::int32_t>::min();
    for (std::int32_t z = 0; z < volume.sizeZ; ++z) {
        for (std::int32_t x = 0; x < volume.sizeX; ++x) {
            const std::int32_t level = jm::floor(buffer.get(volume.indexUnchecked(x, 0, z)));
            surfaceCache_[packColumn(volume.blockX(x), volume.blockZ(z))] = level;
            maxY = std::max(maxY, level);
        }
    }
    return maxY;
}

std::int32_t Aquifer::index(std::int32_t x, std::int32_t y, std::int32_t z) const noexcept {
    return ((y - minGridY_) * gridSizeZ_ + (z - minGridZ_)) * gridSizeX_ + (x - minGridX_);
}

std::optional<std::int32_t> Aquifer::computeSubstance(std::int32_t x, std::int32_t y, std::int32_t z, double density) {
    if (!enabled_) {
        if (density > 0.0) {
            return std::nullopt;
        }
        return globalFluid(y).at(y);
    }
    if (density > 0.0) {
        shouldScheduleFluidUpdate_ = false;
        return std::nullopt;
    }
    const std::int32_t lava = registry::blocks::lava().stateId;
    const std::int32_t water = registry::blocks::water().stateId;
    const FluidStatus global = globalFluid(y);
    if (y > skipSamplingAboveY_) {
        shouldScheduleFluidUpdate_ = false;
        return global.at(y);
    }
    if (global.at(y) == lava) {
        shouldScheduleFluidUpdate_ = false;
        return lava;
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
    for (std::int32_t dx = 0; dx <= 1; ++dx) {
        for (std::int32_t dy = -1; dy <= 1; ++dy) {
            for (std::int32_t dz = 0; dz <= 1; ++dz) {
                const std::int32_t cellX = xAnchor + dx;
                const std::int32_t cellY = yAnchor + dy;
                const std::int32_t cellZ = zAnchor + dz;
                const std::int32_t cell = index(cellX, cellY, cellZ);
                std::int64_t& location = locations_[static_cast<std::size_t>(cell)];
                if (location == kNoLocation) {
                    random::RandomSource random = random_->at(cellX, cellY, cellZ);
                    const std::int32_t lx = fromGridX(cellX, random.nextInt(10));
                    const std::int32_t ly = fromGridY(cellY, random.nextInt(9));
                    const std::int32_t lz = fromGridZ(cellZ, random.nextInt(10));
                    location = packBlock(lx, ly, lz);
                }
                const std::int32_t deltaX = unpackX(location) - x;
                const std::int32_t deltaY = unpackY(location) - y;
                const std::int32_t deltaZ = unpackZ(location) - z;
                const std::int32_t distance = deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ;
                if (distance1 >= distance) {
                    closest4 = closest3;
                    closest3 = closest2;
                    closest2 = closest1;
                    closest1 = cell;
                    distance4 = distance3;
                    distance3 = distance2;
                    distance2 = distance1;
                    distance1 = distance;
                } else if (distance2 >= distance) {
                    closest4 = closest3;
                    closest3 = closest2;
                    closest2 = cell;
                    distance4 = distance3;
                    distance3 = distance2;
                    distance2 = distance;
                } else if (distance3 >= distance) {
                    closest4 = closest3;
                    closest3 = cell;
                    distance4 = distance3;
                    distance3 = distance;
                } else if (distance4 >= distance) {
                    closest4 = cell;
                    distance4 = distance;
                }
            }
        }
    }

    const FluidStatus status1 = status(closest1);
    const double similarity12 = similarity(distance1, distance2);
    const std::int32_t fluidState = status1.at(y);
    if (similarity12 <= 0.0) {
        shouldScheduleFluidUpdate_ = similarity12 >= kFlowingUpdateSimilarity ? !(status1 == status(closest2)) : false;
        return fluidState;
    }
    if (fluidState == water && globalFluid(y - 1).at(y - 1) == lava) {
        shouldScheduleFluidUpdate_ = true;
        return fluidState;
    }
    std::optional<double> barrier;
    const FluidStatus status2 = status(closest2);
    const double barrier12 = similarity12 * calculatePressure(x, y, z, barrier, status1, status2);
    if (density + barrier12 > 0.0) {
        shouldScheduleFluidUpdate_ = false;
        return std::nullopt;
    }
    const FluidStatus status3 = status(closest3);
    const double similarity13 = similarity(distance1, distance3);
    if (similarity13 > 0.0) {
        const double barrier13 = similarity12 * similarity13 * calculatePressure(x, y, z, barrier, status1, status3);
        if (density + barrier13 > 0.0) {
            shouldScheduleFluidUpdate_ = false;
            return std::nullopt;
        }
    }
    const double similarity23 = similarity(distance2, distance3);
    if (similarity23 > 0.0) {
        const double barrier23 = similarity12 * similarity23 * calculatePressure(x, y, z, barrier, status2, status3);
        if (density + barrier23 > 0.0) {
            shouldScheduleFluidUpdate_ = false;
            return std::nullopt;
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

double Aquifer::calculatePressure(std::int32_t x, std::int32_t y, std::int32_t z, std::optional<double>& barrier,
                                  const FluidStatus& first, const FluidStatus& second) {
    const std::int32_t lava = registry::blocks::lava().stateId;
    const std::int32_t water = registry::blocks::water().stateId;
    const std::int32_t type1 = first.at(y);
    const std::int32_t type2 = second.at(y);
    if ((type1 == lava && type2 == water) || (type1 == water && type2 == lava)) {
        return 2.0;
    }
    const std::int32_t fluidYDifference = std::abs(first.fluidLevel - second.fluidLevel);
    if (fluidYDifference == 0) {
        return 0.0;
    }
    const double average = 0.5 * static_cast<double>(first.fluidLevel + second.fluidLevel);
    const double above = static_cast<double>(y) + 0.5 - average;
    const double fromEdge = static_cast<double>(fluidYDifference) / 2.0 - std::fabs(above);
    double gradient = 0.0;
    if (above > 0.0) {
        const double center = 0.0 + fromEdge;
        gradient = center > 0.0 ? center / 1.5 : center / 2.5;
    } else {
        const double center = 3.0 + fromEdge;
        gradient = center > 0.0 ? center / 3.0 : center / 10.0;
    }
    double noise = 0.0;
    if (!(gradient < -2.0) && !(gradient > 2.0)) {
        if (!barrier.has_value()) {
            barrier = static_cast<double>(sample(barrier_, x, y, z));
        }
        noise = *barrier;
    }
    return 2.0 * (noise + gradient);
}

const Aquifer::FluidStatus& Aquifer::status(std::int32_t cell) {
    std::optional<FluidStatus>& slot = statuses_[static_cast<std::size_t>(cell)];
    if (!slot.has_value()) {
        const std::int64_t location = locations_[static_cast<std::size_t>(cell)];
        slot = computeFluid(unpackX(location), unpackY(location), unpackZ(location));
    }
    return *slot;
}

Aquifer::FluidStatus Aquifer::computeFluid(std::int32_t x, std::int32_t y, std::int32_t z) {
    const FluidStatus global = globalFluid(y);
    std::int32_t lowest = std::numeric_limits<std::int32_t>::max();
    const std::int32_t top = y + 12;
    const std::int32_t bottom = y - 12;
    bool centerUnderGlobalFluid = false;
    for (const std::array<std::int32_t, 2>& offset : kSurfaceSamplingOffsets) {
        const std::int32_t sampleX = x + offset[0] * 16;
        const std::int32_t sampleZ = z + offset[1] * 16;
        const std::int32_t level = surfaceLevel(sampleX, sampleZ);
        const std::int32_t adjusted = level + 8;
        const bool center = offset[0] == 0 && offset[1] == 0;
        if (center && bottom > adjusted) {
            return global;
        }
        const bool pokesAbove = top > adjusted;
        if (pokesAbove || center) {
            const FluidStatus atSurface = globalFluid(adjusted);
            if (!registry::blocks::isAir(atSurface.at(adjusted))) {
                if (center) {
                    centerUnderGlobalFluid = true;
                }
                if (pokesAbove) {
                    return atSurface;
                }
            }
        }
        lowest = std::min(lowest, level);
    }
    const std::int32_t fluidSurface = computeSurfaceLevel(x, y, z, global, lowest, centerUnderGlobalFluid);
    return FluidStatus{fluidSurface, computeFluidType(x, y, z, global, fluidSurface)};
}

std::int32_t Aquifer::computeSurfaceLevel(std::int32_t x, std::int32_t y, std::int32_t z, const FluidStatus& global,
                                          std::int32_t lowestSurface, bool centerUnderGlobalFluid) {
    double partially = 0.0;
    double fully = 0.0;
    if (static_cast<double>(sample(exclusion_, x, y, z)) > 0.0) {
        partially = -1.0;
        fully = -1.0;
    } else {
        const std::int32_t distanceBelowSurface = lowestSurface + 8 - y;
        double factor = 0.0;
        if (centerUnderGlobalFluid) {
            // parity: `Mth.clampedMap(distance, 0, 64, 1, 0)` in double.
            const double t = static_cast<double>(distanceBelowSurface) / 64.0;
            factor = t < 0.0 ? 1.0 : (t > 1.0 ? 0.0 : 1.0 + t * (0.0 - 1.0));
        }
        const double noise = std::clamp(static_cast<double>(sample(floodedness_, x, y, z)), -1.0, 1.0);
        const double fullyThreshold = -0.3 + ((factor - 1.0) / (0.0 - 1.0)) * (0.8 - -0.3);
        const double partiallyThreshold = -0.8 + ((factor - 1.0) / (0.0 - 1.0)) * (0.4 - -0.8);
        partially = noise - partiallyThreshold;
        fully = noise - fullyThreshold;
    }
    if (fully > 0.0) {
        return global.fluidLevel;
    }
    if (partially > 0.0) {
        const std::int32_t cellX = jm::floorDiv(x, 16);
        const std::int32_t cellY = jm::floorDiv(y, 40);
        const std::int32_t cellZ = jm::floorDiv(z, 16);
        const std::int32_t middleY = cellY * 40 + 20;
        const auto spread = static_cast<double>(sample(spread_, cellX, cellY, cellZ) * 10.0F);
        const std::int32_t quantized = jm::floor(spread / 3.0) * 3;
        return std::min(lowestSurface, middleY + quantized);
    }
    return kWayBelowMinY;
}

std::int32_t Aquifer::computeFluidType(std::int32_t x, std::int32_t y, std::int32_t z, const FluidStatus& global,
                                       std::int32_t fluidSurface) {
    const std::int32_t lava = registry::blocks::lava().stateId;
    if (fluidSurface <= -10 && fluidSurface != kWayBelowMinY && global.fluidType != lava) {
        const std::int32_t cellX = jm::floorDiv(x, 64);
        const std::int32_t cellY = jm::floorDiv(y, 40);
        const std::int32_t cellZ = jm::floorDiv(z, 64);
        if (std::fabs(static_cast<double>(sample(lava_, cellX, cellY, cellZ))) > 0.3) {
            return lava;
        }
    }
    return global.fluidType;
}

NoiseChunk::NoiseChunk(const NoiseRouter& router, densityfunction::Sampler beardifier, const DensityVolume& volume,
                       const blending::Blender* blender)
    : router_(&router), volume_(volume),
      samplers_(densityfunction::SamplerContext::Options{
          .enableCaches = true, .beardifier = std::move(beardifier), .blender = blender}),
      aquifer_(router, samplers_, volume) {}

std::vector<float> NoiseChunk::finalDensity() {
    std::vector<float> values(static_cast<std::size_t>(volume_.size()));
    DensityBuffer buffer{std::span<float>{values}};
    router_->state->sampler(router_->finalDensity)->sampleVolume(samplers_, buffer, volume_);
    return values;
}

void fillNoise(Chunk& chunk, NoiseChunk& noiseChunk, const NoiseRouter& router) {
    const DensityVolume& volume = noiseChunk.volume();
    const std::vector<float> density = noiseChunk.finalDensity();
    Aquifer& aquifer = noiseChunk.aquifer();
    const std::int32_t air = registry::blocks::air().stateId;
    for (std::int32_t z = 0; z < volume.sizeZ; ++z) {
        const std::int32_t blockZ = volume.blockZ(z);
        for (std::int32_t x = 0; x < volume.sizeX; ++x) {
            const std::int32_t blockX = volume.blockX(x);
            for (std::int32_t y = volume.sizeY - 1; y >= 0; --y) {
                const std::int32_t blockY = volume.blockY(y);
                const auto value =
                    static_cast<double>(density[static_cast<std::size_t>(volume.indexUnchecked(x, y, z))]);
                const std::int32_t state =
                    aquifer.computeSubstance(blockX, blockY, blockZ, value).value_or(router.defaultBlock);
                if (state == air) {
                    continue;
                }
                chunk.setBlockStateId(x, blockY, z, state);
                if (aquifer.shouldScheduleFluidUpdate() && registry::blockFluids::hasFluid(state)) {
                    chunk.markFluidUpdate(blockX, blockY, blockZ);
                }
            }
        }
    }
}

} // namespace basalt::server::world::terrain
