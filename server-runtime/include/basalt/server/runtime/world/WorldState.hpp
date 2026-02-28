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
#include <basalt/server/world/loot/Loot.hpp>

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `RedstoneTorchBlock.RECENT_TOGGLES`, one list per world of every time a torch went out.
/// Eight at the same square inside sixty ticks and it burns out. The list belongs to the world and
/// not to the torch, so whichever one happens to tick is the one that clears out the old entries.
struct TorchToggle {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int64_t when = 0;
};

/// parity: `ServerLevel.blockEvents`. A piston does not move when the signal reaches it: it books an
/// event, and the event is what moves it, once a tick and only while the world runs. That is why a
/// frozen world leaves a piston standing where it is.
struct BlockEvent {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::string block;
    std::int32_t first = 0;
    std::int32_t second = 0;
};

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
        const std::scoped_lock lock{chunkInstalledMutex_};
        onChunkInstalled_ = std::move(handler);
    }

    /// What a comparator reads off a jukebox playing this song.
    ///
    /// parity: `JukeboxSong.comparatorOutput`, a number that lives in the jar's own data and not in
    /// the world, so it arrives as a callback the same way the chunk source does. Without one every
    /// jukebox reads zero, which is what a world with no jar installed can honestly say.
    using SongOutput = std::function<std::int32_t(const std::string& song)>;

    void setSongOutput(SongOutput songOutput) { songOutput_ = std::move(songOutput); }

    [[nodiscard]] std::int32_t songOutput(const std::string& song) const { return songOutput_ ? songOutput_(song) : 0; }

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

    /// Delivers whatever a neighbour queued for this chunk while it did not exist. parity: vanilla
    /// writes a feature that crosses a chunk edge straight into the neighbour's proto chunk, so the
    /// chunk decorates on top of it and reads it while doing so. Called between terrain and
    /// decoration, which is the same point.
    void applyPendingWritesTo(server::world::Chunk& chunk);

    /// Books one, to be run by whoever ticks this world.
    void queueBlockEvent(BlockEvent event);

    /// Everything booked since the last call, in the order it was booked.
    [[nodiscard]] std::vector<BlockEvent> takeBlockEvents();

    /// The oldest one booked, taken out of the queue. The rest stay in, so one booked while this
    /// runs still sees them and collapses against them.
    [[nodiscard]] std::optional<BlockEvent> takeFirstBlockEvent();

    /// One more time a torch went out here.
    void addTorchToggle(TorchToggle toggle);

    /// Drops the toggles at the front that are older than `keepFor` ticks, which is the only place
    /// they ever leave the list.
    void pruneTorchToggles(std::int64_t now, std::int64_t keepFor);

    /// How many of the toggles still in the list happened at this square.
    [[nodiscard]] std::int32_t torchToggleCount(std::int32_t x, std::int32_t y, std::int32_t z) const;

    /// What a neighbour already queued for a cell of a chunk that has not generated yet.
    [[nodiscard]] std::optional<std::int32_t> queuedWriteAt(std::int32_t worldX, std::int32_t y,
                                                            std::int32_t worldZ) const;

    /// Hands the chunk over to be installed, or installs it here when nobody owns this world.
    /// `installed` runs once the chunk is really in the world, which is not when this returns.
    void installLater(server::world::Chunk chunk, std::function<void()> installed = {});

    [[nodiscard]] std::optional<std::int32_t> blockState(std::int32_t x, std::int32_t y, std::int32_t z) const;

    /// What the collision and spawning code asks the world, as the callback they take.
    [[nodiscard]] entity::entityCollision::BlockAt blockAccessor() const;

    /// parity: `Block.UPDATE_SKIP_BLOCK_ENTITY_SIDEEFFECTS`, the 256 bit of the flags vanilla passes
    /// to `Level.setBlock`. Every command that writes blocks sets it: `/setblock` and `/fill` write
    /// with `2 | 256`, and `/clone` blanks each target with a barrier at 816 before writing the real
    /// state, so a container replaced by a command never drops what it held.
    enum class SideEffects : std::uint8_t { kRun, kSkip };

    /// Whether the position falls inside a loaded chunk, which is when the block was set.
    bool setBlockState(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                       SideEffects sideEffects = SideEffects::kRun);

    [[nodiscard]] bool hasSculkSensorNear(double x, double y, double z, std::int32_t range) const;

    /// Queues a fluid tick that many ticks from now, doing nothing if the chunk is not loaded.
    ///
    /// parity: `LevelTicks.schedule`, minus its priorities: only one tick per position and fluid is
    /// ever pending, and re-scheduling an existing one keeps the delay already counting down.
    void scheduleFluidTick(std::int32_t x, std::int32_t y, std::int32_t z, const wire::common::Identifier& type,
                           std::int32_t delay);

    void scheduleBlockTick(std::int32_t x, std::int32_t y, std::int32_t z, const wire::common::Identifier& type,
                           std::int32_t delay, std::int32_t priority = 0);

    /// parity: `LevelTicks.hasScheduledTick`, which is what stops a target being re-armed mid-hold.
    [[nodiscard]] bool hasScheduledBlockTick(std::int32_t x, std::int32_t y, std::int32_t z,
                                             const wire::common::Identifier& type) const;

    /// Whether a chunk's turns run at all this tick, by chunk coordinates. parity:
    /// `ChunkMap.onFullChunkStatusChange` hands a chunk's `LevelTicks` container over only while it
    /// is at block ticking, so a chunk that is loaded and not ticking neither runs its turns nor
    /// counts them down. Unset means every loaded chunk, which is what the tests want.
    using Ticking = std::function<bool(std::int32_t chunkX, std::int32_t chunkZ)>;

    /// Every fluid tick that comes due this tick, in world coordinates.
    [[nodiscard]] std::vector<std::array<std::int32_t, 3>> dueFluidTicks(const Ticking& ticking = {});

    /// One block turn that comes due, with the block it was booked for.
    struct DueTick {
        std::int32_t x = 0;
        std::int32_t y = 0;
        std::int32_t z = 0;
        wire::common::Identifier type = wire::common::Identifier::vanilla("air");
    };

    /// The same for block ticks: a button that has been held down long enough, and later redstone.
    [[nodiscard]] std::vector<std::array<std::int32_t, 3>> dueBlockTicks(const Ticking& ticking = {});

    /// parity: `ServerLevel.tickBlock` runs a turn only on the block it was booked for, so what runs
    /// has to know which block asked for it.
    [[nodiscard]] std::vector<DueTick> dueBlockTicksTyped(const Ticking& ticking = {});

    /// parity: `LevelTicks.willTickThisTick`, the turns already taken for this tick and still to
    /// run. A diode that hears a neighbour while its own turn is waiting books nothing.
    [[nodiscard]] bool willTickThisTick(std::int32_t x, std::int32_t y, std::int32_t z,
                                        const wire::common::Identifier& type) const;

    /// parity: `LevelTicks.runCollectedTicks` takes the turn out of that set before running it.
    void beginBlockTick(std::int32_t x, std::int32_t y, std::int32_t z);

    /// parity: `LevelTicks.cleanupAfterTick`, which empties the batch when the pass ends. A world
    /// that stops running keeps no turns waiting, and a frozen world books like any other.
    void endBlockTicks();

    /// A square that started sliding in this very turn. parity: a `moving_piston` created now has
    /// `progress` and `progressO` both at zero, so its first ticker turn only moves `progress` and
    /// what is written down does not change. We keep one value, so the first turn is skipped instead.
    void noteSlideStarted(std::int32_t x, std::int32_t y, std::int32_t z);

    [[nodiscard]] bool slideStartedThisTick(std::int32_t x, std::int32_t y, std::int32_t z) const;

    void clearSlidesStarted();

    [[nodiscard]] std::vector<GeneratedWorldEntity>
    generatedEntities(const std::function<std::int32_t()>& allocateEntityId, std::int32_t centerChunkX,
                      std::int32_t centerChunkZ, std::int32_t radiusChunks);

    // --- What lives in the world ---

    DroppedItem& spawnItem(DroppedItem dropped);

    [[nodiscard]] std::vector<DroppedItem> items() const;

    [[nodiscard]] std::vector<DroppedItem> nearbyItems(double x, double y, double z, double radius) const;

    [[nodiscard]] std::optional<DroppedItem> removeItem(std::int32_t entityId);

    /// parity: `ItemEntity.hurtServer`. Answers whether the stack is gone.
    bool hurtItem(std::int32_t entityId, std::int32_t amount);

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
    void primeTnt(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view event);

    [[nodiscard]] std::vector<PrimedTntQueue::Primed> takePrimedTnt();

    /// What a container had in it when its block went, waiting to be turned into items.
    ///
    /// parity: `BlockEntity.preRemoveSideEffects`, which drops the contents of **any** block entity
    /// that is a container, from every block replacement that did not ask to skip it. It is queued
    /// rather than spawned on the spot because the block write already holds this world's lock.
    struct Spilled {
        std::int32_t x = 0;
        std::int32_t y = 0;
        std::int32_t z = 0;
        std::vector<server::world::loot::ItemStack> stacks;
    };

    [[nodiscard]] std::vector<Spilled> takeSpilledContents();

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
    [[nodiscard]] std::vector<std::array<std::int32_t, 3>>
    dueTicks(const std::function<void(server::world::Chunk&, std::vector<server::world::ChunkScheduledTick>&)>& advance,
             bool remember, const Ticking& ticking);
    server::world::Chunk& installLocked(server::world::Chunk chunk);

    /// One turn of this tick's batch, kept only to answer `willTickThisTick`.
    struct CollectedTick {
        std::int32_t x;
        std::int32_t y;
        std::int32_t z;
        wire::common::Identifier type;
    };

    std::vector<CollectedTick> collectedBlockTicks_;

    /// Squares whose slide began in this turn, so the block entity pass leaves them alone once.
    std::vector<std::array<std::int32_t, 3>> slidesStarted_;

    mutable WorldMutex mutex_;
    LoadOrGenerate loadOrGenerate_;
    SongOutput songOutput_;
    sched::Owner* owner_ = nullptr;
    sched::Owner* installOwner_ = nullptr;
    std::function<std::int32_t()> regionCount_;

    /// The handler is installed while chunks are already being generated, so the worker that runs
    /// an install reads it while the main thread is still assigning it. Copied out under the lock
    /// and called outside it, because the handler itself reaches the plugin bus.
    [[nodiscard]] std::function<void(server::world::Chunk&)> chunkInstalledHandler() const {
        const std::scoped_lock lock{chunkInstalledMutex_};
        return onChunkInstalled_;
    }

    mutable std::mutex chunkInstalledMutex_;
    std::function<void(server::world::Chunk&)> onChunkInstalled_;

    /// Keyed by packed coordinates, not by a pair: a pair key allocates and boxes both halves on
    /// every lookup, and these are the most called methods in the server.
    std::map<std::int64_t, std::unique_ptr<server::world::Chunk>> chunksByPos_;
    /// parity: the counter behind `ScheduledTick.subTickOrder`, which is per level and not per chunk.
    std::int64_t nextTickOrder_ = 0;
    std::atomic<std::int64_t> chunkVersion_{0};

    PrimedTntQueue primedTntQueue_;
    std::vector<Spilled> spilled_;
    PendingChunkDecorations pendingDecorations_;
    std::vector<BlockEvent> blockEvents_;
    std::deque<TorchToggle> torchToggles_;
    SculkSensorTracker sculkSensors_;
    EntityRegistry entityRegistry_;
    DroppedItemPhysics itemPhysics_;
};

} // namespace basalt::server::runtime::world
