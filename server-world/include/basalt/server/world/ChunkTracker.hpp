#pragma once

#include <cstdint>
#include <set>
#include <vector>

namespace basalt::server::world {

/// A chunk's column position, in chunk (not block) coordinates.
struct ChunkPos {
    std::int32_t x = 0;
    std::int32_t z = 0;

    friend bool operator==(const ChunkPos&, const ChunkPos&) = default;
    friend std::strong_ordering operator<=>(const ChunkPos&, const ChunkPos&) = default;
};

/// Which chunks one player should have loaded, as a load/unload delta when their view changes. The set
/// only: no packets, and not vanilla's nearest-first order, which affects speed rather than correctness.
///
/// parity: `ChunkTrackingView.Positioned`, a cylinder with the neighbour ring `contains` counts in.
class ChunkTracker {
public:
    /// Both lists sorted by x then z, not vanilla's nearest-first spiral.
    struct Delta {
        std::vector<ChunkPos> toLoad;
        std::vector<ChunkPos> toUnload;
    };

    [[nodiscard]] const std::set<ChunkPos>& loaded() const noexcept { return loadedChunks_; }

    /// Forgets everything, so the next `update` reports the whole view as newly loaded. A respawn
    /// needs this: the client discards its own level when it respawns, so chunks it was previously
    /// sent are gone on its side while this tracker still believed it had them.
    void reset() { loadedChunks_.clear(); }

    /// parity: `ChunkTrackingView.isWithinDistance`: each axis less the buffer, squared, under the
    /// view distance squared. `includeNeighbors` is the one-chunk ring the tracked set carries.
    [[nodiscard]] static bool isWithinDistance(std::int32_t centerX, std::int32_t centerZ, std::int32_t viewDistance,
                                               std::int32_t chunkX, std::int32_t chunkZ, bool includeNeighbors);

    /// Refuses a negative radius: that is a caller bug, not a view the server can serve.
    [[nodiscard]] Delta update(std::int32_t centerX, std::int32_t centerZ, std::int32_t radius);

private:
    std::set<ChunkPos> loadedChunks_;
};

} // namespace basalt::server::world
