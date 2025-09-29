#include <basalt/server/world/terrain/DensityChunkSampler.hpp>

#include <stdexcept>
#include <string>

namespace basalt::server::world::terrain {

DensityChunkSampler::DensityChunkSampler(densityfunction::Function density, const std::int32_t chunkX,
                                         const std::int32_t chunkZ, const std::int32_t minY, const std::int32_t height,
                                         const std::int32_t cellWidth, const std::int32_t cellHeight,
                                         densityfunction::DensityCache* cache)
    : density_(std::move(density)), chunkX_(chunkX), chunkZ_(chunkZ), minY_(minY), height_(height),
      cellWidth_(cellWidth), cellHeight_(cellHeight) {
    if (cellWidth <= 0 || 16 % cellWidth != 0) {
        throw std::invalid_argument{"cellWidth must evenly divide 16, got " + std::to_string(cellWidth)};
    }
    if (cellHeight <= 0 || height % cellHeight != 0) {
        throw std::invalid_argument{"cellHeight must evenly divide height, got " + std::to_string(cellHeight)};
    }
    if (cache == nullptr) {
        ownedCache_ =
            std::make_unique<densityfunction::DensityCache>(chunkX, chunkZ, densityfunction::markerSlotCount());
        cache = ownedCache_.get();
    }
    cache->configureInterpolation(cellWidth, cellHeight, minY, height);
    context_ = densityfunction::FunctionContext{.blockX = 0, .blockY = 0, .blockZ = 0, .cache = cache};
}

double DensityChunkSampler::sample(const std::int32_t localX, const std::int32_t y, const std::int32_t localZ) {
    const std::int32_t relativeY = y - minY_;
    if (localX < 0 || localX > 15 || localZ < 0 || localZ > 15) {
        throw std::invalid_argument{"sample position must be within a chunk"};
    }
    if (relativeY < 0 || relativeY >= height_) {
        throw std::invalid_argument{"y must be within the sampler's own column"};
    }
    context_.moveTo(chunkX_ * 16 + localX, y, chunkZ_ * 16 + localZ);
    return density_->compute(context_);
}

} // namespace basalt::server::world::terrain
