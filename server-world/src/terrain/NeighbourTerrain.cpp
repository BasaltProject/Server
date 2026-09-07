#include <basalt/server/world/GenerationMetrics.hpp>
#include <basalt/server/world/terrain/NeighbourTerrain.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <utility>

namespace basalt::server::world::terrain {
namespace {

[[nodiscard]] std::int64_t keyOf(std::int32_t chunkX, std::int32_t chunkZ) {
    return (static_cast<std::int64_t>(chunkX) << 32) | (static_cast<std::int64_t>(chunkZ) & 0xFFFFFFFFLL);
}

/// The view handed to one decorating chunk: it only answers for positions within `radius`.
class BoundedView final : public NeighbourTerrain {
public:
    BoundedView(NeighbourTerrainCache& cache, std::int32_t chunkX, std::int32_t chunkZ)
        : cache_(&cache), chunkX_(chunkX), chunkZ_(chunkZ) {}

    [[nodiscard]] std::optional<std::int32_t> blockStateIdAt(std::int32_t worldX, std::int32_t y,
                                                             std::int32_t worldZ) const override {
        const std::optional<std::pair<std::int32_t, std::int32_t>> reach = inReach(worldX, worldZ);
        if (!reach.has_value()) {
            return std::nullopt;
        }
        return cache_->blockAt(reach->first, reach->second, worldX, y, worldZ);
    }

    [[nodiscard]] std::optional<std::int32_t> oceanFloorHeightAt(std::int32_t worldX,
                                                                 std::int32_t worldZ) const override {
        const std::optional<std::pair<std::int32_t, std::int32_t>> reach = inReach(worldX, worldZ);
        if (!reach.has_value()) {
            return std::nullopt;
        }
        return cache_->oceanFloorAt(reach->first, reach->second, worldX, worldZ);
    }

private:
    [[nodiscard]] std::optional<std::pair<std::int32_t, std::int32_t>> inReach(std::int32_t worldX,
                                                                               std::int32_t worldZ) const {
        const std::int32_t neighbourX = worldX >> 4;
        const std::int32_t neighbourZ = worldZ >> 4;
        if (std::max(std::abs(neighbourX - chunkX_), std::abs(neighbourZ - chunkZ_)) > cache_->radius()) {
            return std::nullopt;
        }
        return std::pair{neighbourX, neighbourZ};
    }

    NeighbourTerrainCache* cache_;
    std::int32_t chunkX_;
    std::int32_t chunkZ_;
};

} // namespace

NeighbourTerrainCache::NeighbourTerrainCache(Generate generateTerrain, const std::int32_t radius,
                                             const std::size_t capacity)
    : generateTerrain_(std::move(generateTerrain)), radius_(radius), capacity_(capacity) {}

std::unique_ptr<NeighbourTerrain> NeighbourTerrainCache::around(const std::int32_t chunkX, const std::int32_t chunkZ) {
    return std::make_unique<BoundedView>(*this, chunkX, chunkZ);
}

void NeighbourTerrainCache::clear() {
    const std::scoped_lock lock{mutex_};
    terrain_.clear();
    insertionOrder_.clear();
}

template <typename Reader>
decltype(auto) NeighbourTerrainCache::withChunk(const std::int64_t key, const std::int32_t neighbourX,
                                                const std::int32_t neighbourZ, Reader&& reader) {
    {
        const std::scoped_lock lock{mutex_};
        if (const auto found = terrain_.find(key); found != terrain_.end()) {
            return reader(found->second);
        }
    }
    // Generated outside the lock, so one slow neighbour does not stall every other decorating chunk.
    // The loser of a race throws its own copy away and reads the winner's.
    Chunk generated = generationMetrics::measure(generationMetrics::Phase::Neighbours,
                                                 [&] { return generateTerrain_(neighbourX, neighbourZ); });
    const std::scoped_lock lock{mutex_};
    auto found = terrain_.find(key);
    if (found == terrain_.end()) {
        if (terrain_.size() >= capacity_ && !insertionOrder_.empty()) {
            terrain_.erase(insertionOrder_.front());
            insertionOrder_.erase(insertionOrder_.begin());
        }
        insertionOrder_.push_back(key);
        found = terrain_.emplace(key, std::move(generated)).first;
    }
    return reader(found->second);
}

void NeighbourTerrainCache::setQueuedAt(QueuedAt queuedAt) { queuedAt_ = std::move(queuedAt); }

std::optional<std::int32_t> NeighbourTerrainCache::blockAt(const std::int32_t neighbourX, const std::int32_t neighbourZ,
                                                           const std::int32_t worldX, const std::int32_t y,
                                                           const std::int32_t worldZ) {
    if (queuedAt_) {
        if (const std::optional<std::int32_t> queued = queuedAt_(worldX, y, worldZ); queued.has_value()) {
            return queued;
        }
    }
    return withChunk(keyOf(neighbourX, neighbourZ), neighbourX, neighbourZ,
                     [&](const Chunk& chunk) -> std::optional<std::int32_t> {
                         const ChunkHeight bounds = chunk.verticalBounds();
                         if (y < bounds.minY() || y > bounds.maxY()) {
                             return std::nullopt;
                         }
                         return chunk.blockStateId(worldX & 15, y, worldZ & 15);
                     });
}

std::int32_t NeighbourTerrainCache::worldgenHeightAt(const std::int32_t worldX, const std::int32_t worldZ,
                                                     const HeightmapType type) {
    const std::int32_t chunkX = worldX >> 4;
    const std::int32_t chunkZ = worldZ >> 4;
    return withChunk(keyOf(chunkX, chunkZ), chunkX, chunkZ,
                     [&](const Chunk& chunk) { return chunk.worldgenHeightAt(worldX & 15, worldZ & 15, type); });
}

std::int32_t NeighbourTerrainCache::oceanFloorAt(const std::int32_t neighbourX, const std::int32_t neighbourZ,
                                                 const std::int32_t worldX, const std::int32_t worldZ) {
    return withChunk(keyOf(neighbourX, neighbourZ), neighbourX, neighbourZ, [&](const Chunk& chunk) {
        // The frozen one: whoever asks a neighbour for its floor is standing in for a `_WG` read,
        // and the live map already has that neighbour's own structures piled on it.
        return chunk.worldgenHeightAt(worldX & 15, worldZ & 15, HeightmapType::OceanFloor);
    });
}

} // namespace basalt::server::world::terrain
