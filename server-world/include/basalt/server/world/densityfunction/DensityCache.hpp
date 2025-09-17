#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace basalt::server::world::densityfunction {

class DensityFunction;
struct FunctionContext;

/// parity: `NoiseChunk`'s flat cache, 2D cache, cache-once and interpolator. Vanilla rewrites the
/// router tree per chunk; the router here is immutable and shared, so the mutable state lives here
/// and travels down inside the context. One instance, one chunk, one thread, so no lock.
///
/// The flat cache is not memoisation: vanilla quantises to the quart grid and samples at y zero, and
/// reproducing that quantisation is the point.
class DensityCache {
public:
    DensityCache(std::int32_t chunkX, std::int32_t chunkZ, std::size_t slotCount);

    /// Fixes the noise cell shape. Called once; asking for a different shape afterwards is a caller
    /// bug, since the corners already cached were computed against the first one.
    void configureInterpolation(std::int32_t cellWidth, std::int32_t cellHeight, std::int32_t minY,
                                std::int32_t height);

    /// parity: `NoiseChunk.FlatCache.compute`. Out-of-range positions fall through to the function
    /// at the real context, exactly as vanilla's own bounds check does. Vanilla fills its grid up
    /// front; this fills each cell on first use, which computes strictly fewer values and the same
    /// ones.
    [[nodiscard]] double flatCache(std::size_t slot, const FunctionContext& context, const DensityFunction& function);

    /// parity: `NoiseChunk.Cache2D`. Exact last-position memoisation, no quantisation.
    [[nodiscard]] double cache2d(std::size_t slot, const FunctionContext& context, const DensityFunction& function);

    /// parity: `NoiseChunk.CacheOnce`. Vanilla keys it on an interpolation counter it bumps once per
    /// sampled position; the position itself is the same identity, and the function is pure in it.
    /// The overworld's sloped cheese is referenced three times in the cave composition, so without
    /// this its whole subtree is evaluated three times per corner.
    [[nodiscard]] double cacheOnce(std::size_t slot, const FunctionContext& context, const DensityFunction& function);

    /// parity: `NoiseChunk.Interpolator`. Only this function is sampled at the noise cell's corners;
    /// callers go on evaluating every node outside its marker at the real block.
    [[nodiscard]] double interpolated(std::size_t slot, const FunctionContext& context,
                                      const DensityFunction& function);

private:
    static constexpr std::int32_t kQuartsPerChunk = 4;
    static constexpr std::size_t kCornersPerCell = 8;
    /// parity: `NoiseChunk`'s own `noiseSizeXZ + 1`, with the real overworld cell width of four,
    /// which makes the noise grid and the quart grid the same thing.
    static constexpr std::int32_t kGridSize = kQuartsPerChunk + 1;

    [[nodiscard]] double corner(std::size_t slot, const DensityFunction& function, std::int32_t x, std::int32_t y,
                                std::int32_t z);

    [[nodiscard]] double distantCorner(std::size_t slot, const DensityFunction& function, std::int32_t x,
                                       std::int32_t y, std::int32_t z);

    std::int32_t firstQuartX_;
    std::int32_t firstQuartZ_;
    std::int32_t firstBlockX_;
    std::int32_t firstBlockZ_;

    std::vector<std::vector<double>> flatValues_;
    std::vector<std::vector<char>> flatPresent_;

    std::vector<char> lastSeen2d_;
    std::vector<std::int64_t> lastPos2d_;
    std::vector<double> lastValue2d_;

    std::vector<char> lastSeenOnce_;
    std::vector<std::int32_t> lastOnceX_;
    std::vector<std::int32_t> lastOnceY_;
    std::vector<std::int32_t> lastOnceZ_;
    std::vector<double> lastOnceValue_;

    std::vector<std::vector<double>> interpolatedValues_;
    std::vector<std::vector<char>> interpolatedPresent_;
    std::vector<std::unordered_map<std::int64_t, double>> distantValues_;
    std::vector<std::int64_t> lastCellKey_;
    std::vector<char> lastCellValid_;
    std::vector<double> lastCorners_;

    std::int32_t cellWidth_ = 0;
    std::int32_t cellHeight_ = 0;
    std::int32_t interpolationMinY_ = 0;
    std::int32_t gridX_ = 0;
    std::int32_t gridY_ = 0;
};

} // namespace basalt::server::world::densityfunction
