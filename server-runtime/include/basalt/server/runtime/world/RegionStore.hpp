#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/region/LinearRegion.hpp>

#include <cstdint>
#include <list>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace basalt::server::runtime::world {

inline constexpr std::string_view kWorldsRoot = "worlds";

/// Linear compresses a region as one zstd stream, so any chunk read unpacks all 1024.
class RegionStore {
public:
    static constexpr std::size_t kCapacity = 12;

    static constexpr std::int64_t kByteBudget = 96LL * 1024 * 1024;

    /// Injectable so a test can reach the ceiling without writing a hundred megabytes.
    explicit RegionStore(std::string worldName, std::string worldsRoot = std::string{kWorldsRoot},
                         server::world::ChunkHeight verticalBounds = server::world::kOverworldHeight,
                         std::size_t capacity = kCapacity, std::int64_t byteBudget = kByteBudget);

    RegionStore(const RegionStore&) = delete;
    RegionStore& operator=(const RegionStore&) = delete;

    /// Regions sit in `region/`, not vanilla's `dimensions/` nesting, as a world is one dimension.
    [[nodiscard]] const std::string& worldRoot() const noexcept { return worldRoot_; }

    [[nodiscard]] std::string regionPath(std::int32_t regionX, std::int32_t regionZ) const;

    [[nodiscard]] std::size_t cachedRegionCount() const;

    [[nodiscard]] std::int64_t cachedRegionBytes() const;

    [[nodiscard]] std::int64_t encodeMillis() const noexcept { return encodeMillis_; }

    [[nodiscard]] std::int64_t writeMillis() const noexcept { return writeMillis_; }

    [[nodiscard]] std::optional<server::world::Chunk> loadChunk(std::int32_t chunkX, std::int32_t chunkZ);

    /// Answers how many region files were rewritten: a region is recompressed whole however few of its chunks changed.
    std::size_t saveChunks(std::span<server::world::Chunk* const> chunks, std::int32_t timestamp);

    std::size_t flush();

private:
    struct Entry {
        std::int64_t key = 0;
        server::world::region::LinearRegion region;
    };

    [[nodiscard]] static std::int64_t key(std::int32_t regionX, std::int32_t regionZ);
    [[nodiscard]] static std::int32_t regionXOf(std::int64_t regionKey);
    [[nodiscard]] static std::int32_t regionZOf(std::int64_t regionKey);

    [[nodiscard]] server::world::region::LinearRegion* touch(std::int64_t regionKey);

    /// Caller holds `mutex_`; reads from disk if needed.
    [[nodiscard]] server::world::region::LinearRegion* region(std::int32_t regionX, std::int32_t regionZ);

    server::world::region::LinearRegion& store(std::int64_t regionKey, server::world::region::LinearRegion region);

    /// Caller holds `mutex_`.
    [[nodiscard]] std::int64_t cachedBytesLocked() const;

    /// Caller must hold `writeMutex_` and must not hold `mutex_`.
    std::size_t flushDirty();

    std::string worldRoot_;
    server::world::ChunkHeight verticalBounds_;
    std::size_t capacity_;
    std::int64_t byteBudget_;

    mutable std::mutex mutex_;

    /// Serializes region staging and file writes; see the class comment for the lock order.
    std::mutex writeMutex_;

    /// Least recently used at the front, so eviction walks forwards.
    std::list<Entry> cached_;
    std::unordered_map<std::int64_t, std::list<Entry>::iterator> byKey_;
    std::unordered_set<std::int64_t> dirtyRegions_;

    std::int64_t encodeMillis_ = 0;
    std::int64_t writeMillis_ = 0;
};

} // namespace basalt::server::runtime::world
