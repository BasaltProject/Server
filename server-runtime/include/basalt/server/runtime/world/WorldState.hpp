#pragma once

#include <basalt/server/runtime/config/MobsConfig.hpp>
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
#include <basalt/server/runtime/world/poi/PoiManager.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/loot/Loot.hpp>
#include <basalt/wire/common/Uuid.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <unordered_set>
#include <utility>
#include <vector>

namespace basalt::server::runtime::plugins {
class EventSink;
}

namespace basalt::server::runtime::world {

/// parity: `RedstoneTorchBlock.RECENT_TOGGLES`.
struct TorchToggle {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int64_t when = 0;
};

/// parity: `ServerLevel.blockEvents`.
struct BlockEvent {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::string block;
    std::int32_t first = 0;
    std::int32_t second = 0;
};

/// parity: what `GameEventDispatcher.post` hands a warden's `VibrationSystem.Listener`.
struct GameEventRecord {
    std::string event;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    std::int64_t gameTime = 0;
    std::optional<wire::common::Uuid> entity;
    std::optional<wire::common::Uuid> projectileOwner;
};

class WorldState {
public:
    /// Nothing when it is already finished and on its way in from a generation worker.
    using LoadOrGenerate = std::function<std::optional<server::world::Chunk>(std::int32_t chunkX, std::int32_t chunkZ)>;

    struct ChunkMemory {
        std::int64_t bytes = 0;
        std::int32_t blockEntities = 0;
        std::int32_t scheduledTicks = 0;
        std::int32_t generatedEntities = 0;
    };

    explicit WorldState(std::vector<server::world::Chunk> initialChunks = {}, LoadOrGenerate loadOrGenerate = {});

    /// parity: `ServerLevel.getPoiManager`, kept in step with every block this world writes.
    [[nodiscard]] poi::PoiManager& poi() const noexcept { return poi_; }

    void setHasSkylight(bool hasSkylight) noexcept { hasSkylight_ = hasSkylight; }

    void setSettlesGeneration(bool settles) noexcept { settlesGeneration_ = settles; }

    WorldState(const WorldState&) = delete;
    WorldState& operator=(const WorldState&) = delete;
    WorldState(WorldState&&) = delete;
    WorldState& operator=(WorldState&&) = delete;
    ~WorldState() = default;

    [[nodiscard]] static std::int64_t key(std::int32_t chunkX, std::int32_t chunkZ);

    /// Backs the ownership check: with one region a wrong-thread access only works by luck.
    void setOwner(sched::Owner* owner) { owner_ = owner; }

    [[nodiscard]] sched::Owner* owner() const noexcept { return owner_; }

    void setEvents(const plugins::EventSink* events, std::string worldName);

    /// Where this world tells the plugins what happens in it; never null.
    [[nodiscard]] const plugins::EventSink& events() const noexcept;

    [[nodiscard]] const std::string& eventWorld() const noexcept { return eventWorld_; }

    void setRegionCount(std::function<std::int32_t()> regionCount) { regionCount_ = std::move(regionCount); }

    /// The global owner, since a fresh chunk belongs to no region and a region's would take every install.
    void setInstallOwner(sched::Owner* installOwner) { installOwner_ = installOwner; }

    /// The chunk and its eight neighbours, pinned until `lightDone`; every call is paired with one `lightDone`.
    [[nodiscard]] bool lightWindowFor(std::int32_t chunkX, std::int32_t chunkZ,
                                      std::array<server::world::Chunk*, 9>& window, bool requireAll = true);

    void lightDone();

    /// Marks the light of a chunk and the eight around it stale and queues it to be worked out again.
    void markLightAround(std::int32_t chunkX, std::int32_t chunkZ);

    [[nodiscard]] bool lightsAsynchronously() const { return static_cast<bool>(onNeighbourhoodComplete_); }

    /// Asks for the chunk's light to be computed off-thread, when its 3x3 stands.
    void requestLight(std::int32_t chunkX, std::int32_t chunkZ);

    /// Called, with the world's lock held, for a chunk whose 3x3 has just become complete or whose light went stale,
    /// with `delayed` set.
    void setOnNeighbourhoodComplete(std::function<void(std::int32_t, std::int32_t, bool)> handler) {
        onNeighbourhoodComplete_ = std::move(handler);
    }

    /// A hook rather than the bus, since this module sits below the plugin one.
    void setOnChunkInstalled(std::function<void(server::world::Chunk&)> handler) {
        const std::scoped_lock lock{chunkInstalledMutex_};
        onChunkInstalled_ = std::move(handler);
    }

    /// parity: `JukeboxSong.comparatorOutput`.
    using SongOutput = std::function<std::int32_t(const std::string& song)>;

    void setSongOutput(SongOutput songOutput) { songOutput_ = std::move(songOutput); }

    [[nodiscard]] std::int32_t songOutput(const std::string& song) const { return songOutput_ ? songOutput_(song) : 0; }

    /// Unset, a container keeps its table and stays empty.
    using LootUnpacker = std::function<void(std::int32_t x, std::int32_t y, std::int32_t z,
                                            const std::optional<wire::common::Uuid>& player)>;

    void setLootUnpacker(LootUnpacker unpacker) { lootUnpacker_ = std::move(unpacker); }

    void unpackLoot(std::int32_t x, std::int32_t y, std::int32_t z,
                    const std::optional<wire::common::Uuid>& player = std::nullopt) const {
        if (lootUnpacker_) {
            lootUnpacker_(x, y, z, player);
        }
    }

    /// parity: `SculkShriekerBlockEntity.tryGetPlayer`.
    using PlayerCheck = std::function<bool(const wire::common::Uuid& entity)>;

    void setPlayerCheck(PlayerCheck playerCheck) { playerCheck_ = std::move(playerCheck); }

    [[nodiscard]] bool leadsToPlayer(const wire::common::Uuid& entity) const {
        return playerCheck_ && playerCheck_(entity);
    }

    [[nodiscard]] std::vector<server::world::Chunk*> chunks() const;

    [[nodiscard]] std::size_t loadedChunkCount() const;

    [[nodiscard]] ChunkMemory chunkMemory() const;

    [[nodiscard]] std::vector<server::world::Chunk*> dirtyChunks() const;

    [[nodiscard]] std::vector<server::world::Chunk*> chunksWithStashedEntities() const;

    [[nodiscard]] std::vector<server::world::Chunk*>
    evictionCandidates(const std::function<bool(std::int32_t chunkX, std::int32_t chunkZ)>& shouldKeep) const;

    [[nodiscard]] std::size_t countMissing(const std::vector<std::pair<std::int32_t, std::int32_t>>& positions) const;

    /// Skips chunks that became dirty again since the caller saved them: evicting one throws away the player's edits.
    std::size_t evictClean(const std::vector<server::world::Chunk*>& chunks);

    [[nodiscard]] std::vector<std::int64_t> loadedChunkKeys() const;

    /// Bumped when a chunk arrives or goes, so the region scheduler recomputes regions only when the set changed.
    [[nodiscard]] std::int64_t chunkSetVersion() const noexcept { return chunkVersion_.load(); }

    [[nodiscard]] bool isLoaded(std::int32_t chunkX, std::int32_t chunkZ) const;

    /// Never generates, so it is safe on a connection thread.
    [[nodiscard]] server::world::Chunk* chunkIfLoaded(std::int32_t chunkX, std::int32_t chunkZ) const;

    server::world::Chunk& getOrGenerateChunk(std::int32_t chunkX, std::int32_t chunkZ);

    [[nodiscard]] std::optional<server::world::Chunk> generateDetached(std::int32_t chunkX, std::int32_t chunkZ);

    /// Answers the chunk that ends up in it: the one already there if two workers raced.
    server::world::Chunk& install(server::world::Chunk chunk);

    void queueBlockEvent(BlockEvent event);

    [[nodiscard]] std::vector<BlockEvent> takeBlockEvents();

    void recordGameEvent(GameEventRecord record);

    [[nodiscard]] std::vector<GameEventRecord> recentGameEvents() const;

    /// parity: `VibrationSystem`'s destination, a block for sensors and an entity for the warden.
    struct ParticleDestination {
        std::optional<std::array<std::int32_t, 3>> block;
        std::optional<std::pair<std::int32_t, float>> entity;

        friend bool operator==(const ParticleDestination&, const ParticleDestination&) = default;
    };

    /// parity: `ServerLevel.sendParticles`; returns whether anyone within range received it.
    using VibrationParticleSender = std::function<bool(
        double x, double y, double z, const ParticleDestination& destination, std::int32_t arrivalTicks)>;

    void setVibrationParticleSender(VibrationParticleSender sender) { vibrationParticleSender_ = std::move(sender); }

    [[nodiscard]] VibrationParticleSender vibrationParticleSender() const { return vibrationParticleSender_; }

    /// parity: `VibrationSystem.Data.reloadVibrationParticle`, runtime only like the memory.
    [[nodiscard]] bool vibrationParticlePending(std::int64_t packedPos) const;
    void setVibrationParticlePending(std::int64_t packedPos, bool pending);

    /// The rest stay in, so one booked while this runs still sees them and collapses against them.
    [[nodiscard]] std::optional<BlockEvent> takeFirstBlockEvent();

    void addTorchToggle(TorchToggle toggle);

    void pruneTorchToggles(std::int64_t now, std::int64_t keepFor);

    [[nodiscard]] std::int32_t torchToggleCount(std::int32_t x, std::int32_t y, std::int32_t z) const;

    /// `installed` runs once the chunk is really in the world, which is not when this returns.
    void installLater(server::world::Chunk chunk, std::function<void()> installed = {});

    [[nodiscard]] std::optional<std::int32_t> blockState(std::int32_t x, std::int32_t y, std::int32_t z) const;

    [[nodiscard]] entity::entityCollision::BlockAt blockAccessor() const;

    /// parity: `Block.UPDATE_SKIP_BLOCK_ENTITY_SIDEEFFECTS`.
    enum class SideEffects : std::uint8_t { kRun, kSkip };

    /// Whether a write keeps the points of interest in step with the blocks.
    enum class PoiTracking : std::uint8_t { kTrack, kSkip };

    bool setBlockState(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                       SideEffects sideEffects = SideEffects::kRun, PoiTracking poiTracking = PoiTracking::kTrack);

    /// Copies a section's 4096 state ids under the read lock, so any thread may call it. False when it is not loaded.
    [[nodiscard]] bool copySection(std::int32_t chunkX, std::int32_t sectionY, std::int32_t chunkZ,
                                   std::span<std::int32_t> out) const;

    /// Copies every section of a chunk, lowest first, under one read lock. False when the chunk is not loaded.
    [[nodiscard]] bool copySections(std::int32_t chunkX, std::int32_t chunkZ, std::span<std::int32_t> out) const;

    /// Writes one section in one pass like `setBlockState` minus item spills; returns blocks changed, -1 if absent.
    std::int32_t replaceSection(std::int32_t chunkX, std::int32_t sectionY, std::int32_t chunkZ,
                                std::span<const std::int32_t> next, std::span<std::int32_t> previous,
                                std::vector<std::uint16_t>* changedOffsets = nullptr);

    /// Moves on whenever a block or the set of chunks changes, so a reader may keep what it copied while it holds.
    [[nodiscard]] std::uint64_t blockVersion() const noexcept { return blockVersion_.load(std::memory_order_acquire); }

    [[nodiscard]] bool hasSculkSensorNear(double x, double y, double z, std::int32_t range) const;

    [[nodiscard]] std::vector<std::array<std::int32_t, 3>>
    sculkSensorsWithin(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t range) const {
        return sculkSensors_.within(x, y, z, range);
    }

    [[nodiscard]] std::int32_t widestSculkRange() const noexcept { return sculkSensors_.widestRange(); }

    void noteSculkRange(std::int32_t range) noexcept { sculkSensors_.noteRange(range); }

    static constexpr std::size_t kShapeChangeMemory = 64;

    /// parity: `ServerLevel.sendBlockUpdated`.
    [[nodiscard]] std::uint64_t shapeChangesSince(std::uint64_t seen, std::vector<std::array<std::int32_t, 3>>& changed,
                                                  bool& overflowed) const;

    /// parity: `LevelTicks.schedule`.
    void scheduleFluidTick(std::int32_t x, std::int32_t y, std::int32_t z, const wire::common::Identifier& type,
                           std::int32_t delay);

    void scheduleBlockTick(std::int32_t x, std::int32_t y, std::int32_t z, const wire::common::Identifier& type,
                           std::int32_t delay, std::int32_t priority = 0);

    /// parity: `LevelTicks.hasScheduledTick`.
    [[nodiscard]] bool hasScheduledBlockTick(std::int32_t x, std::int32_t y, std::int32_t z,
                                             const wire::common::Identifier& type) const;

    /// parity: `ChunkMap.onFullChunkStatusChange`.
    using Ticking = std::function<bool(std::int32_t chunkX, std::int32_t chunkZ)>;

    /// parity: the jar books an absolute `triggerTick`.
    void openTurn(const Ticking& ticking = {});

    [[nodiscard]] std::vector<std::array<std::int32_t, 3>> dueFluidTicks(const Ticking& ticking = {});

    struct DueTick {
        std::int32_t x = 0;
        std::int32_t y = 0;
        std::int32_t z = 0;
        wire::common::Identifier type = wire::common::Identifier::vanilla("air");
    };

    [[nodiscard]] std::vector<std::array<std::int32_t, 3>> dueBlockTicks(const Ticking& ticking = {});

    /// parity: `ServerLevel.tickBlock`.
    [[nodiscard]] std::vector<DueTick> dueBlockTicksTyped(const Ticking& ticking = {});

    /// parity: `LevelTicks.willTickThisTick`.
    [[nodiscard]] bool willTickThisTick(std::int32_t x, std::int32_t y, std::int32_t z,
                                        const wire::common::Identifier& type) const;

    /// parity: `LevelTicks.runCollectedTicks` takes the turn out of that set before running it.
    void beginBlockTick(std::int32_t x, std::int32_t y, std::int32_t z);

    /// parity: `LevelTicks.cleanupAfterTick`.
    void endBlockTicks();

    /// parity: `moving_piston`.
    void noteSlideStarted(std::int32_t x, std::int32_t y, std::int32_t z);

    [[nodiscard]] bool slideStartedThisTick(std::int32_t x, std::int32_t y, std::int32_t z) const;

    void clearSlidesStarted();

    [[nodiscard]] std::vector<GeneratedWorldEntity>
    generatedEntities(const std::function<std::int32_t()>& allocateEntityId, std::int32_t centerChunkX,
                      std::int32_t centerChunkZ, std::int32_t radiusChunks);

    DroppedItem& spawnItem(DroppedItem dropped);

    [[nodiscard]] std::vector<DroppedItem> items() const;

    [[nodiscard]] std::vector<DroppedItem> nearbyItems(double x, double y, double z, double radius) const;

    [[nodiscard]] std::optional<DroppedItem> removeItem(std::int32_t entityId);

    /// parity: `ItemEntity.hurtServer`. Answers whether the stack is gone.
    bool hurtItem(std::int32_t entityId, std::int32_t amount);

    bool setItemCount(std::int32_t entityId, std::int32_t count);

    bool shiftItem(std::int32_t entityId, double dx, double dy, double dz);

    [[nodiscard]] std::vector<std::int32_t> tickItems(const sched::Region* region = nullptr);

    [[nodiscard]] std::vector<entity::ServerEntity*> entities() const;

    [[nodiscard]] bool hasNaturalSpawnEntityObstruction(const entity::Aabb& spawnBox,
                                                        const entity::Aabb& entityBox) const;

    [[nodiscard]] std::vector<entity::ServerEntity*> entitiesIn(std::int32_t chunkX, std::int32_t chunkZ) const;

    [[nodiscard]] std::vector<std::pair<std::int32_t, std::int32_t>> chunksWithEntities() const;

    void forgetEntitiesIn(std::int32_t chunkX, std::int32_t chunkZ);

    void addPendingEntities(std::vector<wire::nbt::NbtCompound> saved);

    [[nodiscard]] std::vector<wire::nbt::NbtCompound> takePendingEntities();

    [[nodiscard]] std::vector<server::world::ChunkGeneratedEntity> takePendingGeneratedEntities();

    [[nodiscard]] entity::ServerEntity* entity(std::int32_t entityId) const;

    entity::ServerEntity& spawnEntity(std::unique_ptr<entity::ServerEntity> entity);

    void setMobSettings(const config::MobSettings* settings);

    /// This world's mob settings, vanilla's until the owning world supplies its own.
    [[nodiscard]] const config::MobSettings& mobSettings() const noexcept;

    [[nodiscard]] std::vector<std::int32_t> collectRemovedEntities();

    /// parity: `TntBlock.explode`, which spawns the entity where the block stood.
    void primeTnt(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view event);

    /// Asks the plugins whether the TNT block here may be lit, by `reason` and, if any, `who`.
    [[nodiscard]] bool tntMayPrime(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view reason,
                                   std::string_view who = {}) const;

    [[nodiscard]] std::vector<PrimedTntQueue::Primed> takePrimedTnt();

    /// Queued because the block write already holds this world's lock.
    struct Spilled {
        std::int32_t x = 0;
        std::int32_t y = 0;
        std::int32_t z = 0;
        std::vector<server::world::loot::ItemStack> stacks;
        /// A furnace's cooking not yet paid out. parity: `AbstractFurnaceBlockEntity.preRemoveSideEffects`.
        std::vector<std::pair<std::string, std::int32_t>> recipesUsed;
        /// parity: `Containers.dropContents` reads the slots, which unpacks the table first.
        std::optional<std::string> lootTable;
        std::int64_t lootSeed = 0;
        std::vector<std::optional<server::world::loot::ItemStack>> lootSlots;
    };

    [[nodiscard]] std::vector<Spilled> takeSpilledContents();

    /// parity: `StructureBlock.neighborChanged`.
    void queueStructureTrigger(std::int32_t x, std::int32_t y, std::int32_t z);

    [[nodiscard]] std::vector<std::array<std::int32_t, 3>> takeStructureTriggers();

    /// parity: `TestBlockEntity.triggered`, which is kept in memory and never saved.
    void setTestTriggered(std::int32_t x, std::int32_t y, std::int32_t z, bool triggered);

    [[nodiscard]] bool testTriggered(std::int32_t x, std::int32_t y, std::int32_t z) const;

private:
    bool hasSkylight_ = true;
    bool settlesGeneration_ = true;
    void mine(std::string_view what) const;

    [[nodiscard]] std::optional<std::int32_t> blockStateLocked(std::int32_t x, std::int32_t y, std::int32_t z) const;

    [[nodiscard]] entity::entityCollision::BlockAt lockedBlockAccessor() const;

    [[nodiscard]] server::world::Chunk* chunkAtLocked(std::int64_t chunkKey) const;
    [[nodiscard]] std::vector<server::world::Chunk*> snapshot() const;
    [[nodiscard]] std::vector<std::array<std::int32_t, 3>>
    dueTicks(const std::function<void(server::world::Chunk&, std::vector<server::world::ChunkScheduledTick>&)>& advance,
             bool remember, const Ticking& ticking);
    server::world::Chunk& installLocked(server::world::Chunk chunk);

    struct CollectedTick {
        std::int32_t x;
        std::int32_t y;
        std::int32_t z;
        wire::common::Identifier type;
    };

    std::vector<CollectedTick> collectedBlockTicks_;

    std::vector<std::array<std::int32_t, 3>> slidesStarted_;

    mutable WorldMutex mutex_;
    std::atomic<std::uint64_t> blockVersion_{0};
    LoadOrGenerate loadOrGenerate_;
    SongOutput songOutput_;
    LootUnpacker lootUnpacker_;
    PlayerCheck playerCheck_;
    sched::Owner* owner_ = nullptr;
    sched::Owner* installOwner_ = nullptr;

    struct PendingInstall {
        std::shared_ptr<server::world::Chunk> chunk;
        std::function<void()> installed;
    };

    /// Installs generated chunks until the tick's time budget is spent, then asks the owner for another turn.
    void installPass(sched::Owner* target);

    std::mutex installMutex_;
    std::deque<PendingInstall> pendingInstalls_;
    bool installPassQueued_ = false;
    std::function<std::int32_t()> regionCount_;

    [[nodiscard]] std::function<void(server::world::Chunk&)> chunkInstalledHandler() const {
        const std::scoped_lock lock{chunkInstalledMutex_};
        return onChunkInstalled_;
    }

    mutable std::mutex chunkInstalledMutex_;
    std::function<void(server::world::Chunk&)> onChunkInstalled_;
    std::function<void(std::int32_t, std::int32_t, bool)> onNeighbourhoodComplete_;

    std::map<std::int64_t, std::unique_ptr<server::world::Chunk>> chunksByPos_;
    /// parity: the counter behind `ScheduledTick.subTickOrder`.
    std::int64_t nextTickOrder_ = 0;
    std::atomic<std::int64_t> chunkVersion_{0};
    std::atomic<std::int32_t> lightBusy_{0};
    std::mutex graveyardMutex_;
    std::vector<std::unique_ptr<server::world::Chunk>> graveyard_;

    const plugins::EventSink* events_ = nullptr;
    std::string eventWorld_;
    PrimedTntQueue primedTntQueue_;
    std::vector<Spilled> spilled_;
    std::vector<std::array<std::int32_t, 3>> structureTriggers_;
    std::set<std::array<std::int32_t, 3>> triggeredTestBlocks_;
    PendingChunkDecorations pendingDecorations_;
    std::vector<BlockEvent> blockEvents_;
    std::vector<GameEventRecord> gameEvents_;
    VibrationParticleSender vibrationParticleSender_;
    std::unordered_set<std::int64_t> pendingVibrationParticles_;
    std::deque<TorchToggle> torchToggles_;
    SculkSensorTracker sculkSensors_;
    std::array<std::array<std::int32_t, 3>, kShapeChangeMemory> shapeChanges_{};
    std::uint64_t shapeChangeCount_ = 0;
    mutable poi::PoiManager poi_;
    EntityRegistry entityRegistry_;
    const config::MobSettings* mobSettings_ = nullptr;
    DroppedItemPhysics itemPhysics_;
};

} // namespace basalt::server::runtime::world
