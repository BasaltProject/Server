#pragma once

#include <cstdint>
#include <set>
#include <vector>

namespace basalt::server::world {

struct ChunkPos {
    std::int32_t x = 0;
    std::int32_t z = 0;

    friend bool operator==(const ChunkPos&, const ChunkPos&) = default;
    friend std::strong_ordering operator<=>(const ChunkPos&, const ChunkPos&) = default;
};

/// parity: `ChunkTrackingView.Positioned`.
class ChunkTracker {
public:
    struct Delta {
        std::vector<ChunkPos> toLoad;
        std::vector<ChunkPos> toUnload;
    };

    [[nodiscard]] const std::set<ChunkPos>& loaded() const noexcept { return loadedChunks_; }

    void reset() { loadedChunks_.clear(); }

    /// parity: `ChunkTrackingView.isWithinDistance`.
    [[nodiscard]] static bool isWithinDistance(std::int32_t centerX, std::int32_t centerZ, std::int32_t viewDistance,
                                               std::int32_t chunkX, std::int32_t chunkZ, bool includeNeighbors);

    [[nodiscard]] Delta update(std::int32_t centerX, std::int32_t centerZ, std::int32_t radius);

private:
    std::set<ChunkPos> loadedChunks_;
};

} // namespace basalt::server::world
