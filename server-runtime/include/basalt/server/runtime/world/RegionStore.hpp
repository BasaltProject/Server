#pragma once

#include <basalt/metamorph/region/BswConvert.hpp>
#include <basalt/metamorph/region/LinearRegion.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/world/BswChunkStore.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <atomic>
#include <cstdint>
#include <list>
#include <memory>
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
                         std::size_t capacity = kCapacity, std::int64_t byteBudget = kByteBudget,
                         config::RegionFormatSetting format = config::RegionFormatSetting::Linear);

    ~RegionStore();

    RegionStore(const RegionStore&) = delete;
    RegionStore& operator=(const RegionStore&) = delete;

    /// Regions sit in `region/`, not vanilla's `dimensions/` nesting, as a world is one dimension.
    [[nodiscard]] const std::string& worldRoot() const noexcept { return worldRoot_; }

    [[nodiscard]] std::string regionPath(std::int32_t regionX, std::int32_t regionZ) const;

    [[nodiscard]] bool usesBsw() const noexcept { return bsw_ != nullptr; }

    [[nodiscard]] bool usesAnvil() const noexcept { return anvil_; }

    [[nodiscard]] std::size_t cachedRegionCount() const;

    [[nodiscard]] std::int64_t cachedRegionBytes() const;

    [[nodiscard]] std::int64_t encodeMillis() const noexcept { return encodeMillis_; }

    [[nodiscard]] std::int64_t writeMillis() const noexcept { return writeMillis_; }

    /// Chunk reads since start and the microseconds they took, decoding included.
    [[nodiscard]] std::int64_t loadCount() const noexcept { return loadCount_.load(std::memory_order_relaxed); }

    [[nodiscard]] std::int64_t loadMicros() const noexcept { return loadMicros_.load(std::memory_order_relaxed); }

    [[nodiscard]] std::optional<server::world::Chunk> loadChunk(std::int32_t chunkX, std::int32_t chunkZ);

    /// The chunk as NBT bytes, decoded from the stored form when that is binary.
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> rawChunk(std::int32_t chunkX, std::int32_t chunkZ);

    [[nodiscard]] bool hasChunk(std::int32_t chunkX, std::int32_t chunkZ);

    /// Stores NBT bytes (empty clears the chunk) and writes its region; refused for chunks that cannot be read.
    bool writeRawChunk(std::int32_t chunkX, std::int32_t chunkZ, std::span<const std::uint8_t> nbt,
                       std::int32_t timestamp);

    /// Chunk coordinates (x, z pairs) stored in one region.
    [[nodiscard]] std::vector<std::int32_t> storedChunks(std::int32_t regionX, std::int32_t regionZ);

    enum class Age : std::uint8_t { Absent, Current, MaybeOld };

    [[nodiscard]] Age ageOf(std::int32_t chunkX, std::int32_t chunkZ);

    /// Answers how many region files were rewritten: a region is recompressed whole however few of its chunks changed.
    std::size_t saveChunks(std::span<server::world::Chunk* const> chunks, std::int32_t timestamp);

    /// A chunk as the owner thread saw it: the stored bytes (bsw) or the NBT (linear) are built there.
    struct Snapshot {
        std::int32_t chunkX = 0;
        std::int32_t chunkZ = 0;
        wire::nbt::NbtCompound nbt;
        std::vector<std::uint8_t> stored;
    };

    /// Encodes the chunk the way this store keeps it.
    [[nodiscard]] Snapshot snapshotOf(const server::world::Chunk& chunk) const;

    /// `saveChunks` for snapshots; answers how many region files were rewritten.
    std::size_t saveSnapshots(std::vector<Snapshot> snapshots, std::int32_t timestamp);

    /// Defers region writes and spreads encoding over all cores until the next `flush`; for shutdown only.
    void beginBatch();

    [[nodiscard]] bool batching() const noexcept { return batching_.load(); }

    std::size_t flush();

    /// Permanently fence disk I/O after unload; retained views cannot recreate deleted files.
    void close();

private:
    struct Entry {
        std::int64_t key = 0;
        metamorph::region::LinearRegion region;
    };

    [[nodiscard]] static std::int64_t key(std::int32_t regionX, std::int32_t regionZ);
    [[nodiscard]] static std::int32_t regionXOf(std::int64_t regionKey);
    [[nodiscard]] static std::int32_t regionZOf(std::int64_t regionKey);

    [[nodiscard]] bool ensureLoaded(std::int32_t regionX, std::int32_t regionZ);

    [[nodiscard]] metamorph::region::LinearRegion* touch(std::int64_t regionKey);

    /// Caller holds `mutex_`; reads from disk if needed.
    [[nodiscard]] metamorph::region::LinearRegion* region(std::int32_t regionX, std::int32_t regionZ);

    struct RegionRead {
        std::optional<metamorph::region::LinearRegion> region;
        /// A file is there but cannot be read; it must be left alone.
        bool unreadable = false;
    };

    /// Moves the entities of a vanilla region's `entities/` file into its chunks.
    void mergeVanillaEntities(metamorph::region::LinearRegion& region, std::int32_t regionX,
                              std::int32_t regionZ) const;

    /// The region from its `.linear` file or, failing that, the `.mca` of a vanilla world.
    [[nodiscard]] RegionRead readRegionFile(std::int32_t regionX, std::int32_t regionZ) const;

    metamorph::region::LinearRegion& store(std::int64_t regionKey, metamorph::region::LinearRegion region);

    /// Caller holds `mutex_`.
    [[nodiscard]] std::int64_t cachedBytesLocked() const;

    /// Caller must hold `writeMutex_` and must not hold `mutex_`.
    std::size_t flushDirty();

    /// Picks the world's format, converting its regions first when the setting asks for another one.
    void chooseFormat(config::RegionFormatSetting setting);

    void convertRegions(metamorph::region::RegionFormat from, metamorph::region::RegionFormat to);

    [[nodiscard]] std::optional<server::world::Chunk> readChunk(std::int32_t chunkX, std::int32_t chunkZ);

    [[nodiscard]] static Age classify(std::span<const std::uint8_t> blob);

    /// Reads a chunk from bsw storage into `out`; false when absent or unreadable, which it records.
    [[nodiscard]] bool readBsw(std::int32_t chunkX, std::int32_t chunkZ, std::vector<std::uint8_t>& out);

    std::string worldRoot_;
    server::world::ChunkHeight verticalBounds_;
    std::size_t capacity_;
    std::int64_t byteBudget_;

    mutable std::mutex mutex_;

    /// Serializes region staging and file writes; see the class comment for the lock order.
    std::mutex writeMutex_;
    bool closed_ = false;
    std::atomic<bool> batching_{false};

    /// Least recently used at the front, so eviction walks forwards.
    std::unordered_set<std::int64_t> writing_;
    std::list<Entry> cached_;
    std::unordered_map<std::int64_t, std::list<Entry>::iterator> byKey_;
    std::unordered_set<std::int64_t> dirtyRegions_;

    /// Chunks and regions that exist on disk but cannot be read: never written over, or the world is lost.
    std::unordered_set<std::int64_t> unreadableChunks_;
    std::unordered_set<std::int64_t> unreadableRegions_;
    wire::common::Logger logger_{"Regions"};
    std::unique_ptr<BswChunkStore> bsw_;
    bool anvil_ = false;

    std::atomic<std::int64_t> loadCount_{0};
    std::atomic<std::int64_t> loadMicros_{0};
    std::int64_t encodeMillis_ = 0;
    std::int64_t writeMillis_ = 0;
};

} // namespace basalt::server::runtime::world
