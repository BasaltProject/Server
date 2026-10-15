#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/entity/Raids.hpp>
#include <basalt/server/runtime/sched/RegionScheduler.hpp>
#include <basalt/server/runtime/world/ChunkGenerationQueue.hpp>
#include <basalt/server/runtime/world/DragonFight.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
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
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::runtime::world {

/// What a caller hands over besides the world's own declaration.
struct LoadedWorldOptions {
    /// Where this world's generation and saving are recorded. Off unless `/timings` is on.
    wire::common::Timings* timings = nullptr;
    const server::world::terrain::NoiseRouter* overworldRouter = nullptr;
    const server::world::terrain::surface::SurfaceRuleEnvironment* surfaceEnvironment = nullptr;
    server::world::terrain::VillageStructureGenerator* villageGenerator = nullptr;
    server::world::terrain::VillageStructureGenerator* campGenerator = nullptr;
    std::vector<StructureLocator> structureLocators;
    /// The single-generator structures `/place structure` can generate on the spot.
    std::vector<StructurePlacer> structurePlacers;
    server::world::terrain::StrongholdStructureGenerator* strongholdGenerator = nullptr;
    /// Where the climate search says this world's spawn chunk is; the origin without one.
    std::function<std::pair<std::int32_t, std::int32_t>()> spawnChunkFinder;
    /// The generation steps the server built for this dimension, which is what a real server passes.
    /// The world fills in the loading, finishing and saving hooks itself.
    server::world::terrain::ProtoChunks::Hooks pipeline;
    /// parity: `ChunkGenerator.spawnOriginalMobs`, run as a generated chunk goes full.
    std::function<void(server::world::Chunk&, std::int64_t gameTime, const std::string& difficulty)> spawnMobs;
    std::int32_t generationWorkers = 1;
    /// Who hands out the regions this world is ticked in. The server passes its own, so every
    /// world shares one scheduler and one global owner.
    sched::RegionScheduler* scheduler = nullptr;
    /// Whether this is one of the default save's three dimensions, which keep vanilla's level keys.
    bool vanillaKey = false;
};

/// One world, and everything that belongs to exactly one world: its chunks, region files,
/// generation workers, settings and seed, none of them reachable without saying which world.
///
/// The player list, the database and the command dispatcher stay shared, being not per world.
class LoadedWorld {
public:
    /// parity: `ChunkGenerator.getSpawnHeight`, a flat 64 that only stands in if the spiral fails.
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

    /// The dragon fight, or null for a world whose dimension has none. parity:
    /// `DimensionType.hasEnderDragonFight`, which only the End's has.
    [[nodiscard]] DragonFight* dragonFight() noexcept { return dragonFight_.get(); }

    /// The named random streams this world keeps, which is what a loot table rolls on.
    [[nodiscard]] RandomSequences& randomSequences() noexcept { return randomSequences_; }

    /// The level's own stream, held for as long as the lease lives so a region at a time draws.
    struct RandomLease {
        std::unique_lock<std::mutex> lock;
        random::RandomSource& random;
    };

    /// parity: `Level.random`, one stream for the whole level seeded apart from the world seed, as
    /// `RandomSource.create` is.
    [[nodiscard]] RandomLease leaseRandom() { return RandomLease{std::unique_lock{randomMutex_}, random_}; }

    /// parity: `ServerLevel.getRaids`, saved beside the regions.
    [[nodiscard]] entity::RaidManager& raids() noexcept { return raids_; }

    /// The region this world is ticked in. One per world while the world is in one piece.
    [[nodiscard]] sched::Region& region() const noexcept { return *region_; }

    /// Who owns this world's state: its chunks, its blocks and everything living in them. The tick
    /// thread claims it on its first turn; anybody else goes through `Owner::submit`.
    [[nodiscard]] sched::Owner& owner() const noexcept { return region_->owner(); }

    /// Who owns the block at these coordinates: always `owner` while a world is one region. Not to be
    /// kept across turns, since regions can merge between them.
    [[nodiscard]] sched::Owner& ownerOf(std::int32_t x, std::int32_t z) const;

    /// What the client calls this world: a `ResourceKey<Level>`, not a dimension type. It is what tells
    /// the client to rebuild its chunk cache on travel. The default world and its Nether and End keep
    /// `minecraft:overworld`, `the_nether` and `the_end`, so a single-world setup looks like vanilla.
    [[nodiscard]] const std::string& levelKey() const noexcept { return levelKey_; }

    /// This world's own directory, which is where `level.nbt` lives.
    [[nodiscard]] const std::string& worldRoot() const noexcept { return worldRoot_; }

    [[nodiscard]] RegionStore& regions() noexcept { return regions_; }

    /// Where this world's portals are, including the ones in chunks nobody has loaded.
    [[nodiscard]] PortalIndex& portals() noexcept { return portals_; }

    [[nodiscard]] WorldState& state() noexcept { return state_; }

    [[nodiscard]] const WorldState& state() const noexcept { return state_; }

    [[nodiscard]] ChunkGenerationQueue& generationQueue() noexcept { return generationQueue_; }

    /// Where a player joining this world lands, resolved once the spawn column has been generated.
    [[nodiscard]] const std::array<double, 3>& spawn() const noexcept { return spawn_; }

    /// Where spawning puts a player from now on, and what `level.nbt` remembers. `/setworldspawn`.
    [[nodiscard]] float spawnYaw() const noexcept { return spawnYaw_; }

    void setSpawn(std::int32_t x, std::int32_t y, std::int32_t z, float yaw = 0.0F);

    /// This world's clock, weather and game rules: what `/time`, `/weather` and `/gamerule` change.
    /// Vanilla keeps the clock per dimension type; here all three are per world, which is the same on
    /// a single-world server and still works with two overworlds.
    [[nodiscard]] WorldClock& clock() noexcept { return clock_; }

    [[nodiscard]] const WorldClock& clock() const noexcept { return clock_; }

    [[nodiscard]] WorldWeather& weather() noexcept { return weather_; }

    [[nodiscard]] const WorldWeather& weather() const noexcept { return weather_; }

    /// How much the sky is dimmed right now, as it stood when this turn began.
    ///
    /// parity: `Level.getSkyDarken`, written once a turn by `updateSkyBrightness` before the clock
    /// moves, so detectors and spawning rules agree within a turn.
    [[nodiscard]] std::int32_t skyDarken() const noexcept { return skyDarken_; }

    /// parity: `Level.updateSkyBrightness`.
    void updateSkyBrightness();

    [[nodiscard]] GameRuleValues& gameRules() noexcept { return gameRules_; }

    [[nodiscard]] const GameRuleValues& gameRules() const noexcept { return gameRules_; }

    /// The biome source, when this world has a real one: what `/locate biome` searches with.
    ///
    /// Absent for flat, void and generator-less worlds, where "the biome at a position" is whatever
    /// the chunk was written with rather than something a sampler can answer.
    [[nodiscard]] const server::world::terrain::OverworldBiomeSource* biomeSource() const noexcept {
        return biomeSource_.get();
    }

    /// Seeded block-biome zoom over the raw quart palettes held by loaded chunks.
    [[nodiscard]] const server::world::terrain::BiomeZoomer& biomeZoomer() const noexcept { return biomeZoomer_; }

    [[nodiscard]] const server::world::terrain::surface::SurfaceRuleEnvironment* surfaceEnvironment() const noexcept {
        return surfaceEnvironment_;
    }

    [[nodiscard]] server::world::terrain::VillageStructureGenerator* villageGenerator() const noexcept {
        return villageGenerator_;
    }

    /// The jigsaw set generator (villages or abandoned camps) that makes `structure`, or null.
    [[nodiscard]] server::world::terrain::VillageStructureGenerator*
    structureSetGenerator(const wire::common::Identifier& structure) const;

    /// Every structure the jigsaw set generators make, which is what `/locate` and `/place` accept.
    [[nodiscard]] std::vector<wire::common::Identifier> structureSetIds() const;

    /// The single-generator structures `/locate` can find besides the sets and the stronghold.
    [[nodiscard]] const std::vector<StructureLocator>& structureLocators() const noexcept { return structureLocators_; }

    /// The single-generator structures `/place structure` can generate besides the sets.
    [[nodiscard]] const std::vector<StructurePlacer>& structurePlacers() const noexcept { return structurePlacers_; }

    [[nodiscard]] server::world::terrain::StrongholdStructureGenerator* strongholdGenerator() const noexcept {
        return strongholdGenerator_;
    }

    /// This world's border. Not persisted yet, so it starts at vanilla's default size each run.
    [[nodiscard]] WorldBorder& border() noexcept { return border_; }

    /// Chunks `/forceload` keeps resident. Consulted by the unload sweep, which is the whole point:
    /// a forced chunk stays generated whether or not a player is anywhere near it.
    [[nodiscard]] std::set<std::int64_t>& forcedChunks() noexcept { return forcedChunks_; }

    [[nodiscard]] const std::set<std::int64_t>& forcedChunks() const noexcept { return forcedChunks_; }

    /// Ticks this world has been running, which is not the same as its clock: `/time set` moves
    /// one, not the other.
    [[nodiscard]] std::int64_t gameTime() const noexcept { return gameTime_; }

    /// What `/difficulty` last set, falling back to `worlds.toml`.
    [[nodiscard]] const std::string& difficulty() const noexcept { return difficulty_; }

    void setDifficulty(std::string value) { difficulty_ = std::move(value); }

    [[nodiscard]] bool difficultyLocked() const noexcept { return difficultyLocked_; }

    void setDifficultyLocked(bool value) noexcept { difficultyLocked_ = value; }

    /// What `/defaultgamemode` last set here, or unset while it follows `server.toml`.
    [[nodiscard]] const std::optional<std::string>& defaultGamemode() const noexcept { return defaultGamemode_; }

    void setDefaultGamemode(std::optional<std::string> value) { defaultGamemode_ = std::move(value); }

    /// One tick of everything that advances on its own. Answers true when clients need a time
    /// update. The random is the weather's: vanilla draws its next delay from the level's own.
    bool tickTime(random::RandomSource& random);

    /// Rewrites `level.nbt` with the state as it stands now.
    void saveMetadata();

    /// Picks the spawn block, generating only the chunks that search reads. Skipped for a void world
    /// (its platform sits under the saved point) and for a spawn moved by `/setworldspawn`.
    void warmSpawnArea();

    void save(const std::vector<server::world::Chunk*>& chunks);

    void saveDirty();

    /// Drops chunks no player in *this* world can reach, after saving whatever changed in them.
    ///
    /// `playerPositions` holds only the players in this world.
    std::size_t unloadUnreachable(const std::vector<std::pair<std::int32_t, std::int32_t>>& playerPositions,
                                  std::int32_t spawnRadiusChunks, std::int32_t retainMarginChunks);

    void stopGenerating(std::int64_t drainTimeoutMillis);

    /// Nothing else runs here. What was queued is cancelled rather than run, which is what a caller
    /// that has to know asks its handle about.
    void closeOwner();

    void flushRegions();

private:
    [[nodiscard]] server::world::Chunk generateChunk(std::int32_t chunkX, std::int32_t chunkZ);

    [[nodiscard]] std::optional<server::world::Chunk> loadOrGenerateChunk(std::int32_t chunkX, std::int32_t chunkZ);

    [[nodiscard]] std::pair<std::int32_t, std::int32_t> climateSpawnChunk();

    [[nodiscard]] std::array<double, 3> spawnBlock(std::int32_t chunkX, std::int32_t chunkZ);

    void stashEntities(const std::vector<server::world::Chunk*>& chunks);

    /// parity: `RandomSupport.generateUniqueSeed`, what `RandomSource.create` starts from.
    [[nodiscard]] static random::RandomSource uniqueRandom();

    config::WorldDeclaration declaration_;
    config::WorldSettings settings_;
    WorldMetadata metadata_;
    std::unique_ptr<DragonFight> dragonFight_;
    RandomSequences randomSequences_{metadata_.seed};
    std::mutex randomMutex_;
    random::RandomSource random_ = uniqueRandom();
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
    WorldState state_;
    ChunkGenerationQueue generationQueue_;
    std::array<double, 3> spawn_{8.5, 64.0, 8.5};
    float spawnYaw_ = 0.0F;
    /// Unset until somebody moves the spawn, so an untouched world keeps deriving it from terrain.
    std::optional<std::array<std::int32_t, 3>> spawnOverride_;
    /// Whether the stored spawn was chosen rather than derived, which is what a moved one looks
    /// like on disk.
    bool movedSpawn_ = false;
    WorldClock clock_;
    WorldWeather weather_;
    GameRuleValues gameRules_;
    std::int64_t biomeZoomSeed_ = 0;
    std::unique_ptr<server::world::terrain::OverworldBiomeSource> biomeSource_;
    server::world::terrain::BiomeZoomer biomeZoomer_;
    WorldBorder border_;
    std::set<std::int64_t> forcedChunks_;
    std::int64_t gameTime_ = 0;
    std::int32_t skyDarken_ = 0;
    std::string difficulty_;
    bool difficultyLocked_ = false;
    std::optional<std::string> defaultGamemode_;
};

} // namespace basalt::server::runtime::world
