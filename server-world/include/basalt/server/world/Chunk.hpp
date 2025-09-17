#pragma once

#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/ChunkSection.hpp>
#include <basalt/server/world/ChunkTypes.hpp>
#include <basalt/server/world/structure/StructureBounds.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace basalt::server::world {

/// Which blocks each heightmap counts.
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

/// Whether a state counts towards that heightmap.
///
/// Answered from a table built once per state id rather than the predicate chain behind it, which
/// costs a lookup plus a dozen block questions every time. Heightmap scans run this millions of
/// times per generated chunk.
[[nodiscard]] bool heightmapMatches(HeightmapType type, std::int32_t stateId);

/// One 16x16 column of the world, as sections plus everything that travels with them.
class Chunk {
public:
    explicit Chunk(std::int32_t chunkX, std::int32_t chunkZ, ChunkHeight bounds = kOverworldHeight);

    [[nodiscard]] std::int32_t chunkX() const noexcept { return chunkX_; }

    [[nodiscard]] std::int32_t chunkZ() const noexcept { return chunkZ_; }

    [[nodiscard]] ChunkHeight verticalBounds() const noexcept { return bounds_; }

    [[nodiscard]] std::size_t sectionCount() const noexcept { return sections_.size(); }

    [[nodiscard]] ChunkSection& section(std::size_t index) { return sections_[index]; }

    [[nodiscard]] const ChunkSection& section(std::size_t index) const { return sections_[index]; }

    [[nodiscard]] std::int32_t blockStateId(std::int32_t x, std::int32_t y, std::int32_t z) const;

    bool setBlockStateId(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId);

    /// Whether this chunk holds anything the region files do not: set by every real block change and
    /// cleared once it has been written out. Saving only what changed is what keeps a shutdown from
    /// reserializing every chunk a session ever loaded.
    [[nodiscard]] bool isDirty() const noexcept { return dirty_; }

    /// Marks a change the block states do not carry, like a furnace counting down inside its own NBT.
    void markDirty() noexcept { dirty_ = true; }

    /// Called after this chunk has been persisted, or right after loading one straight off disk.
    void markClean() noexcept { dirty_ = false; }

    /// How long this chunk has actually been simulated.
    ///
    /// parity: `ChunkAccess.inhabitedTime`, which local difficulty reads rather than the world's
    /// total age. A newly generated chunk in an old world therefore still begins at zero.
    [[nodiscard]] std::int64_t inhabitedTime() const noexcept { return inhabitedTime_; }

    /// Advances the local difficulty clock and makes the persisted chunk reflect it.
    void increaseInhabitedTime(std::int64_t delta = 1);

    /// Restores the disk value without making a freshly loaded chunk dirty.
    void restoreInhabitedTime(std::int64_t value);

    /// Sets the biome network id at quart resolution. `quartY` is the world quart, `quartX` and
    /// `quartZ` are chunk-local.
    void setBiomeId(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ, std::int32_t biomeId);

    [[nodiscard]] std::int32_t biomeId(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ) const;

    /// Topmost matching Y plus one per column, matching vanilla's heightmap convention.
    [[nodiscard]] std::vector<std::int32_t> heightmap(HeightmapType type = HeightmapType::MotionBlocking) const;

    /// The heightmap value for one local column, without scanning the other 255.
    [[nodiscard]] std::int32_t heightAt(std::int32_t x, std::int32_t z,
                                        HeightmapType type = HeightmapType::MotionBlocking) const;

    /// Takes the `_WG` snapshot. Called once, between the carvers and the first feature.
    ///
    /// That is the point of having two sets: an ore's probe must see bare terrain, not what earlier
    /// features piled on top.
    void freezeWorldgenHeights();

    /// The height as it was when the carvers finished. Falls back to the live map for a chunk that
    /// was never frozen, so a detached or test-built chunk still answers something sensible.
    [[nodiscard]] std::int32_t worldgenHeightAt(std::int32_t x, std::int32_t z,
                                                HeightmapType type = HeightmapType::OceanFloor) const;

    /// Tells the chunk its blocks changed behind its back, which writing through `section` does.
    ///
    /// The heightmaps are rebuilt on the next read rather than here, so loading a chunk section by
    /// section rebuilds once and not once per section.
    void invalidateHeightmaps();

    /// One packed nibble array per section, which is how light travels and how it is kept.
    using PackedLight = std::vector<std::vector<std::uint8_t>>;

    /// This chunk's sky light, worked out once and kept until a block moves.
    ///
    /// A pass run without every neighbour loaded is kept all the same: it lets light in from where
    /// nothing is known yet, and `invalidateLight` throws it away as soon as the missing neighbour
    /// arrives. Not keeping it meant recomputing the whole chunk on *every* query at the edge of the
    /// loaded area, which cost over two milliseconds a time.
    const PackedLight& skyLight(const std::function<PackedLight()>& compute);

    /// The same for the light the blocks themselves give off.
    const PackedLight& blockLight(const std::function<PackedLight()>& compute);

    /// The light this chunk has already worked out, or null when nobody has asked yet.
    ///
    /// A reader that only wants one block's light uses this to skip building a window over the nine
    /// chunks around it: that window was costing more than the lookup it was there to serve.
    [[nodiscard]] const PackedLight* cachedSkyLight() const;

    [[nodiscard]] const PackedLight* cachedBlockLight() const;

    /// Drops the kept light, for when a neighbour arrives and this chunk's own pass is now stale.
    void invalidateLight();

    void setBlockEntity(ChunkBlockEntity blockEntity);

    [[nodiscard]] const ChunkBlockEntity* blockEntityAt(std::int32_t localX, std::int32_t y, std::int32_t localZ) const;

    [[nodiscard]] std::vector<ChunkBlockEntity> blockEntities() const;

    [[nodiscard]] std::size_t blockEntityCount() const noexcept { return blockEntities_.size(); }

    /// Scheduled block plus fluid ticks held, without copying either list.
    [[nodiscard]] std::size_t scheduledTickCount() const noexcept { return blockTicks_.size() + fluidTicks_.size(); }

    void scheduleBlockTick(const ChunkScheduledTick& tick);
    void scheduleFluidTick(const ChunkScheduledTick& tick);

    [[nodiscard]] std::span<const ChunkScheduledTick> scheduledBlockTicks() const noexcept { return blockTicks_; }

    [[nodiscard]] std::span<const ChunkScheduledTick> scheduledFluidTicks() const noexcept { return fluidTicks_; }

    /// Counts every pending tick down one and hands the ones that come due to `due`.
    ///
    /// The countdown itself does not dirty the chunk: whatever the tick then does to a block will,
    /// and marking it every tick would have a single flowing spring rewriting its region file twenty
    /// times a second.
    void advanceBlockTicks(std::vector<ChunkScheduledTick>& due);
    void advanceFluidTicks(std::vector<ChunkScheduledTick>& due);

    /// The entities this chunk carries in the region file, as opaque NBT.
    ///
    /// Opaque on purpose: what an entity is made of belongs to the runtime, and this layer only has
    /// to keep it beside the blocks it stands on and hand it back when the chunk returns.
    [[nodiscard]] std::span<const wire::nbt::NbtCompound> savedEntities() const noexcept { return savedEntities_; }

    void setSavedEntities(std::vector<wire::nbt::NbtCompound> entities);

    /// Hands them over once, which is what stops a chunk loaded twice spawning them twice.
    [[nodiscard]] std::vector<wire::nbt::NbtCompound> takeSavedEntities();

    void addGeneratedEntity(ChunkGeneratedEntity entity);

    [[nodiscard]] std::span<const ChunkGeneratedEntity> generatedEntities() const noexcept {
        return generatedEntities_;
    }

    /// Hands matching generated entities to the runtime once, leaving immutable structure furniture
    /// in the chunk. A generated mob becomes a normal saved entity after this handoff; keeping its
    /// descriptor as well would make a second copy whenever the chunk was loaded again.
    [[nodiscard]] std::vector<ChunkGeneratedEntity>
    takeGeneratedEntitiesMatching(const std::function<bool(const ChunkGeneratedEntity&)>& predicate);

    /// Approximate bytes the packed sections and heightmaps keep resident; attachments are counted,
    /// not sized.
    [[nodiscard]] std::size_t residentBytes() const;

    /// What this chunk knows about structures, for position queries and for spawn overrides.
    [[nodiscard]] structure::ChunkStructures& structures() noexcept { return structures_; }

    [[nodiscard]] const structure::ChunkStructures& structures() const noexcept { return structures_; }

    /// Adds a structure start, marking the chunk dirty when it was not already known.
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

    // parity: Heightmap, kept as blocks are written rather than rescanned on demand. Scanning cost
    // 2.6 million block reads per generated chunk, decoration alone asking for 10000 columns.
    mutable std::array<std::vector<std::int32_t>, kHeightmapCount> heights_;
    mutable bool heightsValid_ = true;
    std::optional<std::array<std::vector<std::int32_t>, kHeightmapCount>> worldgenHeights_;

    std::map<std::int32_t, ChunkBlockEntity> blockEntities_;
    std::vector<ChunkGeneratedEntity> generatedEntities_;
    std::vector<wire::nbt::NbtCompound> savedEntities_;
    std::vector<ChunkScheduledTick> blockTicks_;
    std::vector<ChunkScheduledTick> fluidTicks_;
    structure::ChunkStructures structures_;
    std::optional<PackedLight> skyLight_;
    std::optional<PackedLight> blockLight_;
    bool dirty_ = false;
    std::int64_t inhabitedTime_ = 0;
};

} // namespace basalt::server::world
