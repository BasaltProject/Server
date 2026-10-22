#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/entity/Raids.hpp>
#include <basalt/server/runtime/sched/RegionScheduler.hpp>
#include <basalt/server/runtime/world/ChunkGenerationQueue.hpp>
#include <basalt/server/runtime/world/DragonFight.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/OldTerrainBlending.hpp>
#include <basalt/server/runtime/world/PortalIndex.hpp>
#include <basalt/server/runtime/world/RandomSequences.hpp>
#include <basalt/server/runtime/world/RegionStore.hpp>
#include <basalt/server/runtime/world/StructureLocator.hpp>
#include <basalt/server/runtime/world/StructurePlacer.hpp>
#include <basalt/server/runtime/world/WorldBorder.hpp>
#include <basalt/server/runtime/world/WorldClock.hpp>
#include <basalt/server/runtime/world/WorldMetadata.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/BiomeZoomer.hpp>
#include <basalt/server/world/terrain/NoiseRouterData.hpp>
#include <basalt/server/world/terrain/OverworldBiomeSource.hpp>
#include <basalt/server/world/terrain/ProtoChunks.hpp>
#include <basalt/server/world/terrain/StrongholdStructureGenerator.hpp>
#include <basalt/server/world/terrain/VillageStructureGenerator.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/common/timings/Timings.hpp>

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace basalt::server::runtime::plugins {
class EventSink;
}

namespace basalt::server::runtime::world {

struct LoadedWorldOptions {
    wire::common::Timings* timings = nullptr;
    const server::world::terrain::NoiseRouter* overworldRouter = nullptr;
    const server::world::terrain::surface::SurfaceRuleEnvironment* surfaceEnvironment = nullptr;
    server::world::terrain::VillageStructureGenerator* villageGenerator = nullptr;
    server::world::terrain::VillageStructureGenerator* campGenerator = nullptr;
    std::vector<StructureLocator> structureLocators;
    std::vector<StructurePlacer> structurePlacers;
    server::world::terrain::StrongholdStructureGenerator* strongholdGenerator = nullptr;
    std::function<std::pair<std::int32_t, std::int32_t>()> spawnChunkFinder;
    server::world::terrain::ProtoChunks::Hooks pipeline;
    /// parity: `ChunkGenerator.spawnOriginalMobs`, run as a generated chunk goes full.
    std::function<void(server::world::Chunk&, std::int64_t gameTime, const std::string& difficulty)> spawnMobs;
    std::int32_t generationWorkers = 1;
    sched::RegionScheduler* scheduler = nullptr;
    bool vanillaKey = false;
};

/// The player list, the database and the command dispatcher are shared, not per world.
class LoadedWorld {
public:
    static constexpr std::int32_t kDefaultSpawnHeight = 64;

    using Options = LoadedWorldOptions;

    LoadedWorld(config::WorldDeclaration declaration, config::WorldSettings settings, WorldMetadata metadata,
                std::string worldsRoot, wire::common::Logger logger, Options options = {});

    LoadedWorld(const LoadedWorld&) = delete;
    LoadedWorld& operator=(const LoadedWorld&) = delete;
    LoadedWorld(LoadedWorld&&) = delete;
    LoadedWorld& operator=(LoadedWorld&&) = delete;
    ~LoadedWorld();

    [[nodiscard]] const config::WorldDeclaration& declaration() const noexcept { return declaration_; }

    [[nodiscard]] const config::WorldSettings& settings() const noexcept { return settings_; }

    [[nodiscard]] const WorldMetadata& metadata() const noexcept { return metadata_; }

    [[nodiscard]] const std::string& name() const noexcept { return declaration_.name; }

    /// The dragon fight, or null for a world whose dimension has none.
    [[nodiscard]] DragonFight* dragonFight() noexcept { return dragonFight_.get(); }

    [[nodiscard]] RandomSequences& randomSequences() noexcept { return randomSequences_; }

    struct RandomLease {
        std::unique_lock<std::mutex> lock;
        random::RandomSource& random;
    };

    /// parity: `Level.random`.
    [[nodiscard]] RandomLease leaseRandom() { return RandomLease{std::unique_lock{randomMutex_}, random_}; }

    /// parity: `ServerLevel.getRaids`, saved beside the regions.
    [[nodiscard]] entity::RaidManager& raids() noexcept { return raids_; }

    [[nodiscard]] sched::Region& region() const noexcept { return *region_; }

    /// Who owns this world's state: its chunks, its blocks and everything living in them.
    [[nodiscard]] sched::Owner& owner() const noexcept { return region_->owner(); }

    void setEvents(plugins::EventSink* events) noexcept { events_ = events; }

    /// Where the world tells the plugins what happens in it; never null.
    [[nodiscard]] const plugins::EventSink& events() const noexcept;

    /// Who owns the block at these coordinates: always `owner` while a world is one region.
    [[nodiscard]] sched::Owner& ownerOf(std::int32_t x, std::int32_t z) const;

    /// What the client calls this world: a `ResourceKey<Level>`, not a dimension type.
    [[nodiscard]] const std::string& levelKey() const noexcept { return levelKey_; }

    [[nodiscard]] const std::string& worldRoot() const noexcept { return worldRoot_; }

    [[nodiscard]] RegionStore& regions() noexcept { return regions_; }

    [[nodiscard]] PortalIndex& portals() noexcept { return portals_; }

    [[nodiscard]] WorldState& state() noexcept { return state_; }

    [[nodiscard]] const WorldState& state() const noexcept { return state_; }

    [[nodiscard]] ChunkGenerationQueue& generationQueue() noexcept { return generationQueue_; }

    [[nodiscard]] const std::array<double, 3>& spawn() const noexcept { return spawn_; }

    [[nodiscard]] float spawnYaw() const noexcept { return spawnYaw_; }

    void setSpawn(std::int32_t x, std::int32_t y, std::int32_t z, float yaw = 0.0F);

    /// Vanilla keeps the clock per dimension type; here clock, weather and game rules are per world.
    [[nodiscard]] WorldClock& clock() noexcept { return clock_; }

    void setAttributeOverride(const std::string& name, std::optional<double> value);

    [[nodiscard]] std::optional<double> attributeOverride(std::string_view name) const;

    [[nodiscard]] const WorldClock& clock() const noexcept { return clock_; }

    [[nodiscard]] WorldWeather& weather() noexcept { return weather_; }

    [[nodiscard]] const WorldWeather& weather() const noexcept { return weather_; }

    /// How much the sky is dimmed right now, as it stood when this turn began.
    /// parity: `Level.getSkyDarken`.
    [[nodiscard]] std::int32_t skyDarken() const noexcept { return skyDarken_; }

    void updateSkyBrightness();

    [[nodiscard]] GameRuleValues& gameRules() noexcept { return gameRules_; }

    [[nodiscard]] const GameRuleValues& gameRules() const noexcept { return gameRules_; }

    /// Absent for flat, void and generator-less worlds, where the biome is whatever the chunk was written with.
    [[nodiscard]] const server::world::terrain::OverworldBiomeSource* biomeSource() const noexcept {
        return biomeSource_.get();
    }

    [[nodiscard]] const server::world::terrain::BiomeZoomer& biomeZoomer() const noexcept { return biomeZoomer_; }

    [[nodiscard]] const server::world::terrain::surface::SurfaceRuleEnvironment* surfaceEnvironment() const noexcept {
        return surfaceEnvironment_;
    }

    [[nodiscard]] server::world::terrain::VillageStructureGenerator* villageGenerator() const noexcept {
        return villageGenerator_;
    }

    [[nodiscard]] server::world::terrain::VillageStructureGenerator*
    structureSetGenerator(const wire::common::Identifier& structure) const;

    [[nodiscard]] std::vector<wire::common::Identifier> structureSetIds() const;

    [[nodiscard]] const std::vector<StructureLocator>& structureLocators() const noexcept { return structureLocators_; }

    [[nodiscard]] const std::vector<StructurePlacer>& structurePlacers() const noexcept { return structurePlacers_; }

    [[nodiscard]] server::world::terrain::StrongholdStructureGenerator* strongholdGenerator() const noexcept {
        return strongholdGenerator_;
    }

    /// This world's border. Not persisted yet, so it starts at vanilla's default size each run.
    [[nodiscard]] WorldBorder& border() noexcept { return border_; }

    /// Consulted by the unload sweep: a forced chunk stays generated whether or not a player is near.
    [[nodiscard]] std::set<std::int64_t>& forcedChunks() noexcept { return forcedChunks_; }

    [[nodiscard]] const std::set<std::int64_t>& forcedChunks() const noexcept { return forcedChunks_; }

    /// Keeps chunks (packed keys) from being unloaded for the holder `token` until `releaseChunks`.
    void holdChunks(const std::string& token, const std::vector<std::int64_t>& keys);

    void releaseChunks(const std::string& token);

    [[nodiscard]] bool isHeld(std::int32_t chunkX, std::int32_t chunkZ) const;

    /// Not the same as the clock: `/time set` moves one, not the other.
    [[nodiscard]] std::int64_t gameTime() const noexcept { return gameTime_; }

    [[nodiscard]] const std::string& difficulty() const noexcept { return difficulty_; }

    void setDifficulty(std::string value) { difficulty_ = std::move(value); }

    [[nodiscard]] bool difficultyLocked() const noexcept { return difficultyLocked_; }

    void setDifficultyLocked(bool value) noexcept { difficultyLocked_ = value; }

    [[nodiscard]] const std::optional<std::string>& defaultGamemode() const noexcept { return defaultGamemode_; }

    void setDefaultGamemode(std::optional<std::string> value) { defaultGamemode_ = std::move(value); }

    /// The random is the weather's: vanilla draws its next delay from the level's own.
    bool tickTime(random::RandomSource& random);

    void saveMetadata();

    /// Skipped for a void world (its platform sits under the saved point) and for a spawn moved by `/setworldspawn`.
    void warmSpawnArea();

    void save(const std::vector<server::world::Chunk*>& chunks);

    void saveDirty();

    /// The encoding and writing half of a save, for snapshots `snapshotOf` made on the owner.
    void writeSnapshots(std::vector<RegionStore::Snapshot> snapshots);

    /// `playerPositions` holds only the players in this world.
    std::size_t unloadUnreachable(const std::vector<std::pair<std::int32_t, std::int32_t>>& playerPositions,
                                  std::int32_t spawnRadiusChunks, std::int32_t retainMarginChunks);

    void stopGenerating(std::int64_t drainTimeoutMillis);

    /// What was queued is cancelled rather than run.
    void closeOwner();

    void flushRegions();

private:
    [[nodiscard]] server::world::Chunk generateChunk(std::int32_t chunkX, std::int32_t chunkZ);

    [[nodiscard]] std::optional<server::world::Chunk> loadOrGenerateChunk(std::int32_t chunkX, std::int32_t chunkZ);
    void restoreBelowZero(server::world::Chunk& chunk);

    [[nodiscard]] std::pair<std::int32_t, std::int32_t> climateSpawnChunk();

    [[nodiscard]] std::array<double, 3> spawnBlock(std::int32_t chunkX, std::int32_t chunkZ);

    void stashEntities(const std::vector<server::world::Chunk*>& chunks);

    /// Owner thread only: the entities stashed, the NBT of each chunk built and the chunk marked clean.
    [[nodiscard]] std::vector<RegionStore::Snapshot> snapshotOf(const std::vector<server::world::Chunk*>& chunks);

    /// A few chunks per owner turn, so a long save never holds the tick.
    [[nodiscard]] std::vector<RegionStore::Snapshot>
    snapshotInBatches(const std::vector<std::pair<std::int32_t, std::int32_t>>& positions);

    /// parity: `RandomSupport.generateUniqueSeed`, what `RandomSource.create` starts from.
    [[nodiscard]] static random::RandomSource uniqueRandom();

    config::WorldDeclaration declaration_;
    config::WorldSettings settings_;
    WorldMetadata metadata_;
    std::unique_ptr<DragonFight> dragonFight_;
    RandomSequences randomSequences_{metadata_.seed};
    std::mutex randomMutex_;
    random::RandomSource random_ = uniqueRandom();
    plugins::EventSink* events_ = nullptr;
    entity::RaidManager raids_;
    wire::common::Logger logger_;
    wire::common::Timings ownTimings_;
    wire::common::Timings& timings_;
    const server::world::terrain::NoiseRouter* overworldRouter_ = nullptr;
    const server::world::terrain::surface::SurfaceRuleEnvironment* surfaceEnvironment_ = nullptr;
    server::world::terrain::VillageStructureGenerator* villageGenerator_ = nullptr;
    server::world::terrain::VillageStructureGenerator* campGenerator_ = nullptr;
    std::vector<StructureLocator> structureLocators_;
    std::vector<StructurePlacer> structurePlacers_;
    server::world::terrain::StrongholdStructureGenerator* strongholdGenerator_ = nullptr;
    std::function<std::pair<std::int32_t, std::int32_t>()> spawnChunkFinder_;
    std::function<void(server::world::Chunk&, std::int64_t, const std::string&)> spawnMobs_;
    sched::RegionScheduler ownScheduler_;
    sched::RegionScheduler& scheduler_;
    sched::Region* region_ = nullptr;
    std::string levelKey_;
    std::string worldRoot_;
    RegionStore regions_;
    PortalIndex portals_;
    /// Declared ahead of the queue whose workers step it, so it outlives them.
    std::unique_ptr<server::world::terrain::ProtoChunks> protoChunks_;
    std::function<server::world::terrain::realTerrainGenerator::GeneratedTerrain(
        std::int32_t, std::int32_t, const server::world::blending::Blender*)>
        terrainOf_;
    std::unique_ptr<OldTerrainBlending> oldTerrain_;
    WorldState state_;
    ChunkGenerationQueue generationQueue_;
    std::array<double, 3> spawn_{8.5, 64.0, 8.5};
    float spawnYaw_ = 0.0F;
    /// Unset until somebody moves the spawn, so an untouched world keeps deriving it from terrain.
    std::optional<std::array<std::int32_t, 3>> spawnOverride_;
    bool movedSpawn_ = false;
    WorldClock clock_;
    static constexpr std::size_t kSnapshotBatch = 8;
    static constexpr std::size_t kLightQueueLimit = 4096;
    static constexpr std::chrono::milliseconds kLightDebounce{80};
    std::mutex lightQueueMutex_;
    std::condition_variable_any lightWake_;
    std::multimap<std::chrono::steady_clock::time_point, std::int64_t> lightQueue_;
    std::unordered_map<std::int64_t, std::chrono::steady_clock::time_point> lightPending_;
    mutable std::mutex attributeMutex_;
    std::map<std::string, double, std::less<>> attributeOverrides_;
    WorldWeather weather_;
    GameRuleValues gameRules_;
    std::int64_t biomeZoomSeed_ = 0;
    std::unique_ptr<server::world::terrain::OverworldBiomeSource> biomeSource_;
    server::world::terrain::BiomeZoomer biomeZoomer_;
    WorldBorder border_;
    std::set<std::int64_t> forcedChunks_;
    mutable std::mutex holdsMutex_;
    std::map<std::string, std::set<std::int64_t>> holds_;
    std::int64_t gameTime_ = 0;
    std::int32_t skyDarken_ = 0;
    std::string difficulty_;
    bool difficultyLocked_ = false;
    std::optional<std::string> defaultGamemode_;
    std::jthread lightWorker_;
};

} // namespace basalt::server::runtime::world
