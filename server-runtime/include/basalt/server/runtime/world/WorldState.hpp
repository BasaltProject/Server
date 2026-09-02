#pragma once

#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/sched/Owner.hpp>
#include <basalt/server/runtime/sched/RegionScheduler.hpp>
#include <basalt/server/runtime/world/DroppedItemPhysics.hpp>
#include <basalt/server/runtime/world/EntityRegistry.hpp>
#include <basalt/server/runtime/world/PendingChunkDecorations.hpp>
#include <basalt/server/runtime/world/PrimedTntQueue.hpp>
#include <basalt/server/runtime/world/SculkSensorTracker.hpp>
#include <basalt/server/runtime/world/WorldMutex.hpp>
#include <basalt/server/world/Chunk.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace basalt::server::runtime::world {

/// Shared view over the loaded chunks.
///
/// The owner writes blocks without waiting for anybody: what the lock guards is the *set* of
/// chunks, which the generation pool adds to from its own threads, and a block read takes it
/// shared. `loadOrGenerate` must read the region files before generating, or a visited chunk comes
/// back regenerated with the player's edits gone.
class WorldState {
public:
    /// How a chunk that is not loaded is produced: read from disk if it is there, generated if not.
    using LoadOrGenerate = std::function<server::world::Chunk(std::int32_t chunkX, std::int32_t chunkZ)>;

    /// What every loaded chunk keeps resident, for the timings report's memory breakdown.
    struct ChunkMemory {
        std::int64_t bytes = 0;
        std::int32_t blockEntities = 0;
        std::int32_t scheduledTicks = 0;
        std::int32_t generatedEntities = 0;
    };

    explicit WorldState(std::vector<server::world::Chunk> initialChunks = {}, LoadOrGenerate loadOrGenerate = {});

    WorldState(const WorldState&) = delete;
    WorldState& operator=(const WorldState&) = delete;
    WorldState(WorldState&&) = delete;
    WorldState& operator=(WorldState&&) = delete;
    ~WorldState() = default;

    /// The packed key a chunk is filed under, which is what the region scheduler reasons about.
    [[nodiscard]] static std::int64_t key(std::int32_t chunkX, std::int32_t chunkZ);

    /// Who owns this state, once somebody does.
    ///
    /// Null for the bare world a test builds, which is single threaded by construction. What it
    /// buys is the ownership check: with one region an access from the wrong thread works by luck,
    /// and luck is what stops working the day regions are turned on.
    void setOwner(sched::Owner* owner) { owner_ = owner; }

    [[nodiscard]] sched::Owner* owner() const noexcept { return owner_; }

    /// How many regions this world is in, which decides whether the ownership check can say
    /// anything useful.
    void setRegionCount(std::function<std::int32_t()> regionCount) { regionCount_ = std::move(regionCount); }

    /// Who installs the chunks the generation pool produces.
    ///
    /// The global owner rather than a region's: a chunk that has just been generated belongs to no
    /// region yet, so handing it to one of them meant every install in the world piling onto
    /// whichever region was first.
    void setInstallOwner(sched::Owner* installOwner) { installOwner_ = installOwner; }

    /// Told when a chunk arrives, which is where the chunk-load event is fired from.
    ///
    /// A hook rather than the bus itself: this module is below the plugin one, and a world knowing
    /// what a plugin is would be an arrow pointing the wrong way.
    void setOnChunkInstalled(std::function<void(server::world::Chunk&)> handler) {
        onChunkInstalled_ = std::move(handler);
    }

    [[nodiscard]] std::vector<server::world::Chunk*> chunks() const;

    [[nodiscard]] std::size_t loadedChunkCount() const;

    [[nodiscard]] ChunkMemory chunkMemory() const;

    /// Only the chunks holding something the region files do not, which is all a save has to write.
    [[nodiscard]] std::vector<server::world::Chunk*> dirtyChunks() const;

    /// Loaded chunks carrying entities from the last save, which have to be rewritten when they
    /// move.
    [[nodiscard]] std::vector<server::world::Chunk*> chunksWithStashedEntities() const;

    /// Every loaded chunk the predicate rejects.
    [[nodiscard]] std::vector<server::world::Chunk*>
    evictionCandidates(const std::function<bool(std::int32_t chunkX, std::int32_t chunkZ)>& shouldKeep) const;

    /// How many of those positions are still missing, counted in one pass under one lock.
    [[nodiscard]] std::size_t countMissing(const std::vector<std::pair<std::int32_t, std::int32_t>>& positions) const;

    /// Drops those chunks from memory, skipping any that became dirty again since the caller saved
    /// them: evicting a chunk whose changes are not on disk throws away whatever the player built
    /// in it, and it will come up for eviction again on the next sweep anyway.
    std::size_t evictClean(const std::vector<server::world::Chunk*>& chunks);

    /// Every loaded chunk as one packed key, for whoever has to reason about the shape of the world
    /// rather than about its contents.
    [[nodiscard]] std::vector<std::int64_t> loadedChunkKeys() const;

    /// Bumped whenever a chunk arrives or goes.
    ///
    /// What the region scheduler asks before working out the regions again: the shape of a world
    /// only changes when its chunks do, and walking every loaded chunk on a tick where none of them
    /// moved was pure cost.
    [[nodiscard]] std::int64_t chunkSetVersion() const noexcept { return chunkVersion_.load(); }

    [[nodiscard]] bool isLoaded(std::int32_t chunkX, std::int32_t chunkZ) const;

    /// The chunk if it is already in memory. Never generates, so it is safe on a connection thread.
    [[nodiscard]] server::world::Chunk* chunkIfLoaded(std::int32_t chunkX, std::int32_t chunkZ) const;

    /// The chunk at this position, generating (but not sending or persisting) it first if unseen.
    server::world::Chunk& getOrGenerateChunk(std::int32_t chunkX, std::int32_t chunkZ);

    /// Reads or generates one chunk without putting it anywhere.
    ///
    /// The expensive half, and the half that touches nothing shared: a generation worker runs this
    /// on its own thread and then hands the result over. Answers nothing when the chunk turned up
    /// in the meantime, so the caller reads it instead.
    [[nodiscard]] std::optional<server::world::Chunk> generateDetached(std::int32_t chunkX, std::int32_t chunkZ);

    /// Puts a generated chunk into the world, answering whichever chunk ends up in it: the one
    /// already there if two workers raced on the same position.
    server::world::Chunk& install(server::world::Chunk chunk);

    /// Hands the chunk over to be installed, or installs it here when nobody owns this world.
    void installLater(server::world::Chunk chunk);

    [[nodiscard]] std::optional<std::int32_t> blockState(std::int32_t x, std::int32_t y, std::int32_t z) const;

    /// What the collision and spawning code asks the world, as the callback they take.
    [[nodiscard]] entity::entityCollision::BlockAt blockAccessor() const;

    /// Whether the position falls inside a loaded chunk, which is when the block was set.
    bool setBlockState(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId);

    [[nodiscard]] bool hasSculkSensorNear(double x, double y, double z, std::int32_t range) const;

    /// Queues a fluid tick that many ticks from now, doing nothing if the chunk is not loaded.
    ///
    /// parity: `LevelTicks.schedule`, minus its priorities: only one tick per position and fluid is
    /// ever pending, and re-scheduling an existing one keeps the delay already counting down.
    void scheduleFluidTick(std::int32_t x, std::int32_t y, std::int32_t z, const wire::common::Identifier& type,
                           std::int32_t delay);

    void scheduleBlockTick(std::int32_t x, std::int32_t y, std::int32_t z, const wire::common::Identifier& type,
                           std::int32_t delay);

    /// parity: `LevelTicks.hasScheduledTick`, which is what stops a target being re-armed mid-hold.
    [[nodiscard]] bool hasScheduledBlockTick(std::int32_t x, std::int32_t y, std::int32_t z,
                                             const wire::common::Identifier& type) const;

    /// Every fluid tick that comes due this tick, in world coordinates.
    [[nodiscard]] std::vector<std::array<std::int32_t, 3>> dueFluidTicks();

    /// The same for block ticks: a button that has been held down long enough, and later redstone.
    [[nodiscard]] std::vector<std::array<std::int32_t, 3>> dueBlockTicks();

    [[nodiscard]] std::vector<GeneratedWorldEntity>
    generatedEntities(const std::function<std::int32_t()>& allocateEntityId, std::int32_t centerChunkX,
                      std::int32_t centerChunkZ, std::int32_t radiusChunks);

    // --- What lives in the world ---

    DroppedItem& spawnItem(DroppedItem dropped);

    [[nodiscard]] std::vector<DroppedItem> items() const;

    [[nodiscard]] std::vector<DroppedItem> nearbyItems(double x, double y, double z, double radius) const;

    [[nodiscard]] std::optional<DroppedItem> removeItem(std::int32_t entityId);

    /// Shoves one item along, for a piston pushing whatever its blocks sweep over.
    bool shiftItem(std::int32_t entityId, double dx, double dy, double dz);

    [[nodiscard]] std::vector<std::int32_t> tickItems(const sched::Region* region = nullptr);

    [[nodiscard]] std::vector<entity::ServerEntity*> entities() const;

    [[nodiscard]] bool hasNaturalSpawnEntityObstruction(const entity::Aabb& spawnBox,
                                                        const entity::Aabb& entityBox) const;

    [[nodiscard]] std::vector<entity::ServerEntity*> entitiesIn(std::int32_t chunkX, std::int32_t chunkZ) const;

    [[nodiscard]] std::vector<std::pair<std::int32_t, std::int32_t>> chunksWithEntities() const;

    void forgetEntitiesIn(std::int32_t chunkX, std::int32_t chunkZ);

    [[nodiscard]] std::vector<wire::nbt::NbtCompound> takePendingEntities();

    [[nodiscard]] std::vector<server::world::ChunkGeneratedEntity> takePendingGeneratedEntities();

    [[nodiscard]] entity::ServerEntity* entity(std::int32_t entityId) const;

    entity::ServerEntity& spawnEntity(std::unique_ptr<entity::ServerEntity> entity);

    [[nodiscard]] std::vector<std::int32_t> collectRemovedEntities();

    /// parity: `TntBlock.explode`, which spawns the entity where the block stood.
    void primeTnt(std::int32_t x, std::int32_t y, std::int32_t z);

    [[nodiscard]] std::vector<std::array<std::int32_t, 3>> takePrimedTnt();

private:
    /// Refuses an access from anywhere but the owner, and only while strict ownership is on.
    ///
    /// Skipped while a world is split: the owner is the first region's, and a block out in the
    /// second one belongs to that one instead, so the check would refuse what is in fact correct.
    void mine(std::string_view what) const;

    [[nodiscard]] std::optional<std::int32_t> blockStateLocked(std::int32_t x, std::int32_t y, std::int32_t z) const;

    /// The block accessor for a collaborator that already holds the lock, which is where a
    /// non-recursive lock would otherwise deadlock against itself: the item physics walks the
    /// blocks under every dropped item while holding the world for writing.
    [[nodiscard]] entity::entityCollision::BlockAt lockedBlockAccessor() const;

    [[nodiscard]] server::world::Chunk* chunkAtLocked(std::int64_t chunkKey) const;
    [[nodiscard]] std::vector<server::world::Chunk*> snapshot() const;
    [[nodiscard]] std::vector<std::array<std::int32_t, 3>> dueTicks(
        const std::function<void(server::world::Chunk&, std::vector<server::world::ChunkScheduledTick>&)>& advance);
    server::world::Chunk& installLocked(server::world::Chunk chunk);

    mutable WorldMutex mutex_;
    LoadOrGenerate loadOrGenerate_;
    sched::Owner* owner_ = nullptr;
    sched::Owner* installOwner_ = nullptr;
    std::function<std::int32_t()> regionCount_;
    std::function<void(server::world::Chunk&)> onChunkInstalled_;

    /// Keyed by packed coordinates, not by a pair: a pair key allocates and boxes both halves on
    /// every lookup, and these are the most called methods in the server.
    std::map<std::int64_t, std::unique_ptr<server::world::Chunk>> chunksByPos_;
    std::atomic<std::int64_t> chunkVersion_{0};

    PrimedTntQueue primedTntQueue_;
    PendingChunkDecorations pendingDecorations_;
    SculkSensorTracker sculkSensors_;
    EntityRegistry entityRegistry_;
    DroppedItemPhysics itemPhysics_;
};

} // namespace basalt::server::runtime::world
