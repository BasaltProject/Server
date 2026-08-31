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

/// Which chunks one player should have loaded, as a load and unload delta whenever their view
/// centre or distance changes. It decides the set only: no packets, no chunk data, and not
/// vanilla's nearest-first spiral, which affects how fast terrain appears rather than correctness.
///
/// The set is a square, Chebyshev distance at most `radius`, as `SetChunkCacheRadiusPacket`
/// declares it. Vanilla's circular area with its ticket falloff is a documented gap.
class ChunkTracker {
public:
    /// Both lists come out sorted by x then z. The frozen Kotlin emitted them in the order its
    /// insertion-ordered set happened to hold, which was an artifact of that container rather than
    /// anything vanilla promises: vanilla sends a nearest-first spiral, which neither does.
    struct Delta {
        std::vector<ChunkPos> toLoad;
        std::vector<ChunkPos> toUnload;
    };

    [[nodiscard]] const std::set<ChunkPos>& loaded() const noexcept { return loadedChunks_; }

    /// Forgets everything, so the next `update` reports the whole view as newly loaded. A respawn
    /// needs this: the client discards its own level when it respawns, so chunks it was previously
    /// sent are gone on its side while this tracker still believed it had them.
    void reset() { loadedChunks_.clear(); }

    /// Refuses a negative radius: that is a caller bug, not a view the server can serve.
    [[nodiscard]] Delta update(std::int32_t centerX, std::int32_t centerZ, std::int32_t radius);

private:
    std::set<ChunkPos> loadedChunks_;
};

} // namespace basalt::server::world
