#pragma once

#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/ChunkSection.hpp>
#include <basalt/server/world/ChunkTypes.hpp>
#include <basalt/server/world/structure/StructureBounds.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <vector>

namespace basalt::server::world {

namespace terrain {
class ProtoRegion;
} // namespace terrain

namespace blending {
class BlendingData;
} // namespace blending

enum class HeightmapType : std::uint8_t {
    /// parity: `Heightmap.Types.WORLD_SURFACE(_WG)`, every non-air block and fluid.
    WorldSurface,
    /// parity: `Heightmap.Types.OCEAN_FLOOR(_WG)`, blocks that stop motion but not fluids.
    OceanFloor,
    /// parity: `Heightmap.Types.MOTION_BLOCKING`, motion-blocking blocks plus fluids.
    MotionBlocking,
    /// parity: `MOTION_BLOCKING_NO_LEAVES`, used by near-water firefly bushes.
    MotionBlockingNoLeaves,
};

inline constexpr std::size_t kHeightmapCount = 4;

[[nodiscard]] bool heightmapMatches(HeightmapType type, std::int32_t stateId);

/// parity: `BlockTags.BLOCKS_MOTION_IN_HEIGHTMAP(_NO_LEAVES)`, one flag per block state; applies to later queries.
void useHeightmapBlockSets(const std::vector<bool>& blocksMotion, const std::vector<bool>& blocksMotionNoLeaves);

/// Whether replacing one block state with another can move the light around it.
[[nodiscard]] bool changesLight(std::int32_t before, std::int32_t after);

class Chunk {
public:
    explicit Chunk(std::int32_t chunkX, std::int32_t chunkZ, ChunkHeight bounds = kOverworldHeight);

    [[nodiscard]] std::int32_t chunkX() const noexcept { return chunkX_; }

    [[nodiscard]] std::int32_t chunkZ() const noexcept { return chunkZ_; }

    [[nodiscard]] ChunkHeight verticalBounds() const noexcept { return bounds_; }

    [[nodiscard]] std::size_t sectionCount() const noexcept { return sections_.size(); }

    [[nodiscard]] ChunkSection& section(std::size_t index) { return sections_[index]; }

    [[nodiscard]] const ChunkSection& section(std::size_t index) const { return sections_[index]; }

    /// parity: nothing; before this a chunk belongs to the one task generating it and its sections take no lock.
    void share() noexcept {
        for (ChunkSection& section : sections_) {
            section.share();
        }
    }

    [[nodiscard]] std::int32_t blockStateId(std::int32_t x, std::int32_t y, std::int32_t z) const;

    bool setBlockStateId(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId);

    /// Replaces a whole section at once: block entities of changed blocks go, heights and light are marked stale.
    void replaceSectionStates(std::size_t sectionIndex, std::span<const std::int32_t> next);

    [[nodiscard]] bool isDirty() const noexcept { return dirty_; }

    void markDirty() noexcept { dirty_ = true; }

    void markClean() noexcept { dirty_ = false; }

    /// parity: `ChunkAccess.inhabitedTime`.
    [[nodiscard]] std::int64_t inhabitedTime() const noexcept { return inhabitedTime_; }

    void increaseInhabitedTime(std::int64_t delta = 1);

    void restoreInhabitedTime(std::int64_t value);

    void setBiomeId(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ, std::int32_t biomeId);

    [[nodiscard]] std::int32_t biomeId(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ) const;

    /// parity: the union of the section palettes that `applyBiomeDecoration` reads as `getBiomesWithin`.
    [[nodiscard]] bool hasBiome(std::int32_t biomeId) const;

    [[nodiscard]] std::vector<std::int32_t> heightmap(HeightmapType type = HeightmapType::MotionBlocking) const;

    [[nodiscard]] std::int32_t heightAt(std::int32_t x, std::int32_t z,
                                        HeightmapType type = HeightmapType::MotionBlocking) const;

    void freezeWorldgenHeights();

    [[nodiscard]] std::int32_t worldgenHeightAt(std::int32_t x, std::int32_t z,
                                                HeightmapType type = HeightmapType::OceanFloor) const;

    void invalidateHeightmaps();

    using PackedLight = std::vector<std::vector<std::uint8_t>>;

    [[nodiscard]] PackedLight skyLight(const std::function<PackedLight()>& compute);

    [[nodiscard]] PackedLight blockLight(const std::function<PackedLight()>& compute);

    using SharedLight = std::shared_ptr<const PackedLight>;

    /// The light as last computed, stale or not; null before the first computation.
    [[nodiscard]] SharedLight cachedSkyLight() const;

    [[nodiscard]] SharedLight cachedBlockLight() const;

    void invalidateLight();

    void markLightStale();

    [[nodiscard]] bool lightIsStale() const;

    void setBlockEntity(ChunkBlockEntity blockEntity);

    [[nodiscard]] const ChunkBlockEntity* blockEntityAt(std::int32_t localX, std::int32_t y, std::int32_t localZ) const;

    [[nodiscard]] ChunkBlockEntity* blockEntityAt(std::int32_t localX, std::int32_t y, std::int32_t localZ);

    void removeBlockEntityAt(std::int32_t x, std::int32_t y, std::int32_t z);

    [[nodiscard]] std::vector<ChunkBlockEntity> blockEntities() const;

    [[nodiscard]] std::size_t blockEntityCount() const noexcept { return blockEntities_.size(); }

    [[nodiscard]] std::size_t scheduledTickCount() const noexcept { return blockTicks_.size() + fluidTicks_.size(); }

    void scheduleBlockTick(const ChunkScheduledTick& tick);
    void scheduleFluidTick(const ChunkScheduledTick& tick);

    [[nodiscard]] std::span<const ChunkScheduledTick> scheduledBlockTicks() const noexcept { return blockTicks_; }

    [[nodiscard]] std::span<const ChunkScheduledTick> scheduledFluidTicks() const noexcept { return fluidTicks_; }

    /// parity: the jar books an absolute `triggerTick`.
    void openTurn();

    void advanceBlockTicks(std::vector<ChunkScheduledTick>& due);
    void advanceFluidTicks(std::vector<ChunkScheduledTick>& due);

    [[nodiscard]] std::span<const wire::nbt::NbtCompound> savedEntities() const noexcept { return savedEntities_; }

    /// Read back from storage already finished, so its blocks are final and nothing is settled over them.
    [[nodiscard]] bool loadedComplete() const noexcept { return loadedComplete_; }

    void markLoadedComplete() noexcept { loadedComplete_ = true; }

    /// A chunk saved before 1.18 keeps its terrain above zero and still needs the layers below it made.
    [[nodiscard]] bool needsRetrogen() const noexcept { return needsRetrogen_; }

    void setNeedsRetrogen(bool needed) noexcept { needsRetrogen_ = needed; }

    /// parity: `ChunkAccess.getBlendingData`, set on a chunk upgraded from before the 1.18 terrain.
    [[nodiscard]] const std::shared_ptr<blending::BlendingData>& blendingData() const noexcept { return blending_; }

    void setBlendingData(std::shared_ptr<blending::BlendingData> data) noexcept { blending_ = std::move(data); }

    void setSavedEntities(std::vector<wire::nbt::NbtCompound> entities);

    [[nodiscard]] std::vector<wire::nbt::NbtCompound> takeSavedEntities();

    void addGeneratedEntity(ChunkGeneratedEntity entity);

    [[nodiscard]] std::span<const ChunkGeneratedEntity> generatedEntities() const noexcept {
        return generatedEntities_;
    }

    [[nodiscard]] std::vector<ChunkGeneratedEntity>
    takeGeneratedEntitiesMatching(const std::function<bool(const ChunkGeneratedEntity&)>& predicate);

    [[nodiscard]] ChunkStage stage() const noexcept { return stage_; }

    void setStage(ChunkStage stage) noexcept { stage_ = stage; }

    [[nodiscard]] std::size_t residentBytes() const;

    void markFluidUpdate(std::int32_t worldX, std::int32_t y, std::int32_t worldZ);

    [[nodiscard]] std::span<const ChunkShapeUpdate> fluidUpdates() const { return fluidUpdates_; }

    void markShapeUpdate(std::int32_t worldX, std::int32_t y, std::int32_t worldZ);

    [[nodiscard]] std::span<const ChunkShapeUpdate> shapeUpdates() const { return shapeUpdates_; }

    /// parity: `Feature.markAboveForPostProcessing`.
    void markAboveForPostProcessing(std::int32_t worldX, std::int32_t y, std::int32_t worldZ);

    [[nodiscard]] std::int32_t decorationBlockStateId(std::int32_t worldX, std::int32_t y, std::int32_t worldZ) const;

    [[nodiscard]] std::optional<std::int32_t> queuedDecorationStateId(std::int32_t worldX, std::int32_t y,
                                                                      std::int32_t worldZ) const;

    void setDecorationBlockStateId(std::int32_t worldX, std::int32_t y, std::int32_t worldZ, std::int32_t stateId,
                                   std::optional<std::int32_t> expectedExternalStateId = std::nullopt,
                                   bool replaceAnyExternalState = false);

    [[nodiscard]] std::vector<ChunkGeneratedBlockWrite> takeGeneratedBlockWrites();

    void setDecorationBlockEntity(std::int32_t worldX, std::int32_t y, std::int32_t worldZ,
                                  std::int32_t expectedStateId, wire::common::Identifier type,
                                  wire::nbt::NbtCompound data);

    [[nodiscard]] std::vector<ChunkGeneratedBlockEntityWrite> takeGeneratedBlockEntityWrites();

    void scheduleDecorationBlockTick(std::int32_t worldX, std::int32_t y, std::int32_t worldZ,
                                     std::int32_t expectedStateId, wire::common::Identifier type, std::int32_t delay,
                                     std::int32_t priority = 0);

    [[nodiscard]] std::vector<ChunkGeneratedBlockTick> takeGeneratedBlockTicks();

    class BlockWriteObserverScope {
    public:
        using Observer = std::function<void(std::int32_t, std::int32_t, std::int32_t, std::int32_t, std::int32_t)>;

        BlockWriteObserverScope(Chunk& chunk, Observer observer);
        ~BlockWriteObserverScope();
        BlockWriteObserverScope(const BlockWriteObserverScope&) = delete;
        BlockWriteObserverScope& operator=(const BlockWriteObserverScope&) = delete;
        BlockWriteObserverScope(BlockWriteObserverScope&&) = delete;
        BlockWriteObserverScope& operator=(BlockWriteObserverScope&&) = delete;

    private:
        Chunk* chunk_;
    };

    class ExternalStateScope {
    public:
        using Reader = std::function<std::optional<std::int32_t>(std::int32_t, std::int32_t, std::int32_t)>;

        ExternalStateScope(Chunk& chunk, Reader reader);
        ~ExternalStateScope();
        ExternalStateScope(const ExternalStateScope&) = delete;
        ExternalStateScope& operator=(const ExternalStateScope&) = delete;
        ExternalStateScope(ExternalStateScope&&) = delete;
        ExternalStateScope& operator=(ExternalStateScope&&) = delete;

    private:
        Chunk* chunk_;
        Reader previous_;
    };

    /// parity: `WorldGenRegion`.
    class RegionScope {
    public:
        RegionScope(Chunk& chunk, terrain::ProtoRegion& region);
        ~RegionScope();
        RegionScope(const RegionScope&) = delete;
        RegionScope& operator=(const RegionScope&) = delete;
        RegionScope(RegionScope&&) = delete;
        RegionScope& operator=(RegionScope&&) = delete;

    private:
        Chunk* chunk_;
        terrain::ProtoRegion* previous_;
    };

    class WritePolicyScope {
    public:
        WritePolicyScope(Chunk& chunk, GeneratedBlockWritePolicy policy);
        ~WritePolicyScope();
        WritePolicyScope(const WritePolicyScope&) = delete;
        WritePolicyScope& operator=(const WritePolicyScope&) = delete;
        WritePolicyScope(WritePolicyScope&&) = delete;
        WritePolicyScope& operator=(WritePolicyScope&&) = delete;

    private:
        Chunk* chunk_;
        GeneratedBlockWritePolicy previous_;
    };

    [[nodiscard]] structure::ChunkStructures& structures() noexcept { return structures_; }

    [[nodiscard]] const structure::ChunkStructures& structures() const noexcept { return structures_; }

    void addStructureStart(structure::StructureStartRecord start);

private:
    [[nodiscard]] std::size_t sectionIndexFor(std::int32_t y) const;

    [[nodiscard]] std::int32_t blockEntityKey(std::int32_t x, std::int32_t y, std::int32_t z) const;

    void updateHeights(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId);

    void rebuildHeights() const;

    void scheduleTick(std::vector<ChunkScheduledTick>& ticks, const ChunkScheduledTick& tick);

    static void advance(std::vector<ChunkScheduledTick>& pending, std::vector<ChunkScheduledTick>& due);

    std::int32_t chunkX_;
    std::int32_t chunkZ_;
    ChunkHeight bounds_;
    std::vector<ChunkSection> sections_;

    // parity: Heightmap, kept as blocks are written rather than rescanned on demand. Scanning cost.
    mutable std::array<std::vector<std::int32_t>, kHeightmapCount> heights_;
    mutable bool heightsValid_ = true;
    std::optional<std::array<std::vector<std::int32_t>, kHeightmapCount>> worldgenHeights_;

    std::map<std::int32_t, ChunkBlockEntity> blockEntities_;
    std::vector<std::int32_t> blockEntityOrder_;
    std::vector<ChunkGeneratedEntity> generatedEntities_;
    std::vector<wire::nbt::NbtCompound> savedEntities_;
    bool loadedComplete_ = false;
    bool needsRetrogen_ = false;
    std::shared_ptr<blending::BlendingData> blending_;
    std::vector<ChunkScheduledTick> blockTicks_;
    std::vector<ChunkScheduledTick> fluidTicks_;
    structure::ChunkStructures structures_;

    std::vector<ChunkShapeUpdate> shapeUpdates_;
    std::vector<ChunkShapeUpdate> fluidUpdates_;
    std::vector<ChunkGeneratedBlockWrite> generatedBlockWrites_;
    std::vector<ChunkGeneratedBlockEntityWrite> generatedBlockEntityWrites_;
    std::vector<ChunkGeneratedBlockTick> generatedBlockTicks_;
    std::function<void(std::int32_t, std::int32_t, std::int32_t, std::int32_t, std::int32_t)> blockWriteObserver_;
    std::function<std::optional<std::int32_t>(std::int32_t, std::int32_t, std::int32_t)> externalDecorationState_;
    GeneratedBlockWritePolicy generatedBlockWritePolicy_ = GeneratedBlockWritePolicy::Exact;
    terrain::ProtoRegion* region_ = nullptr;
    ChunkStage stage_ = ChunkStage::Full;

    struct LightCache {
        std::mutex lock;
        std::atomic<SharedLight> sky;
        std::atomic<SharedLight> block;
        std::atomic<bool> staleSky{false};
        std::atomic<bool> staleBlock{false};
    };

    std::unique_ptr<LightCache> light_ = std::make_unique<LightCache>();
    bool dirty_ = false;
    std::int64_t inhabitedTime_ = 0;
};

} // namespace basalt::server::world
