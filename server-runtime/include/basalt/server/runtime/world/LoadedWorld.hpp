#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/sched/RegionScheduler.hpp>
#include <basalt/server/runtime/world/ChunkGenerationQueue.hpp>
#include <basalt/server/runtime/world/DragonFight.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/PortalIndex.hpp>
#include <basalt/server/runtime/world/RandomSequences.hpp>
#include <basalt/server/runtime/world/RegionStore.hpp>
#include <basalt/server/runtime/world/WorldBorder.hpp>
#include <basalt/server/runtime/world/WorldClock.hpp>
#include <basalt/server/runtime/world/WorldMetadata.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/BiomeZoomer.hpp>
#include <basalt/server/world/terrain/NeighbourTerrain.hpp>
#include <basalt/server/world/terrain/NoiseRouterData.hpp>
#include <basalt/server/world/terrain/OverworldBiomeSource.hpp>
#include <basalt/server/world/terrain/StrongholdStructureGenerator.hpp>
#include <basalt/server/world/terrain/VillageStructureGenerator.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/common/timings/Timings.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::runtime::world {

/// What a caller hands over besides the world's own declaration.
///
/// A struct because there are a dozen of them and all but a few have a sensible default: a test
/// builds a world with two fields set, and the server fills the rest.
struct LoadedWorldOptions {
    /// Where this world's generation and saving are recorded. Off unless `/timings` is on.
    wire::common::Timings* timings = nullptr;
    const server::world::terrain::OverworldNoiseRouter* overworldRouter = nullptr;
    const server::world::terrain::surface::SurfaceRuleEnvironment* surfaceEnvironment = nullptr;
    server::world::terrain::VillageStructureGenerator* villageGenerator = nullptr;
    server::world::terrain::StrongholdStructureGenerator* strongholdGenerator = nullptr;
    /// Where the climate search says this world's spawn chunk is; the origin without one.
    std::function<std::pair<std::int32_t, std::int32_t>()> spawnChunkFinder;
    /// A generator the server built for this dimension, which is what a real server passes.
    std::function<server::world::Chunk(std::int32_t chunkX, std::int32_t chunkZ, std::int64_t gameTime,
                                       std::string_view difficulty,
                                       const std::function<void(server::world::Chunk&)>& beforeDecoration)>
        normalGenerator;
    std::int32_t generationWorkers = 1;
    /// Who hands out the regions this world is ticked in. The server passes its own, so every
    /// world shares one scheduler and one global owner.
    sched::RegionScheduler* scheduler = nullptr;
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

    /// What a neighbour already queued for a cell of a chunk that has not generated yet, so a
    /// feature reading past the edge sees it instead of undecorated terrain.
    [[nodiscard]] std::optional<std::int32_t> queuedWriteAt(std::int32_t worldX, std::int32_t y,
                                                            std::int32_t worldZ) const {
        return state_.queuedWriteAt(worldX, y, worldZ);
    }

    /// parity: what a neighbour that has already decorated really has at a cell. Vanilla reads the
    /// neighbour's proto chunk, decoration included; the terrain cache only ever serves the chunk as
    /// it was before decorating.
    [[nodiscard]] std::optional<std::int32_t> installedBlockAt(std::int32_t worldX, std::int32_t y,
                                                               std::int32_t worldZ) const {
        return state_.blockState(worldX, y, worldZ);
    }

    [[nodiscard]] const config::WorldDeclaration& declaration() const noexcept { return declaration_; }

    [[nodiscard]] const config::WorldSettings& settings() const noexcept { return settings_; }

    [[nodiscard]] const WorldMetadata& metadata() const noexcept { return metadata_; }

    [[nodiscard]] const std::string& name() const noexcept { return declaration_.name; }

    /// The dragon fight, or null for a world whose dimension has none. parity:
    /// `DimensionType.hasEnderDragonFight`, which only the End's has.
    [[nodiscard]] DragonFight* dragonFight() noexcept { return dragonFight_.get(); }

    /// The named random streams this world keeps, which is what a loot table rolls on.
    [[nodiscard]] RandomSequences& randomSequences() noexcept { return randomSequences_; }

    /// The region this world is ticked in. One per world while the world is in one piece.
    [[nodiscard]] sched::Region& region() const noexcept { return *region_; }

    /// Who owns this world's state: its chunks, its blocks and everything living in them.
    ///
    /// The tick thread claims it on its first turn, and from then on anybody else reaches it with
    /// `Owner::submit` rather than by touching it. One world, one region, one owner today; one
    /// owner per region once the world is split, with the same rule.
    [[nodiscard]] sched::Owner& owner() const noexcept { return region_->owner(); }

    /// Who owns the block at these coordinates.
    ///
    /// One world is one region today, so it is always `owner`. The signature is the point: code
    /// that asks this instead of assuming keeps working when a world is several regions, and the
    /// answer must not be kept from one turn to the next, because a region can merge into another
    /// between two of them.
    [[nodiscard]] sched::Owner& ownerOf(std::int32_t x, std::int32_t z) const;

    /// What the client calls this world: a `ResourceKey<Level>`, not a dimension type.
    ///
    /// The distinction is what makes travelling between worlds work at all. The dimension type is
    /// selected separately by the declaration, while the level key identifies this concrete world
    /// and tells the client when to rebuild its chunk cache.
    ///
    /// The default world keeps `minecraft:overworld` so an unmodified single-world setup looks
    /// exactly as before, including in the client's own debug screen.
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

    /// This world's clock, weather and game rules: everything a `/time`, `/weather` or `/gamerule`
    /// changes and a restart has to remember.
    ///
    /// Vanilla keeps one clock per *dimension type* on the server rather than one per level, and
    /// weather and rules per level. Basalt keeps all three per world, which is the same thing on a
    /// vanilla-shaped single-world server and the only sensible answer once an operator declares
    /// two overworlds, something vanilla cannot do at all.
    [[nodiscard]] WorldClock& clock() noexcept { return clock_; }

    [[nodiscard]] const WorldClock& clock() const noexcept { return clock_; }

    [[nodiscard]] WorldWeather& weather() noexcept { return weather_; }

    [[nodiscard]] const WorldWeather& weather() const noexcept { return weather_; }

    /// How much the sky is dimmed right now, as it stood when this turn began.
    ///
    /// parity: `Level.getSkyDarken`, a field `updateSkyBrightness` writes once a turn before the
    /// clock moves. Everything that reads the sky, a detector and the spawning rules, reads this and
    /// not a fresh sum, so they all agree within a turn.
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

    /// Generates the small warm cache around spawn and picks the spawn block out of it.
    ///
    /// A void world skips the search: its spawn is whatever the metadata says, because there is no
    /// terrain to stand on other than the platform placed under exactly that point. A world whose
    /// spawn was moved by `/setworldspawn` skips it too: the point of moving it is that it is no
    /// longer wherever the terrain happens to be.
    void warmSpawnArea(std::int32_t spawnRadiusChunks);

    void save(const std::vector<server::world::Chunk*>& chunks);

    void saveDirty();

    /// Drops chunks no player in *this* world can reach, after saving whatever changed in them.
    ///
    /// `playerPositions` is only the players who are in this world: keeping a chunk resident
    /// because somebody is standing at the same coordinates in a different world would be nonsense.
    std::size_t unloadUnreachable(const std::vector<std::pair<std::int32_t, std::int32_t>>& playerPositions,
                                  std::int32_t spawnRadiusChunks, std::int32_t retainMarginChunks);

    void stopGenerating(std::int64_t drainTimeoutMillis);

    /// Nothing else runs here. What was queued is cancelled rather than run, which is what a caller
    /// that has to know asks its handle about.
    void closeOwner();

    void flushRegions();

private:
    [[nodiscard]] server::world::Chunk generateChunk(std::int32_t chunkX, std::int32_t chunkZ);

    [[nodiscard]] server::world::Chunk loadOrGenerateChunk(std::int32_t chunkX, std::int32_t chunkZ);

    [[nodiscard]] std::pair<std::int32_t, std::int32_t> climateSpawnChunk();

    [[nodiscard]] std::array<double, 3> spawnBlock(std::int32_t chunkX, std::int32_t chunkZ);

    void stashEntities(const std::vector<server::world::Chunk*>& chunks);

    config::WorldDeclaration declaration_;
    config::WorldSettings settings_;
    WorldMetadata metadata_;
    std::unique_ptr<DragonFight> dragonFight_;
    RandomSequences randomSequences_{metadata_.seed};
    wire::common::Logger logger_;
    wire::common::Timings ownTimings_;
    wire::common::Timings& timings_;
    const server::world::terrain::OverworldNoiseRouter* overworldRouter_ = nullptr;
    const server::world::terrain::surface::SurfaceRuleEnvironment* surfaceEnvironment_ = nullptr;
    server::world::terrain::VillageStructureGenerator* villageGenerator_ = nullptr;
    server::world::terrain::StrongholdStructureGenerator* strongholdGenerator_ = nullptr;
    std::function<std::pair<std::int32_t, std::int32_t>()> spawnChunkFinder_;
    std::function<server::world::Chunk(std::int32_t, std::int32_t, std::int64_t, std::string_view,
                                       const std::function<void(server::world::Chunk&)>&)>
        normalGenerator_;
    sched::RegionScheduler ownScheduler_;
    sched::RegionScheduler& scheduler_;
    sched::Region* region_ = nullptr;
    std::string levelKey_;
    std::string worldRoot_;
    RegionStore regions_;
    PortalIndex portals_;
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
    // Without this the step-1 bodies read the outside as air, and a lake preflight then rejects
    // every volume that is not perfectly centred, so no lake would ever generate here.
    std::unique_ptr<server::world::terrain::NeighbourTerrainCache> fallbackNeighbours_;
};

} // namespace basalt::server::runtime::world
