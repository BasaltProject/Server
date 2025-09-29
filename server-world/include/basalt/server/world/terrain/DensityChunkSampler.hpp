#pragma once

#include <basalt/server/world/densityfunction/DensityCache.hpp>
#include <basalt/server/world/densityfunction/DensityFunction.hpp>

#include <cstdint>
#include <memory>
#include <utility>

namespace basalt::server::world::terrain {

/// parity: `NoiseChunk`. Explicit interpolation markers sample their own wrapped subtrees at cell
/// corners through the cache; nodes outside those markers are evaluated at the real block. The
/// distinction matters for nonlinear post-processing such as squeeze and for cave composition.
///
/// One instance covers one chunk column, `minY` up to `minY + height - 1`. `cellWidth` must divide
/// 16 and `cellHeight` must divide `height`, as the overworld settings do.
class DensityChunkSampler {
public:
    DensityChunkSampler(densityfunction::Function density, std::int32_t chunkX, std::int32_t chunkZ, std::int32_t minY,
                        std::int32_t height, std::int32_t cellWidth, std::int32_t cellHeight,
                        densityfunction::DensityCache* cache = nullptr);

    /// Cell counts, so a caller can iterate the same grid this samples from.
    [[nodiscard]] std::int32_t horizontalCells() const noexcept { return 16 / cellWidth_; }

    [[nodiscard]] std::int32_t verticalCells() const noexcept { return height_ / cellHeight_; }

    /// `localX` and `localZ` are chunk-local; `y` is a world y within `[minY, minY + height)`.
    [[nodiscard]] double sample(std::int32_t localX, std::int32_t y, std::int32_t localZ);

private:
    densityfunction::Function density_;
    std::int32_t chunkX_;
    std::int32_t chunkZ_;
    std::int32_t minY_;
    std::int32_t height_;
    std::int32_t cellWidth_;
    std::int32_t cellHeight_;
    std::unique_ptr<densityfunction::DensityCache> ownedCache_;
    // One context for the whole chunk: building one per block was 98304 allocations a chunk on the
    // one path that runs for every block there is.
    densityfunction::FunctionContext context_;
};

} // namespace basalt::server::world::terrain
