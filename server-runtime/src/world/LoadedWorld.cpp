#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/world/BiomeZoomSeed.hpp>
#include <basalt/server/runtime/world/DaylightDetectors.hpp>
#include <basalt/server/runtime/world/DroppedItemNbt.hpp>
#include <basalt/server/runtime/world/Lightning.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/WorldSpawnPoint.hpp>
#include <basalt/server/world/LeafDistances.hpp>
#include <basalt/server/world/NoiseWorldGenerator.hpp>
#include <basalt/server/world/SimpleWorldGenerators.hpp>
#include <basalt/server/world/climate/Sampler.hpp>
#include <basalt/server/world/climate/SpawnFinder.hpp>
#include <basalt/server/world/terrain/RealTerrainGenerator.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <set>

namespace basalt::server::runtime::world {
namespace {

namespace terrain = server::world::terrain;

using server::world::Chunk;

/// A world's own key when it is not the default one: its name, with anything a resource location
/// cannot carry replaced.
[[nodiscard]] std::string levelKeyOf(const config::WorldDeclaration& declaration, bool vanillaKey) {
    if (vanillaKey || declaration.isDefault) {
        return config::dimensionRegistryKey(declaration.dimension);
    }
    std::string key = "basalt:";
    for (const char letter : declaration.name) {
        const bool plain =
            (std::isalnum(static_cast<unsigned char>(letter)) != 0) || letter == '.' || letter == '_' || letter == '-';
        key.push_back(plain ? static_cast<char>(std::tolower(static_cast<unsigned char>(letter))) : '_');
    }
    return key;
}

[[nodiscard]] std::int64_t epochSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

} // namespace

LoadedWorld::LoadedWorld(config::WorldDeclaration declaration, config::WorldSettings settings, WorldMetadata metadata,
                         std::string worldsRoot, wire::common::Logger logger, Options options)
    : declaration_(std::move(declaration)), settings_(std::move(settings)), metadata_(std::move(metadata)),
      logger_(std::move(logger)), timings_(options.timings == nullptr ? ownTimings_ : *options.timings),
      overworldRouter_(options.overworldRouter), surfaceEnvironment_(options.surfaceEnvironment),
      villageGenerator_(options.villageGenerator), campGenerator_(options.campGenerator),
      structureLocators_(std::move(options.structureLocators)), strongholdGenerator_(options.strongholdGenerator),
      spawnChunkFinder_(std::move(options.spawnChunkFinder)), normalGenerator_(std::move(options.normalGenerator)),
      scheduler_(options.scheduler == nullptr ? ownScheduler_ : *options.scheduler),
      region_(&scheduler_.regionFor(declaration_.name)), levelKey_(levelKeyOf(declaration_, options.vanillaKey)),
      worldRoot_(worldsRoot + "/" + declaration_.name),
      regions_(declaration_.name, std::move(worldsRoot), config::dimensionFacts(declaration_.dimension).chunkHeight),
      state_({}, [this](std::int32_t chunkX, std::int32_t chunkZ) { return loadOrGenerateChunk(chunkX, chunkZ); }),
      generationQueue_(state_, std::string{config::dimensionFacts(declaration_.dimension).registryPath},
                       options.generationWorkers),
      clock_(metadata_.timeOfDay, metadata_.clockRate, metadata_.clockPaused), gameRules_(metadata_.gameRules),
      biomeZoomSeed_(obfuscateBiomeSeed(metadata_.seed)), biomeZoomer_(biomeZoomSeed_), gameTime_(metadata_.gameTime),
      difficulty_(metadata_.difficulty.value_or(settings_.difficulty)), difficultyLocked_(metadata_.difficultyLocked),
      defaultGamemode_(metadata_.defaultGamemode) {
    state_.setHasSkylight(config::dimensionFacts(declaration_.dimension).hasSkylight);
    state_.poi().configure(worldRoot_ + "/poi", config::dimensionFacts(declaration_.dimension).chunkHeight);
    // Said rather than migrated: a world that looks empty is worse than a line in the log.
    if (wire::io::fileIO::isDirectory(worldRoot_ + "/dimensions")) {
        logger_.warn([this] {
            return "World '" + declaration_.name + "' still has its regions under " + worldRoot_ +
                   "/dimensions/. They are read from " + worldRoot_ +
                   "/region/ now: move the .linear files there, or the world regenerates from its seed.";
        });
    }
    if (declaration_.dimension == config::WorldDimension::End) {
        dragonFight_ = std::make_unique<DragonFight>(
            metadata_.dragonFight.has_value() ? DragonFight::decode(*metadata_.dragonFight) : DragonFight{});
        dragonFight_->shuffleGateways(metadata_.seed);
    }
    weather_.raining = metadata_.raining;
    weather_.rainTime = metadata_.rainTime;
    weather_.thundering = metadata_.thundering;
    weather_.thunderTime = metadata_.thunderTime;
    weather_.clearWeatherTime = metadata_.clearWeatherTime;
    // parity: `TicketType.FORCED` persists, so a `/forceload` is still in force after a restart.
    // Without this the world came back with nothing forced, and with nobody connected that means
    // nothing is simulated at all: a sculk sensor with a noise still on its way never delivered it.
    forcedChunks_.insert(metadata_.forcedChunks.begin(), metadata_.forcedChunks.end());
    // The same as the command does when it forces one: there now, not on the next visit. The
    // simulated list names a forced chunk whether or not it is in memory, and an absent one ticks
    // nothing.
    for (const std::int64_t key : forcedChunks_) {
        generationQueue_.request(ChunkGenerationQueue::unpackX(key), ChunkGenerationQueue::unpackZ(key), 0);
    }
    portals_.load(worldRoot_);
    randomSequences_.load(worldRoot_);
    raids_.load(worldRoot_);
    state_.setOwner(&region_->owner());
    state_.setInstallOwner(&scheduler_.global());
    state_.setRegionCount([this] {
        return std::max<std::int32_t>(1, static_cast<std::int32_t>(scheduler_.regionsOf(declaration_.name).size()));
    });
    movedSpawn_ = std::array<std::int32_t, 3>{metadata_.spawnX, metadata_.spawnY, metadata_.spawnZ} !=
                  WorldMetadata::kDefaultSpawn;
    if (overworldRouter_ != nullptr && declaration_.dimension == config::WorldDimension::Overworld) {
        biomeSource_ = std::make_unique<terrain::OverworldBiomeSource>(*overworldRouter_, biomeZoomSeed_);
    }
    // parity: `ServerLevel`'s constructor ends on `updateSkyBrightness()`, so a world that has not
    // ticked yet already has a reading.
    updateSkyBrightness();
}

LoadedWorld::~LoadedWorld() = default;

server::world::terrain::VillageStructureGenerator*
LoadedWorld::structureSetGenerator(const wire::common::Identifier& structure) const {
    for (server::world::terrain::VillageStructureGenerator* generator : {villageGenerator_, campGenerator_}) {
        if (generator == nullptr) {
            continue;
        }
        const std::vector<wire::common::Identifier> ids = generator->ids();
        if (std::ranges::find(ids, structure) != ids.end()) {
            return generator;
        }
    }
    return nullptr;
}

std::vector<wire::common::Identifier> LoadedWorld::structureSetIds() const {
    std::vector<wire::common::Identifier> ids;
    for (const server::world::terrain::VillageStructureGenerator* generator : {villageGenerator_, campGenerator_}) {
        if (generator != nullptr) {
            std::ranges::copy(generator->ids(), std::back_inserter(ids));
        }
    }
    return ids;
}

sched::Owner& LoadedWorld::ownerOf(std::int32_t x, std::int32_t z) const {
    return scheduler_.regionFor(declaration_.name, x >> 4, z >> 4).owner();
}

void LoadedWorld::setSpawn(std::int32_t x, std::int32_t y, std::int32_t z, float yaw) {
    spawn_ = {x + 0.5, static_cast<double>(y), z + 0.5};
    spawnYaw_ = yaw;
    spawnOverride_ = std::array<std::int32_t, 3>{x, y, z};
}

void LoadedWorld::updateSkyBrightness() {
    const config::DimensionFacts& facts = config::dimensionFacts(declaration_.dimension);
    skyDarken_ = daylightDetectors::skyDarken(
        clock_, weather_, daylightDetectors::Sky{.level = facts.skyLightLevel, .dayTimeline = facts.hasDayTimeline});
}

bool LoadedWorld::tickTime(random::RandomSource& random) {
    // parity: `MinecraftServer.tickChildren` moves the day clock before any level ticks, so the sky
    // reading sees this turn's clock. Only a dimension with a clock of its own advances it.
    const bool advanced =
        config::hasClock(declaration_.dimension) && clock_.tick(gameRules_.get(gameRules::kAdvanceTime));
    // parity: `ServerLevel.advanceWeatherCycle` runs only where the dimension can have weather, and
    // the game rule stops the counters inside it, not the whole thing: the levels keep ramping.
    if (config::canHaveWeather(declaration_.dimension)) {
        weather_.tick(random, gameRules_.get(gameRules::kAdvanceWeather));
    }
    // parity: `ServerLevel.tick` takes the reading after the weather and before `tickTime()`, so it
    // is the clock of this turn and the weather of this turn both.
    updateSkyBrightness();
    gameTime_++;
    return advanced;
}

void LoadedWorld::saveMetadata() {
    const std::array<std::int32_t, 3> spawn =
        spawnOverride_.value_or(std::array<std::int32_t, 3>{metadata_.spawnX, metadata_.spawnY, metadata_.spawnZ});
    WorldMetadata saved = metadata_;
    saved.spawnX = spawn[0];
    saved.spawnY = spawn[1];
    saved.spawnZ = spawn[2];
    saved.timeOfDay = clock_.totalTicks();
    saved.gameTime = gameTime_;
    saved.clockRate = clock_.rate();
    saved.clockPaused = clock_.paused();
    saved.forcedChunks.assign(forcedChunks_.begin(), forcedChunks_.end());
    saved.raining = weather_.raining;
    saved.rainTime = weather_.rainTime;
    saved.thundering = weather_.thundering;
    saved.thunderTime = weather_.thunderTime;
    saved.clearWeatherTime = weather_.clearWeatherTime;
    saved.gameRules = gameRules_;
    saved.difficulty = difficulty_;
    saved.difficultyLocked = difficultyLocked_;
    saved.defaultGamemode = defaultGamemode_;
    if (dragonFight_ != nullptr) {
        saved.dragonFight = dragonFight_->encode();
    }
    metadata_ = saved;
    static_cast<void>(saved.writeTo(worldRoot_));
}

Chunk LoadedWorld::generateChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    const auto started = std::chrono::steady_clock::now();
    const config::DimensionFacts& facts = config::dimensionFacts(declaration_.dimension);
    Chunk chunk = [&] {
        switch (declaration_.type) {
        case config::WorldType::Flat:
            return server::world::flatWorldGenerator::generate(chunkX, chunkZ, facts.chunkHeight);
        case config::WorldType::Void:
            return server::world::voidWorldGenerator::generate(
                chunkX, chunkZ, metadata_.spawnX, metadata_.spawnY, metadata_.spawnZ,
                server::world::voidWorldGenerator::kDefaultPlatformRadius, facts.chunkHeight);
        case config::WorldType::Normal:
        case config::WorldType::Amplified:
        default:
            break;
        }
        // What a neighbour queued for this chunk belongs in it before it decorates, not after it is
        // installed: a feature that reads the block next to it has to see the same world vanilla does.
        const auto deliverQueued = [this](server::world::Chunk& generated) { state_.applyPendingWritesTo(generated); };
        if (normalGenerator_) {
            return normalGenerator_(chunkX, chunkZ, gameTime_, difficulty_, deliverQueued);
        }
        if (overworldRouter_ != nullptr && surfaceEnvironment_ != nullptr) {
            if (fallbackNeighbours_ == nullptr) {
                fallbackNeighbours_ = std::make_unique<terrain::NeighbourTerrainCache>(
                    [this](std::int32_t neighbourX, std::int32_t neighbourZ) {
                        terrain::realTerrainGenerator::GeneratedTerrain generated =
                            terrain::realTerrainGenerator::generateTerrain(
                                *overworldRouter_, *surfaceEnvironment_, neighbourX, neighbourZ, metadata_.seed,
                                terrain::TerrainStructureGenerators{.village = villageGenerator_});
                        return std::move(generated.chunk());
                    });
            }
            return terrain::realTerrainGenerator::generate(
                *overworldRouter_, *surfaceEnvironment_, chunkX, chunkZ, metadata_.seed,
                terrain::TerrainStructureGenerators{.village = villageGenerator_}, fallbackNeighbours_.get(), nullptr,
                deliverQueued);
        }
        // Nothing to generate a real Nether or End with, and a noise approximation of one would be
        // a different world rather than a cheaper one.
        return server::world::noiseWorldGenerator::generate(chunkX, chunkZ);
    }();
    timings_.of("world.generate")
        .record(
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started).count());
    return chunk;
}

Chunk LoadedWorld::loadOrGenerateChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    std::optional<Chunk> stored = regions_.loadChunk(chunkX, chunkZ);
    Chunk chunk = stored.has_value() ? std::move(*stored) : generateChunk(chunkX, chunkZ);
    // What a chunk in memory says about its portals is always the truth; the index only has to
    // remember the chunks that are not.
    portals_.scan(chunk);
    // Off the tick thread, and once: from here on a leaf knows its own distance.
    server::world::leafDistances::assign(chunk);
    return chunk;
}

std::pair<std::int32_t, std::int32_t> LoadedWorld::climateSpawnChunk() {
    if (spawnChunkFinder_) {
        return spawnChunkFinder_();
    }
    if (overworldRouter_ == nullptr) {
        return {0, 0};
    }
    const server::world::climate::Sampler sampler{*overworldRouter_->state,     overworldRouter_->temperature,
                                                  overworldRouter_->vegetation, overworldRouter_->continents,
                                                  overworldRouter_->erosion,    overworldRouter_->depth,
                                                  overworldRouter_->ridges};
    const std::pair<std::int32_t, std::int32_t> found = server::world::climate::spawnFinder::findSpawnPosition(sampler);
    return {found.first >> 4, found.second >> 4};
}

std::array<double, 3> LoadedWorld::spawnBlock(std::int32_t chunkX, std::int32_t chunkZ) {
    const std::optional<std::array<std::int32_t, 3>> found =
        worldSpawnPoint::find(chunkX, chunkZ, [this](std::int32_t x, std::int32_t z) -> const Chunk& {
            return state_.getOrGenerateChunk(x, z);
        });
    if (found.has_value()) {
        return {(*found)[0] + 0.5, static_cast<double>((*found)[1]), (*found)[2] + 0.5};
    }
    return {(chunkX * 16) + 8.5, static_cast<double>(kDefaultSpawnHeight), (chunkZ * 16) + 8.5};
}

void LoadedWorld::warmSpawnArea(std::int32_t spawnRadiusChunks) {
    // parity: `MinecraftServer.prepareLevels` prepares the overworld and nothing else, so no other
    // dimension has a chunk generated before whatever asks for one first. Its spawn is the recorded
    // one, which needs no chunk to answer.
    if (declaration_.dimension != config::WorldDimension::Overworld) {
        spawn_ = {metadata_.spawnX + 0.5, static_cast<double>(metadata_.spawnY), metadata_.spawnZ + 0.5};
        return;
    }
    const bool fixedSpawn = declaration_.type == config::WorldType::Void || movedSpawn_;
    const std::pair<std::int32_t, std::int32_t> centre = fixedSpawn ? std::pair{0, 0} : climateSpawnChunk();
    std::vector<std::pair<std::int32_t, std::int32_t>> positions;
    for (std::int32_t x = centre.first - spawnRadiusChunks; x <= centre.first + spawnRadiusChunks; x++) {
        for (std::int32_t z = centre.second - spawnRadiusChunks; z <= centre.second + spawnRadiusChunks; z++) {
            positions.emplace_back(x, z);
        }
    }
    static_cast<void>(generationQueue_.awaitAll(positions));
    if (fixedSpawn) {
        spawn_ = {metadata_.spawnX + 0.5, static_cast<double>(metadata_.spawnY), metadata_.spawnZ + 0.5};
        return;
    }
    spawn_ = spawnBlock(centre.first, centre.second);
    // parity: `setInitialSpawn` writes the climate search's answer into the level data, so a world picks
    // its spawn once and every later boot reads it back instead of searching the same terrain again.
    spawnOverride_ = std::array<std::int32_t, 3>{static_cast<std::int32_t>(std::floor(spawn_[0])),
                                                 static_cast<std::int32_t>(spawn_[1]),
                                                 static_cast<std::int32_t>(std::floor(spawn_[2]))};
}

void LoadedWorld::stashEntities(const std::vector<Chunk*>& chunks) {
    for (Chunk* chunk : chunks) {
        const std::vector<entity::ServerEntity*> entities = state_.entitiesIn(chunk->chunkX(), chunk->chunkZ());
        // parity: `ItemEntity` is saved with the chunk it stands in like anything else. What is
        // lying on the floor is half of what a farm is, and without this it went with the chunk.
        std::vector<wire::nbt::NbtCompound> dropped;
        for (const DroppedItem& item : state_.items()) {
            if ((static_cast<std::int32_t>(std::floor(item.x)) >> 4) == chunk->chunkX() &&
                (static_cast<std::int32_t>(std::floor(item.z)) >> 4) == chunk->chunkZ()) {
                dropped.push_back(droppedItemNbt::encode(item));
            }
        }
        if (entities.empty() && dropped.empty() && chunk->savedEntities().empty()) {
            continue;
        }
        std::vector<wire::nbt::NbtCompound> saved;
        for (entity::ServerEntity* entity : entities) {
            // parity: `EntityType.LIGHTNING_BOLT` is `noSave`.
            if (entity->removed || entity->vehicleId.has_value() || entity->type() == lightning::bolt()) {
                continue;
            }
            saved.push_back(entity::entityNbt::encodeTree(
                *entity, [this](std::int32_t entityId) { return state_.entity(entityId); }));
        }
        saved.insert(saved.end(), std::make_move_iterator(dropped.begin()), std::make_move_iterator(dropped.end()));
        chunk->setSavedEntities(std::move(saved));
    }
}

void LoadedWorld::save(const std::vector<Chunk*>& chunks) {
    if (chunks.empty()) {
        return;
    }
    stashEntities(chunks);
    const auto started = std::chrono::steady_clock::now();
    const std::size_t written = regions_.saveChunks(chunks, static_cast<std::int32_t>(epochSeconds()));
    const auto elapsed = std::chrono::steady_clock::now() - started;
    timings_.of("world.save").record(std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
    logger_.debug([&] {
        return "[" + declaration_.name + "] saved " + std::to_string(chunks.size()) + " chunk(s) across " +
               std::to_string(written) + " region(s) in " +
               std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()) + "ms (" +
               std::to_string(regions_.encodeMillis()) + "ms encoding, " + std::to_string(regions_.writeMillis()) +
               "ms writing, " + std::to_string(regions_.cachedRegionCount()) + " regions cached, " +
               std::to_string(state_.loadedChunkCount()) + " loaded).";
    });
}

void LoadedWorld::saveDirty() {
    // A mob walking across a chunk nothing else touched still has to be written down, so the chunks
    // holding entities join whatever the blocks made dirty.
    std::vector<Chunk*> chunks = state_.dirtyChunks();
    std::set<std::pair<std::int32_t, std::int32_t>> already;
    for (const Chunk* chunk : chunks) {
        already.emplace(chunk->chunkX(), chunk->chunkZ());
    }
    // A chunk that carried a mob last time has to be written again once it walks away, or the old
    // copy on disk comes back beside the one that moved.
    for (Chunk* chunk : state_.chunksWithStashedEntities()) {
        if (already.emplace(chunk->chunkX(), chunk->chunkZ()).second) {
            chunks.push_back(chunk);
        }
    }
    for (const std::pair<std::int32_t, std::int32_t>& position : state_.chunksWithEntities()) {
        if (!already.emplace(position).second) {
            continue;
        }
        if (Chunk* chunk = state_.chunkIfLoaded(position.first, position.second); chunk != nullptr) {
            chunks.push_back(chunk);
        }
    }
    save(chunks);
    portals_.save(worldRoot_);
    randomSequences_.save(worldRoot_);
    raids_.save(worldRoot_);
    state_.poi().save();
}

std::size_t LoadedWorld::unloadUnreachable(const std::vector<std::pair<std::int32_t, std::int32_t>>& playerPositions,
                                           std::int32_t spawnRadiusChunks, std::int32_t retainMarginChunks) {
    const std::int32_t retainRadius = settings_.viewDistance + retainMarginChunks;
    const std::unordered_set<std::int64_t> pending = generationQueue_.pendingKeys();
    const auto nearAPlayer = [&playerPositions, retainRadius](std::int32_t chunkX, std::int32_t chunkZ) {
        return std::ranges::any_of(playerPositions, [&](const std::pair<std::int32_t, std::int32_t>& position) {
            return std::abs(chunkX - position.first) <= retainRadius &&
                   std::abs(chunkZ - position.second) <= retainRadius;
        });
    };
    const std::vector<Chunk*> candidates = state_.evictionCandidates([&](std::int32_t chunkX, std::int32_t chunkZ) {
        if (std::abs(chunkX) <= spawnRadiusChunks && std::abs(chunkZ) <= spawnRadiusChunks) {
            return true;
        }
        const std::int64_t key = ChunkGenerationQueue::pack(chunkX, chunkZ);
        if (forcedChunks_.contains(key) || pending.contains(key)) {
            return true;
        }
        return nearAPlayer(chunkX, chunkZ);
    });
    static_cast<void>(generationQueue_.retainWhere(nearAPlayer));
    if (candidates.empty()) {
        return 0;
    }
    // Entities go to disk with the chunk they stand in, and then out of memory with it: leaving
    // them behind would spawn a second copy when the chunk came back.
    std::vector<Chunk*> withEntities;
    std::vector<Chunk*> toSave;
    for (Chunk* chunk : candidates) {
        const bool carries = !state_.entitiesIn(chunk->chunkX(), chunk->chunkZ()).empty();
        if (carries) {
            withEntities.push_back(chunk);
        }
        if (chunk->isDirty() || carries) {
            toSave.push_back(chunk);
        }
    }
    save(toSave);
    const std::size_t evicted = state_.evictClean(candidates);
    for (const Chunk* chunk : withEntities) {
        state_.forgetEntitiesIn(chunk->chunkX(), chunk->chunkZ());
    }
    return evicted;
}

void LoadedWorld::stopGenerating(std::int64_t drainTimeoutMillis) {
    generationQueue_.stop();
    generationQueue_.awaitIdle(drainTimeoutMillis);
    // Whatever the workers handed over on their way out still belongs in the world, so it is run
    // rather than dropped: a chunk installed here is one the save below writes out.
    static_cast<void>(owner().runPending([this](const std::exception& failure) {
        logger_.error(
            [&] { return "A task for world '" + declaration_.name + "' failed while stopping: " + failure.what(); });
    }));
}

void LoadedWorld::closeOwner() { owner().close(); }

void LoadedWorld::flushRegions() { static_cast<void>(regions_.flush()); }

} // namespace basalt::server::runtime::world
