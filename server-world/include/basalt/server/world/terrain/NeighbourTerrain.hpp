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

/// parity: `level.getHeight`.
using WorldHeightAt = std::function<std::int32_t(std::int32_t, std::int32_t, HeightmapType)>;

class NeighbourTerrain {
public:
    virtual ~NeighbourTerrain() = default;
    NeighbourTerrain(const NeighbourTerrain&) = delete;
    NeighbourTerrain& operator=(const NeighbourTerrain&) = delete;
    NeighbourTerrain(NeighbourTerrain&&) = delete;
    NeighbourTerrain& operator=(NeighbourTerrain&&) = delete;

    [[nodiscard]] virtual std::optional<std::int32_t> blockStateIdAt(std::int32_t worldX, std::int32_t y,
                                                                     std::int32_t worldZ) const = 0;

    /// parity: an ore blob asks for every column its box covers, and a stricter preflight also skips.
    [[nodiscard]] virtual std::optional<std::int32_t> oceanFloorHeightAt(std::int32_t worldX,
                                                                         std::int32_t worldZ) const = 0;

protected:
    NeighbourTerrain() = default;
};

class NeighbourTerrainCache {
public:
    using Generate = std::function<Chunk(std::int32_t, std::int32_t)>;

    explicit NeighbourTerrainCache(Generate generateTerrain, std::int32_t radius = 1,
                                   std::size_t capacity = kDefaultCapacity);

    [[nodiscard]] std::unique_ptr<NeighbourTerrain> around(std::int32_t chunkX, std::int32_t chunkZ);

    void clear();

    [[nodiscard]] std::int32_t radius() const noexcept { return radius_; }

    [[nodiscard]] std::optional<std::int32_t> blockAt(std::int32_t neighbourX, std::int32_t neighbourZ,
                                                      std::int32_t worldX, std::int32_t y, std::int32_t worldZ);

    [[nodiscard]] std::int32_t oceanFloorAt(std::int32_t neighbourX, std::int32_t neighbourZ, std::int32_t worldX,
                                            std::int32_t worldZ);

    [[nodiscard]] std::int32_t worldgenHeightAt(std::int32_t worldX, std::int32_t worldZ, HeightmapType type);

private:
    template <typename Reader>
    decltype(auto) withChunk(std::int64_t key, std::int32_t neighbourX, std::int32_t neighbourZ, Reader&& reader);

    static constexpr std::size_t kDefaultCapacity = 128;

    Generate generateTerrain_;
    std::int32_t radius_;
    std::size_t capacity_;
    std::mutex mutex_;
    std::map<std::int64_t, Chunk> terrain_;
    std::vector<std::int64_t> insertionOrder_;
};

} // namespace basalt::server::world::terrain
