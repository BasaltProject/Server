#pragma once

#include <basalt/server/runtime/world/poi/PoiTypes.hpp>
#include <basalt/server/world/ChunkHeight.hpp>

#include <array>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace basalt::server::world {
class Chunk;
}

namespace basalt::wire::nbt {
class NbtCompound;
}

namespace basalt::server::runtime::world::poi {

struct BlockPos {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;

    friend bool operator==(const BlockPos&, const BlockPos&) = default;
};

/// parity: `PoiRecord`, a workstation, bed or bell and how many of its tickets are still free.
struct PoiRecord {
    BlockPos pos;
    PoiType type = PoiType::kNone;
    std::int32_t freeTickets = 0;

    [[nodiscard]] bool hasSpace() const noexcept { return freeTickets > 0; }

    [[nodiscard]] bool isOccupied() const noexcept { return freeTickets != poiTypes::maxTickets(type); }
};

/// parity: `PoiManager.Occupancy`.
enum class Occupancy : std::uint8_t { kHasSpace, kIsOccupied, kAny };

/// Every point of interest in one world, and how far each section is from a village.
/// parity: `PoiManager` over `PoiSection`, vanilla's NBT in `poi/` Linear regions (an `.mca` is read too).
/// Records keep the order of vanilla's per-type `HashSet`, bucket by bucket: the first free bed or
/// bell in range is whichever that order finds first.
class PoiManager {
public:
    /// parity: `MAX_VILLAGE_DISTANCE`, the furthest the section tracker counts.
    static constexpr std::int32_t kMaxVillageDistance = 6;

    explicit PoiManager(server::world::ChunkHeight bounds = server::world::kOverworldHeight);

    /// Where the region files live and how tall the world is. An empty directory keeps everything
    /// in memory, which is what a test wants.
    void configure(std::string directory, server::world::ChunkHeight bounds);

    /// parity: `ServerLevel.updatePOIOnBlockStateChange`.
    void noteBlockChange(const BlockPos& pos, std::int32_t oldStateId, std::int32_t newStateId);

    /// The same, for a write that reached the chunk without going through the world, where only
    /// the state it ended with is known.
    void noteBlockState(const BlockPos& pos, std::int32_t stateId);

    /// parity: `PoiManager.checkConsistencyWithBlocks` for every section of a chunk that arrived.
    void chunkInstalled(const server::world::Chunk& chunk);

    [[nodiscard]] std::optional<PoiType> type(const BlockPos& pos);

    [[nodiscard]] bool exists(const BlockPos& pos, PoiMask mask);

    [[nodiscard]] std::optional<PoiRecord> record(const BlockPos& pos);

    /// parity: `getInRange`, in the order vanilla walks it: chunks by row, sections upwards, then
    /// each type's set in its hash order. The result is written into `out`, which is cleared first.
    void inRange(PoiMask mask, const BlockPos& center, std::int32_t radius, Occupancy occupancy,
                 std::vector<PoiRecord>& out);

    /// parity: `getInSquare`, the same walk with only the square test.
    void inSquare(PoiMask mask, const BlockPos& center, std::int32_t radius, Occupancy occupancy,
                  std::vector<PoiRecord>& out);

    /// parity: `getCountInRange`.
    [[nodiscard]] std::int64_t countInRange(PoiMask mask, const BlockPos& center, std::int32_t radius,
                                            Occupancy occupancy);

    /// parity: `findClosest`, the first of the nearest in walk order.
    [[nodiscard]] std::optional<BlockPos> findClosest(PoiMask mask, const BlockPos& center, std::int32_t radius,
                                                      Occupancy occupancy);

    /// parity: `PoiRecord.acquireTicket` at a position, as `take` ends. Answers whether one was free.
    bool acquire(const BlockPos& pos);

    /// parity: `release`. Answers false where there is nothing registered, which vanilla throws on.
    bool release(const BlockPos& pos);

    /// parity: `sectionsToVillage`, the Chebyshev distance in sections to one holding an occupied
    /// village point, up to seven.
    [[nodiscard]] std::int32_t sectionsToVillage(std::int32_t sectionX, std::int32_t sectionY, std::int32_t sectionZ);

    /// parity: `ServerLevel.isCloseToVillage`.
    [[nodiscard]] bool isCloseToVillage(const BlockPos& pos, std::int32_t sectionDistance);

    /// parity: `ServerLevel.isVillage`.
    [[nodiscard]] bool isVillage(const BlockPos& pos) { return isCloseToVillage(pos, 1); }

    /// Writes every region with a changed chunk.
    void save();

    /// parity: `PoiSection.Packed.CODEC` over a whole chunk, `Sections` keyed by section y.
    [[nodiscard]] static wire::nbt::NbtCompound encodeChunk(const std::vector<PoiRecord>& records,
                                                            std::int32_t minSection, std::int32_t sectionCount);

private:
    struct Section {
        bool valid = true;
        /// Insertion order, which is the tie-break inside a hash bucket.
        std::vector<PoiRecord> records;
        /// The most records of each type the set has held, which fixes its table size.
        std::array<std::uint16_t, static_cast<std::size_t>(PoiType::kCount)> peak{};
    };

    struct ChunkPoi {
        std::vector<std::optional<Section>> sections;
    };

    [[nodiscard]] static std::int64_t chunkKey(std::int32_t chunkX, std::int32_t chunkZ);
    [[nodiscard]] static std::int64_t sectionKey(std::int32_t x, std::int32_t y, std::int32_t z);

    /// Caller holds the lock. Both read the region from disk the first time.
    [[nodiscard]] const ChunkPoi* findChunkLocked(std::int32_t chunkX, std::int32_t chunkZ);
    ChunkPoi& chunkLocked(std::int32_t chunkX, std::int32_t chunkZ);
    [[nodiscard]] Section* sectionLocked(std::int32_t x, std::int32_t y, std::int32_t z, bool create);
    void ensureRegionLocked(std::int32_t regionX, std::int32_t regionZ);
    void readChunkLocked(std::int32_t chunkX, std::int32_t chunkZ, const wire::nbt::NbtCompound& tag);

    void addLocked(Section& section, const BlockPos& pos, PoiType type, std::int32_t freeTickets);
    void removeLocked(Section& section, const BlockPos& pos);
    void changedLocked(const BlockPos& pos);
    void refreshCenterLocked(std::int32_t sectionX, std::int32_t sectionY, std::int32_t sectionZ);
    void walkLocked(PoiMask mask, const BlockPos& center, std::int32_t radius, Occupancy occupancy, bool round,
                    std::vector<PoiRecord>& out);

    server::world::ChunkHeight bounds_;
    std::string directory_;
    std::mutex mutex_;
    std::unordered_map<std::int64_t, ChunkPoi> chunks_;
    std::unordered_set<std::int64_t> loadedRegions_;
    std::unordered_set<std::int64_t> dirtyRegions_;
    std::unordered_set<std::int64_t> centers_;
    std::unordered_map<std::int64_t, std::uint8_t> levels_;
};

} // namespace basalt::server::runtime::world::poi
