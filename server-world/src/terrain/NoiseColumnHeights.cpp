#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/terrain/DensityChunkSampler.hpp>
#include <basalt/server/world/terrain/NoiseColumnHeights.hpp>

#include <algorithm>
#include <cstdint>

namespace basalt::server::world::terrain {
namespace {

constexpr std::int32_t kCellWidth = 4;
constexpr std::int32_t kCellHeight = 8;
constexpr std::int32_t kSeaLevel = 63;

[[nodiscard]] DensityChunkSampler samplerFor(const OverworldNoiseRouter& router, std::int32_t chunkX,
                                             std::int32_t chunkZ) {
    return DensityChunkSampler{router.finalDensity,       chunkX,     chunkZ,     kOverworldHeight.minY(),
                               kOverworldHeight.height(), kCellWidth, kCellHeight};
}

/// The last solid block in the column, or one below the world when the whole column is open.
[[nodiscard]] std::int32_t topSolidY(DensityChunkSampler& sampler, std::int32_t localX, std::int32_t localZ) {
    std::int32_t y = kOverworldHeight.maxY();
    while (y >= kOverworldHeight.minY() && sampler.sample(localX, y, localZ) <= 0.0) {
        --y;
    }
    return y;
}

} // namespace

std::int32_t NoiseColumnHeights::surfaceHeight(const std::int32_t x, const std::int32_t z) const {
    DensityChunkSampler sampler = samplerFor(*router_, x >> 4, z >> 4);
    // The Overworld's global aquifer fills an open column through sea level, so the world surface
    // never reads below it even where the solid floor does.
    return std::max(topSolidY(sampler, x & 15, z & 15) + 1, kSeaLevel);
}

std::int32_t NoiseColumnHeights::oceanFloorHeight(const std::int32_t x, const std::int32_t z) const {
    DensityChunkSampler sampler = samplerFor(*router_, x >> 4, z >> 4);
    return topSolidY(sampler, x & 15, z & 15) + 1;
}

bool NoiseColumnHeights::isSolid(const std::int32_t x, const std::int32_t y, const std::int32_t z) const {
    if (y < kOverworldHeight.minY() || y > kOverworldHeight.maxY()) {
        return false;
    }
    DensityChunkSampler sampler = samplerFor(*router_, x >> 4, z >> 4);
    return sampler.sample(x & 15, y, z & 15) > 0.0;
}

} // namespace basalt::server::world::terrain
