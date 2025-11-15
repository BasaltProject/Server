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

/// Every world lives under this directory, and nothing else does.
inline constexpr std::string_view kWorldsRoot = "worlds";

/// Keeps a few decompressed regions in memory, so a region file is unpacked once rather than once
/// per chunk. Linear compresses a whole region as one zstd stream, so reading any chunk unpacks all
/// 1024: doing that per chunk from every worker was the dominant source of memory pressure while
/// exploring.
///
/// Decompression happens outside the lock. Holding it across a region of zstd starved the
/// save/unload sweep, which needs the same lock, until the server quietly stopped saving at all.
///
/// Compression is outside it too: `mutex_` only ever guards the cache and region staging, while
/// disk writes run under `writeMutex_` alone. A sweep compressing a dozen regions used to hold the
/// cache lock for the whole 700 ms, and every generation worker needing `loadChunk` stood in line
/// behind it. Lock order is `writeMutex_` then `mutex_`, never the reverse.
class RegionStore {
public:
    // A player near a region corner works across four at once, and prefetch reaches past them, so
    // four meant constant thrashing: every miss is a full region decompressed again.
    static constexpr std::size_t kCapacity = 12;

    // The count on its own bounds nothing: a region holds 1024 decompressed chunk blobs, so twelve
    // full ones are hundreds of megabytes, and the process grew with distance flown. Whichever
    // limit bites first wins, which keeps many sparse regions cheap and few full ones bounded.
    static constexpr std::int64_t kByteBudget = 96LL * 1024 * 1024;

    /// `capacity` and `byteBudget` are injectable so a test can reach the ceiling without writing a
    /// hundred megabytes.
    explicit RegionStore(std::string worldName, std::string worldsRoot = std::string{kWorldsRoot},
                         server::world::ChunkHeight verticalBounds = server::world::kOverworldHeight,
                         std::size_t capacity = kCapacity, std::int64_t byteBudget = kByteBudget);

    RegionStore(const RegionStore&) = delete;
    RegionStore& operator=(const RegionStore&) = delete;

    /// Where this world's files live: `worlds/<name>/`, and nothing outside it belongs to this
    /// world.
    ///
    /// Regions sit in `region/`, not under vanilla's `dimensions/<namespace>/<path>/`: that nesting
    /// is for a save holding three dimensions, and here a world is one. Player state deliberately
    /// does not live here (it is in `players.db`, beside `worlds/`), which is what lets a world be
    /// deleted, copied or reset without taking everyone's inventory with it.
    [[nodiscard]] const std::string& worldRoot() const noexcept { return worldRoot_; }

    [[nodiscard]] std::string regionPath(std::int32_t regionX, std::int32_t regionZ) const;

    [[nodiscard]] std::size_t cachedRegionCount() const;

    [[nodiscard]] std::int64_t cachedRegionBytes() const;

    /// What the last save spent where, so a slow one can be attributed instead of guessed at.
    [[nodiscard]] std::int64_t encodeMillis() const noexcept { return encodeMillis_; }

    [[nodiscard]] std::int64_t writeMillis() const noexcept { return writeMillis_; }

    /// Nothing if this chunk has never been saved, meaning it still needs to be generated.
    [[nodiscard]] std::optional<server::world::Chunk> loadChunk(std::int32_t chunkX, std::int32_t chunkZ);

    /// Stages these chunks into their regions and writes every region they touched. Answers how
    /// many region files were rewritten, the unit of real work: a region is recompressed whole
    /// however few of its chunks changed.
    std::size_t saveChunks(std::span<server::world::Chunk* const> chunks, std::int32_t timestamp);

    /// Writes out any region held with unsaved changes.
    std::size_t flush();

private:
    struct Entry {
        std::int64_t key = 0;
        server::world::region::LinearRegion region;
    };

    [[nodiscard]] static std::int64_t key(std::int32_t regionX, std::int32_t regionZ);
    [[nodiscard]] static std::int32_t regionXOf(std::int64_t regionKey);
    [[nodiscard]] static std::int32_t regionZOf(std::int64_t regionKey);

    /// Caller holds `mutex_`. Moves an already-cached region to the back of the LRU.
    [[nodiscard]] server::world::region::LinearRegion* touch(std::int64_t regionKey);

    /// Caller holds `mutex_`. Reads from disk if needed; only the save path, which is already under
    /// the lock.
    [[nodiscard]] server::world::region::LinearRegion* region(std::int32_t regionX, std::int32_t regionZ);

    /// Caller holds `mutex_`.
    server::world::region::LinearRegion& store(std::int64_t regionKey, server::world::region::LinearRegion region);

    /// Caller holds `mutex_`. Sums the cache rather than tracking it: a dozen entries is nothing to
    /// add up, and a counter would drift the moment a save wrote through a region already cached.
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
