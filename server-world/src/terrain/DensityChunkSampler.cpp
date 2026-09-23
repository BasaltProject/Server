#include <basalt/server/world/terrain/DensityChunkSampler.hpp>
#include <basalt/server/world/terrain/NoiseChunk.hpp>

#include <stdexcept>

namespace basalt::server::world::terrain {

DensityChunkSampler::DensityChunkSampler(const NoiseRouter& router, std::int32_t chunkX, std::int32_t chunkZ,
                                         densityfunction::Sampler beardifier)
    : minY_(router.minY), height_(router.height) {
    NoiseChunk noiseChunk{router, std::move(beardifier),
                          densityfunction::DensityVolume{16, router.height, 16, chunkX * 16, router.minY, chunkZ * 16}};
    values_ = noiseChunk.finalDensity();
}

double DensityChunkSampler::sample(std::int32_t localX, std::int32_t y, std::int32_t localZ) const {
    const std::int32_t relativeY = y - minY_;
    if (localX < 0 || localX > 15 || localZ < 0 || localZ > 15 || relativeY < 0 || relativeY >= height_) {
        throw std::invalid_argument{"sample position must be within the chunk's noise volume"};
    }
    return static_cast<double>(values_[static_cast<std::size_t>(relativeY + (localX + localZ * 16) * height_)]);
}

} // namespace basalt::server::world::terrain
