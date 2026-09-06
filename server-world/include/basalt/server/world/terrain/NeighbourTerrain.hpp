#pragma once

#include <basalt/server/world/Chunk.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace basalt::server::world::terrain {

/// parity: `level.getHeight`, the world's own map rather than the density estimate the placement
/// used to pick the spot. A piece that settles on it is placed by every chunk it crosses, so all of
/// them have to get the same answer or the piece is torn.
using WorldHeightAt = std::function<std::int32_t(std::int32_t, std::int32_t, HeightmapType)>;

/// The undecorated terrain around the chunk being decorated, so a feature reaching past the edge
/// does not take the outside for air. World coordinates throughout.
class NeighbourTerrain {
public:
    virtual ~NeighbourTerrain() = default;
    NeighbourTerrain(const NeighbourTerrain&) = delete;
    NeighbourTerrain& operator=(const NeighbourTerrain&) = delete;
    NeighbourTerrain(NeighbourTerrain&&) = delete;
    NeighbourTerrain& operator=(NeighbourTerrain&&) = delete;

    /// The pre-decoration block at a world position, or empty when that position is out of reach.
    [[nodiscard]] virtual std::optional<std::int32_t> blockStateIdAt(std::int32_t worldX, std::int32_t y,
                                                                     std::int32_t worldZ) const = 0;

    /// The `OCEAN_FLOOR` height of a column outside the chunk, or empty when out of reach.
    /// parity: an ore blob asks for every column its box covers, and a stricter preflight also skips
    /// the draws it would have made.
    [[nodiscard]] virtual std::optional<std::int32_t> oceanFloorHeightAt(std::int32_t worldX,
                                                                         std::int32_t worldZ) const = 0;

protected:
    NeighbourTerrain() = default;
};

/// Generates a neighbour the first time a feature reads into it, then keeps it. `generateTerrain`
/// must stop before decoration or reading a neighbour recurses. Reached from several threads at
/// once, so the map is behind a mutex and the generation itself happens outside it: the loser of a
/// race throws its own copy away and takes the winner's.
class NeighbourTerrainCache {
public:
    using Generate = std::function<Chunk(std::int32_t, std::int32_t)>;

    /// parity: what another chunk already wrote into this one. Vanilla puts that write in the
    /// neighbour's proto chunk, where every later read finds it; without this the reader sees
    /// undecorated terrain and places over what is already there.
    using QueuedAt = std::function<std::optional<std::int32_t>(std::int32_t, std::int32_t, std::int32_t)>;

    explicit NeighbourTerrainCache(Generate generateTerrain, std::int32_t radius = 1,
                                   std::size_t capacity = kDefaultCapacity);

    /// Installs the lookup for writes waiting on a chunk that has not generated yet.
    void setQueuedAt(QueuedAt queuedAt);

    /// A view bounded to `radius` chunks around the one being decorated. The view borrows this
    /// cache, so it must not outlive it.
    [[nodiscard]] std::unique_ptr<NeighbourTerrain> around(std::int32_t chunkX, std::int32_t chunkZ);

    /// Forgets everything held, for an owner that is done generating.
    void clear();

    [[nodiscard]] std::int32_t radius() const noexcept { return radius_; }

    /// The block, or empty when the position is out of this chunk's vertical bounds.
    [[nodiscard]] std::optional<std::int32_t> blockAt(std::int32_t neighbourX, std::int32_t neighbourZ,
                                                      std::int32_t worldX, std::int32_t y, std::int32_t worldZ);

    [[nodiscard]] std::int32_t oceanFloorAt(std::int32_t neighbourX, std::int32_t neighbourZ, std::int32_t worldX,
                                            std::int32_t worldZ);

    /// The `_WG` height of any column, generating the chunk that owns it. Unlike the bounded view,
    /// this always answers: a structure piece that settles on the mean of its footprint is placed by
    /// every chunk it crosses, and they all have to reach the same number or the piece is torn.
    [[nodiscard]] std::int32_t worldgenHeightAt(std::int32_t worldX, std::int32_t worldZ, HeightmapType type);

private:
    /// Runs `reader` on the neighbour, generating it first if this is the first read. The lock is
    /// held across the read: a reference into the map is only valid while another thread cannot be
    /// evicting the entry it points at.
    template <typename Reader>
    decltype(auto) withChunk(std::int64_t key, std::int32_t neighbourX, std::int32_t neighbourZ, Reader&& reader);

    // Wide enough that a chunk's eight neighbours survive until they generate for real: at 25 the
    // measured cost was 96 ms per chunk, about a neighbour and a half regenerated every time,
    // because concurrent generation interleaves several chunks' working sets.
    static constexpr std::size_t kDefaultCapacity = 128;

    Generate generateTerrain_;
    QueuedAt queuedAt_;
    std::int32_t radius_;
    std::size_t capacity_;
    std::mutex mutex_;
    /// Insertion order is tracked separately, so dropping the oldest key drops the least recently
    /// generated chunk rather than the numerically smallest one.
    std::map<std::int64_t, Chunk> terrain_;
    std::vector<std::int64_t> insertionOrder_;
};

} // namespace basalt::server::world::terrain
