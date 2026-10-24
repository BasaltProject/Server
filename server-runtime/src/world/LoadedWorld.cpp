#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/plugins/EventSink.hpp>
#include <basalt/server/runtime/sched/OnOwner.hpp>
#include <basalt/server/runtime/world/BiomeZoomSeed.hpp>
#include <basalt/server/runtime/world/DaylightDetectors.hpp>
#include <basalt/server/runtime/world/DroppedItemNbt.hpp>
#include <basalt/server/runtime/world/Lightning.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/SavedDataFiles.hpp>
#include <basalt/server/runtime/world/WorldLight.hpp>
#include <basalt/server/runtime/world/WorldSpawnPoint.hpp>
#include <basalt/server/world/LeafDistances.hpp>
#include <basalt/server/world/NoiseWorldGenerator.hpp>
#include <basalt/server/world/SimpleWorldGenerators.hpp>
#include <basalt/server/world/climate/Sampler.hpp>
#include <basalt/server/world/climate/SpawnFinder.hpp>
#include <basalt/server/world/region/ChunkNbt.hpp>
#include <basalt/server/world/terrain/RealTerrainGenerator.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/io/Platform.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <future>
#include <iterator>
#include <memory>
#include <set>

namespace basalt::server::runtime::world {
namespace {

namespace terrain = server::world::terrain;

using server::world::Chunk;

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

random::RandomSource LoadedWorld::uniqueRandom() {
    static std::atomic<std::uint64_t> uniquifier{8682522807148012ULL};
    std::uint64_t seed = uniquifier.load();
    while (!uniquifier.compare_exchange_weak(seed, seed * 1181783497276652981ULL)) {
    }
    const auto nanos = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    return random::RandomSource::legacy(static_cast<std::int64_t>((seed * 1181783497276652981ULL) ^ nanos));
}

LoadedWorld::LoadedWorld(config::WorldDeclaration declaration, config::WorldSettings settings, WorldMetadata metadata,
                         std::string worldsRoot, wire::common::Logger logger, Options options)
    : declaration_(std::move(declaration)), settings_(std::move(settings)), metadata_(std::move(metadata)),
      logger_(std::move(logger)), timings_(options.timings == nullptr ? ownTimings_ : *options.timings),
      overworldRouter_(options.overworldRouter), surfaceEnvironment_(options.surfaceEnvironment),
      villageGenerator_(options.villageGenerator), campGenerator_(options.campGenerator),
      structureLocators_(std::move(options.structureLocators)), structurePlacers_(std::move(options.structurePlacers)),
      strongholdGenerator_(options.strongholdGenerator), spawnChunkFinder_(std::move(options.spawnChunkFinder)),
      spawnMobs_(std::move(options.spawnMobs)),
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
      hardcore_(metadata_.hardcore.value_or(settings_.hardcore)), defaultGamemode_(metadata_.defaultGamemode) {
    if (hardcore_) {
        difficulty_ = "hard";
    }
    if (options.pipeline.terrain) {
        terrainOf_ = options.pipeline.terrain;
        server::world::terrain::ProtoChunks::Hooks hooks = std::move(options.pipeline);
        hooks.load = [this](std::int32_t chunkX,
                            std::int32_t chunkZ) -> std::optional<server::world::terrain::ProtoChunks::Stored> {
            std::optional<Chunk> stored = regions_.loadChunk(chunkX, chunkZ);
            if (!stored.has_value()) {
                return std::nullopt;
            }
            restoreBelowZero(*stored);
            const server::world::ChunkStage stage = stored->stage();
            return server::world::terrain::ProtoChunks::Stored{.chunk = std::move(*stored), .stage = stage};
        };
        if (declaration_.dimension == config::WorldDimension::Overworld &&
            declaration_.type != config::WorldType::Flat && declaration_.type != config::WorldType::Debug) {
            oldTerrain_ = std::make_unique<OldTerrainBlending>(regions_);
            hooks.blender = [this](std::int32_t chunkX, std::int32_t chunkZ) {
                return oldTerrain_->blenderFor(chunkX, chunkZ);
            };
        }
        const auto baseTerrain = hooks.terrain;
        const auto baseAdopt = hooks.adopt;
        const auto baseDecorate = hooks.decorate;
        hooks.terrain = [this, baseTerrain, baseAdopt](std::int32_t chunkX, std::int32_t chunkZ,
                                                       const server::world::blending::Blender* blender) {
            ChunkFiller filler;
            FillerTraits traits;
            {
                const std::lock_guard<std::mutex> lock{fillerMutex_};
                filler = chunkFiller_;
                traits = fillerTraits_;
            }
            if (!filler) {
                return baseTerrain(chunkX, chunkZ, blender);
            }
            const config::DimensionFacts& facts = config::dimensionFacts(declaration_.dimension);
            if (traits.vanillaTerrain || traits.structures) {
                const server::world::terrain::realTerrainGenerator::TerrainStepsScope steps{
                    {.noise = traits.noise, .surface = traits.surface, .caves = traits.caves}};
                server::world::terrain::realTerrainGenerator::GeneratedTerrain terrain =
                    baseTerrain(chunkX, chunkZ, blender);
                if (!traits.vanillaTerrain) {
                    for (std::size_t index = 0; index < terrain.chunk().sectionCount(); ++index) {
                        terrain.chunk().section(index).setBlockStateIds(
                            std::vector<std::int32_t>(server::world::ChunkSection::kBlocks, 0));
                    }
                }
                filler(chunkX, chunkZ, terrain.chunk());
                return terrain;
            }
            Chunk made = server::world::voidWorldGenerator::generate(chunkX, chunkZ, metadata_.spawnX, metadata_.spawnY,
                                                                     metadata_.spawnZ, 0, facts.chunkHeight);
            filler(chunkX, chunkZ, made);
            return baseAdopt(std::move(made));
        };
        hooks.decorate = [this, baseDecorate](server::world::terrain::realTerrainGenerator::GeneratedTerrain& terrain,
                                              server::world::terrain::ProtoRegion& region) {
            bool skip = false;
            {
                const std::lock_guard<std::mutex> lock{fillerMutex_};
                skip = chunkFiller_ && !fillerTraits_.decorations && !fillerTraits_.structures;
            }
            if (!skip) {
                baseDecorate(terrain, region);
            }
        };
        hooks.finish = [this](Chunk& chunk, bool generated) {
            bool mobs = true;
            {
                const std::lock_guard<std::mutex> lock{fillerMutex_};
                mobs = !chunkFiller_ || fillerTraits_.mobs;
            }
            if (generated && spawnMobs_ && mobs) {
                spawnMobs_(chunk, gameTime_, difficulty_);
            }
            portals_.scan(chunk);
            if (generated) {
                server::world::leafDistances::assign(chunk);
            }
        };
        hooks.persist = [this](std::vector<Chunk> chunks) {
            std::vector<Chunk*> held;
            held.reserve(chunks.size());
            for (Chunk& chunk : chunks) {
                held.push_back(&chunk);
            }
            static_cast<void>(regions_.saveChunks(held, static_cast<std::int32_t>(epochSeconds())));
        };
        protoChunks_ = std::make_unique<server::world::terrain::ProtoChunks>(std::move(hooks));
        generationQueue_.usePipeline(protoChunks_.get());
    }
    state_.setHasSkylight(config::dimensionFacts(declaration_.dimension).hasSkylight);
    state_.setOnNeighbourhoodComplete([this](std::int32_t chunkX, std::int32_t chunkZ, bool delayed) {
        const std::int64_t key = ChunkGenerationQueue::pack(chunkX, chunkZ);
        const std::lock_guard<std::mutex> lock{lightQueueMutex_};
        if (lightQueue_.size() >= kLightQueueLimit || lightPending_.contains(key)) {
            return;
        }
        const auto due = std::chrono::steady_clock::now() + (delayed ? kLightDebounce : std::chrono::milliseconds{0});
        lightPending_.emplace(key, due);
        lightQueue_.emplace(due, key);
        lightWake_.notify_one();
    });
    lightWorker_ = std::jthread{[this](const std::stop_token stop) {
        const bool skylight = config::dimensionFacts(declaration_.dimension).hasSkylight;
        const auto recompute = [this, skylight](const std::int64_t wanted, const bool requireAll) {
            std::array<Chunk*, 9> window{};
            const bool found = state_.lightWindowFor(ChunkGenerationQueue::unpackX(wanted),
                                                     ChunkGenerationQueue::unpackZ(wanted), window, requireAll);
            if (found) {
                if (!requireAll) {
                    window[4]->markLightStale();
                }
                worldLight::precompute(window, skylight);
            }
            state_.lightDone();
            return found;
        };
        const auto earliest = [this] {
            auto next = std::chrono::steady_clock::time_point::max();
            if (!lightQueue_.empty()) {
                next = lightQueue_.begin()->first;
            }
            for (const auto& [key, later] : lightLater_) {
                next = std::min(next, later.first);
            }
            return next;
        };
        while (!stop.stop_requested()) {
            std::int64_t key = 0;
            {
                std::unique_lock<std::mutex> lock{lightQueueMutex_};
                if (!lightWake_.wait(lock, stop, [this] {
                        return !lightQueue_.empty() || !lightJobs_.empty() || !lightLater_.empty();
                    })) {
                    return;
                }
                if (!lightJobs_.empty()) {
                    auto job = std::move(lightJobs_.front());
                    lightJobs_.pop_front();
                    lock.unlock();
                    std::int32_t relit = 0;
                    for (const std::int64_t wanted : job.first) {
                        relit += recompute(wanted, false) ? 1 : 0;
                    }
                    job.second(relit);
                    continue;
                }
                const auto next = earliest();
                if (next > std::chrono::steady_clock::now()) {
                    lightWake_.wait_until(lock, stop, next, [&] { return !lightJobs_.empty() || earliest() < next; });
                    continue;
                }
                if (const auto later = std::ranges::find_if(
                        lightLater_, [next](const auto& entry) { return entry.second.first == next; });
                    later != lightLater_.end()) {
                    const std::int64_t wanted = later->first;
                    auto then = std::move(later->second.second);
                    lightLater_.erase(later);
                    lock.unlock();
                    if (recompute(wanted, false) && then) {
                        then();
                    }
                    continue;
                }
                key = lightQueue_.begin()->second;
                lightQueue_.erase(lightQueue_.begin());
                lightPending_.erase(key);
            }
            static_cast<void>(recompute(key, true));
        }
    }};
    state_.setSettlesGeneration(declaration_.type != config::WorldType::Debug);
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
    if (metadata_.worldBorder.has_value()) {
        border_.restore(*metadata_.worldBorder);
    }
    weather_.raining = metadata_.raining;
    weather_.rainTime = metadata_.rainTime;
    weather_.thundering = metadata_.thundering;
    weather_.thunderTime = metadata_.thunderTime;
    weather_.clearWeatherTime = metadata_.clearWeatherTime;
    // parity: `TicketType.FORCED` persists.
    forcedChunks_.insert(metadata_.forcedChunks.begin(), metadata_.forcedChunks.end());
    for (const std::int64_t key : forcedChunks_) {
        generationQueue_.requestForced(ChunkGenerationQueue::unpackX(key), ChunkGenerationQueue::unpackZ(key));
    }
    portals_.load(worldRoot_);
    randomSequences_.load(worldRoot_);
    raids_.load(worldRoot_, config::dimensionFacts(declaration_.dimension).registryPath);
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
    // parity: `ServerLevel`'s constructor ends on `updateSkyBrightness()`.
    updateSkyBrightness();
}

void LoadedWorld::relightAsync(std::vector<std::int64_t> keys, std::function<void(std::int32_t)> done) {
    {
        const std::lock_guard<std::mutex> lock{lightQueueMutex_};
        lightJobs_.emplace_back(std::move(keys), std::move(done));
    }
    lightWake_.notify_one();
}

void LoadedWorld::relightSoon(std::int32_t chunkX, std::int32_t chunkZ, std::chrono::milliseconds quiet,
                              std::function<void()> then) {
    {
        const std::lock_guard<std::mutex> lock{lightQueueMutex_};
        lightLater_.insert_or_assign(ChunkGenerationQueue::pack(chunkX, chunkZ),
                                     std::pair{std::chrono::steady_clock::now() + quiet, std::move(then)});
    }
    lightWake_.notify_one();
}

LoadedWorld::~LoadedWorld() {
    lightWorker_.request_stop();
    lightWorker_.join();
}

void LoadedWorld::setAttributeOverride(const std::string& name, std::optional<double> value) {
    const std::lock_guard lock{attributeMutex_};
    if (value.has_value()) {
        attributeOverrides_.insert_or_assign(name, *value);
    } else {
        attributeOverrides_.erase(name);
    }
}

std::optional<double> LoadedWorld::attributeOverride(std::string_view name) const {
    const std::lock_guard lock{attributeMutex_};
    const auto found = attributeOverrides_.find(name);
    return found == attributeOverrides_.end() ? std::nullopt : std::optional{found->second};
}

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
    events().notify(&server::plugins::pluginEventTypes::worldSpawnChange, [&](server::plugins::PluginEvent& event) {
        event.set("world", name());
        event.set("x", static_cast<std::int64_t>(x));
        event.set("y", static_cast<std::int64_t>(y));
        event.set("z", static_cast<std::int64_t>(z));
        event.set("from_x", static_cast<std::int64_t>(std::floor(spawn_[0])));
        event.set("from_y", static_cast<std::int64_t>(std::floor(spawn_[1])));
        event.set("from_z", static_cast<std::int64_t>(std::floor(spawn_[2])));
    });
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
    // parity: `MinecraftServer.tickChildren` moves the day clock before any level ticks.
    const bool advanced =
        config::hasClock(declaration_.dimension) && clock_.tick(gameRules_.get(gameRules::kAdvanceTime));
    // parity: `ServerLevel.advanceWeatherCycle` runs only.
    if (config::canHaveWeather(declaration_.dimension)) {
        weather_.tick(random, gameRules_.get(gameRules::kAdvanceWeather));
    }
    // parity: `ServerLevel.tick` takes the reading after the weather and before `tickTime()`.
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
    saved.hardcore = hardcore_;
    saved.defaultGamemode = defaultGamemode_;
    saved.worldBorder = border_.encode();
    if (dragonFight_ != nullptr) {
        saved.dragonFight = dragonFight_->encode();
    }
    metadata_ = saved;
    static_cast<void>(saved.writeTo(worldRoot_));
}

Chunk LoadedWorld::generateChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    const auto started = std::chrono::steady_clock::now();
    ChunkFiller filler;
    {
        const std::lock_guard<std::mutex> lock{fillerMutex_};
        filler = chunkFiller_;
    }
    const config::DimensionFacts& facts = config::dimensionFacts(declaration_.dimension);
    Chunk chunk = [&] {
        if (filler && pipelineSetAside()) {
            return server::world::voidWorldGenerator::generate(chunkX, chunkZ, metadata_.spawnX, metadata_.spawnY,
                                                               metadata_.spawnZ, 0, facts.chunkHeight);
        }
        switch (declaration_.type) {
        case config::WorldType::Flat:
            return server::world::flatWorldGenerator::generate(chunkX, chunkZ, facts.chunkHeight);
        case config::WorldType::Void:
            return server::world::voidWorldGenerator::generate(
                chunkX, chunkZ, metadata_.spawnX, metadata_.spawnY, metadata_.spawnZ,
                filler ? 0 : server::world::voidWorldGenerator::kDefaultPlatformRadius, facts.chunkHeight);
        case config::WorldType::Debug:
            return server::world::debugWorldGenerator::generate(chunkX, chunkZ, facts.chunkHeight);
        case config::WorldType::Normal:
        case config::WorldType::Amplified:
        case config::WorldType::LargeBiomes:
        default:
            break;
        }
        return server::world::noiseWorldGenerator::generate(chunkX, chunkZ);
    }();
    if (filler && pipelineSetAside()) {
        filler(chunkX, chunkZ, chunk);
    }
    timings_.of("world.generate")
        .record(
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started).count());
    return chunk;
}

void LoadedWorld::restoreBelowZero(Chunk& chunk) {
    if (!chunk.needsRetrogen()) {
        return;
    }
    // parity: `below_zero_retrogen`, which makes the layers under zero and keeps what the old chunk had above them.
    const auto copyBelowZero = [&chunk](Chunk& made) {
        const std::size_t belowZero = static_cast<std::size_t>(-chunk.verticalBounds().minSection());
        for (std::size_t index = 0; index < belowZero && index < chunk.sectionCount(); ++index) {
            chunk.section(index).setBlockStateIds(made.section(index).blockStateIds());
            chunk.section(index).setBiomeIds(made.section(index).biomeIds());
        }
    };
    if (terrainOf_) {
        const std::shared_ptr<const server::world::blending::Blender> blender =
            oldTerrain_ != nullptr ? oldTerrain_->blenderFor(chunk.chunkX(), chunk.chunkZ()) : nullptr;
        server::world::terrain::realTerrainGenerator::GeneratedTerrain terrain =
            terrainOf_(chunk.chunkX(), chunk.chunkZ(), blender.get());
        copyBelowZero(terrain.chunk());
    } else {
        Chunk made = generateChunk(chunk.chunkX(), chunk.chunkZ());
        copyBelowZero(made);
    }
    chunk.setNeedsRetrogen(false);
    chunk.invalidateHeightmaps();
}

std::optional<Chunk> LoadedWorld::loadOrGenerateChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    std::optional<Chunk> stored = regions_.loadChunk(chunkX, chunkZ);
    if (stored.has_value()) {
        restoreBelowZero(*stored);
    }
    if (protoChunks_ != nullptr && !pipelineSetAside() &&
        (!stored.has_value() || stored->stage() != server::world::ChunkStage::Full)) {
        // A proto chunk on disk is the pipeline's to finish, and it reads it back itself.
        const auto started = std::chrono::steady_clock::now();
        std::optional<Chunk> made = protoChunks_->generateNow(chunkX, chunkZ);
        timings_.of("world.generate")
            .record(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started)
                        .count());
        if (made.has_value() && !stored.has_value()) {
            made->markFresh();
        }
        return made;
    }
    Chunk chunk = stored.has_value() ? std::move(*stored) : generateChunk(chunkX, chunkZ);
    chunk.setStage(server::world::ChunkStage::Full);
    if (!stored.has_value()) {
        chunk.markFresh();
    }
    portals_.scan(chunk);
    // Off the tick thread, and once: from here on a leaf knows its own distance. parity: not in a debug
    if (!stored.has_value() && declaration_.type != config::WorldType::Debug) {
        server::world::leafDistances::assign(chunk);
    }
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

void LoadedWorld::warmSpawnArea() {
    // parity: `MinecraftServer.prepareLevels` prepares the overworld and nothing else.
    if (declaration_.dimension != config::WorldDimension::Overworld) {
        spawn_ = {metadata_.spawnX + 0.5, static_cast<double>(metadata_.spawnY), metadata_.spawnZ + 0.5};
        return;
    }
    const bool fixedSpawn = declaration_.type == config::WorldType::Void || movedSpawn_;
    // parity: vanilla keeps no spawn chunks. The only chunks it makes full before anyone joins are.
    const std::pair<std::int32_t, std::int32_t> centre = fixedSpawn ? std::pair{0, 0} : climateSpawnChunk();
    if (fixedSpawn) {
        spawn_ = {metadata_.spawnX + 0.5, static_cast<double>(metadata_.spawnY), metadata_.spawnZ + 0.5};
        return;
    }
    spawn_ = spawnBlock(centre.first, centre.second);
    // parity: `setInitialSpawn` writes the climate search's answer into the level data.
    spawnOverride_ = std::array<std::int32_t, 3>{static_cast<std::int32_t>(std::floor(spawn_[0])),
                                                 static_cast<std::int32_t>(spawn_[1]),
                                                 static_cast<std::int32_t>(std::floor(spawn_[2]))};
}

const plugins::EventSink& WorldState::events() const noexcept {
    static const plugins::EventSink kSilent{[]() -> server::plugins::PluginEventBus* { return nullptr; }};
    return events_ != nullptr ? *events_ : kSilent;
}

const plugins::EventSink& LoadedWorld::events() const noexcept {
    static const plugins::EventSink kSilent{[]() -> server::plugins::PluginEventBus* { return nullptr; }};
    return events_ != nullptr ? *events_ : kSilent;
}

void LoadedWorld::stashEntities(const std::vector<Chunk*>& chunks) {
    for (Chunk* chunk : chunks) {
        const std::vector<entity::ServerEntity*> entities = state_.entitiesIn(chunk->chunkX(), chunk->chunkZ());
        // parity: `ItemEntity` is saved with the chunk it stands in like anything else. What is.
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

std::vector<RegionStore::Snapshot> LoadedWorld::snapshotOf(const std::vector<Chunk*>& chunks) {
    stashEntities(chunks);
    std::vector<RegionStore::Snapshot> snapshots;
    snapshots.reserve(chunks.size());
    for (Chunk* chunk : chunks) {
        snapshots.push_back(
            RegionStore::Snapshot{chunk->chunkX(), chunk->chunkZ(), server::world::region::chunkNbt::write(*chunk)});
        chunk->markClean();
    }
    return snapshots;
}

void LoadedWorld::save(const std::vector<Chunk*>& chunks) {
    if (chunks.empty()) {
        return;
    }
    std::vector<std::pair<std::int32_t, std::int32_t>> positions;
    positions.reserve(chunks.size());
    for (const Chunk* chunk : chunks) {
        positions.emplace_back(chunk->chunkX(), chunk->chunkZ());
    }
    writeSnapshots(snapshotInBatches(positions));
}

std::vector<RegionStore::Snapshot>
LoadedWorld::snapshotInBatches(const std::vector<std::pair<std::int32_t, std::int32_t>>& positions) {
    std::vector<RegionStore::Snapshot> snapshots;
    snapshots.reserve(positions.size());
    for (std::size_t first = 0; first < positions.size(); first += kSnapshotBatch) {
        const std::size_t last = std::min(positions.size(), first + kSnapshotBatch);
        std::vector<RegionStore::Snapshot> batch =
            sched::onOwner<std::vector<RegionStore::Snapshot>>(owner(), [this, &positions, first, last] {
                std::vector<Chunk*> chunks;
                for (std::size_t at = first; at < last; ++at) {
                    if (Chunk* chunk = state_.chunkIfLoaded(positions[at].first, positions[at].second);
                        chunk != nullptr) {
                        chunks.push_back(chunk);
                    }
                }
                return snapshotOf(chunks);
            });
        snapshots.insert(snapshots.end(), std::make_move_iterator(batch.begin()), std::make_move_iterator(batch.end()));
    }
    return snapshots;
}

void LoadedWorld::writeSnapshots(std::vector<RegionStore::Snapshot> snapshots) {
    if (snapshots.empty()) {
        return;
    }
    const std::size_t count = snapshots.size();
    const auto started = std::chrono::steady_clock::now();
    const std::size_t written = regions_.saveSnapshots(std::move(snapshots), static_cast<std::int32_t>(epochSeconds()));
    const auto elapsed = std::chrono::steady_clock::now() - started;
    timings_.of("world.save").record(std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
    logger_.debug([&] {
        return "[" + declaration_.name + "] saved " + std::to_string(count) + " chunk(s) across " +
               std::to_string(written) + " region(s) in " +
               std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()) + "ms (" +
               std::to_string(regions_.encodeMillis()) + "ms encoding, " + std::to_string(regions_.writeMillis()) +
               "ms writing, " + std::to_string(regions_.cachedRegionCount()) + " regions cached, " +
               std::to_string(state_.loadedChunkCount()) + " loaded).";
    });
}

void LoadedWorld::saveDirty() {
    const std::vector<std::pair<std::int32_t, std::int32_t>> positions =
        sched::onOwner<std::vector<std::pair<std::int32_t, std::int32_t>>>(owner(), [this] {
            std::vector<Chunk*> chunks = state_.dirtyChunks();
            std::set<std::pair<std::int32_t, std::int32_t>> already;
            for (const Chunk* chunk : chunks) {
                already.emplace(chunk->chunkX(), chunk->chunkZ());
            }
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
            std::vector<std::pair<std::int32_t, std::int32_t>> found;
            found.reserve(chunks.size());
            for (const Chunk* chunk : chunks) {
                found.emplace_back(chunk->chunkX(), chunk->chunkZ());
            }
            return found;
        });
    writeSnapshots(snapshotInBatches(positions));
    portals_.save(worldRoot_);
    randomSequences_.save(worldRoot_);
    raids_.save(worldRoot_);
    state_.poi().save();
    const std::lock_guard<std::mutex> lock{pluginDataMutex_};
    if (pluginDataDirty_ && pluginData_.has_value()) {
        pluginDataDirty_ = !savedData::write(worldRoot_, "plugin_data", *pluginData_);
    }
}

wire::nbt::NbtCompound LoadedWorld::pluginData() {
    const std::lock_guard<std::mutex> lock{pluginDataMutex_};
    if (!pluginData_.has_value()) {
        pluginData_ = savedData::read(worldRoot_, "plugin_data").value_or(wire::nbt::NbtCompound{});
    }
    return *pluginData_;
}

void LoadedWorld::setPluginData(wire::nbt::NbtCompound data) {
    const std::lock_guard<std::mutex> lock{pluginDataMutex_};
    pluginData_ = std::move(data);
    pluginDataDirty_ = true;
}

void LoadedWorld::holdChunks(const std::string& token, const std::vector<std::int64_t>& keys) {
    const std::lock_guard<std::mutex> lock{holdsMutex_};
    holds_[token].insert(keys.begin(), keys.end());
}

void LoadedWorld::releaseChunks(const std::string& token) {
    const std::lock_guard<std::mutex> lock{holdsMutex_};
    holds_.erase(token);
}

bool LoadedWorld::isHeld(std::int32_t chunkX, std::int32_t chunkZ) const {
    const std::lock_guard<std::mutex> lock{holdsMutex_};
    const std::int64_t key = ChunkGenerationQueue::pack(chunkX, chunkZ);
    return std::ranges::any_of(holds_, [key](const auto& entry) { return entry.second.contains(key); });
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
    // parity: a forced ticket keeps its two full rings loaded too.
    const auto nearForced = [this](std::int32_t chunkX, std::int32_t chunkZ) {
        constexpr std::int32_t kRings = ChunkGenerationQueue::kForcedFullRings;
        for (std::int32_t dx = -kRings; dx <= kRings; ++dx) {
            for (std::int32_t dz = -kRings; dz <= kRings; ++dz) {
                if (forcedChunks_.contains(ChunkGenerationQueue::pack(chunkX + dx, chunkZ + dz))) {
                    return true;
                }
            }
        }
        return false;
    };
    const std::vector<Chunk*> candidates = state_.evictionCandidates([&](std::int32_t chunkX, std::int32_t chunkZ) {
        if (std::abs(chunkX) <= spawnRadiusChunks && std::abs(chunkZ) <= spawnRadiusChunks) {
            return true;
        }
        const std::int64_t key = ChunkGenerationQueue::pack(chunkX, chunkZ);
        if (pending.contains(key) || nearForced(chunkX, chunkZ) || isHeld(chunkX, chunkZ)) {
            return true;
        }
        return nearAPlayer(chunkX, chunkZ);
    });
    static_cast<void>(generationQueue_.retainWhere([&](std::int32_t chunkX, std::int32_t chunkZ) {
        return nearAPlayer(chunkX, chunkZ) || isHeld(chunkX, chunkZ);
    }));
    if (candidates.empty()) {
        return 0;
    }
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
    return onOwner<std::size_t>(owner(), [this, &candidates, &withEntities] {
        const std::size_t evicted = state_.evictClean(candidates);
        for (const Chunk* chunk : withEntities) {
            state_.forgetEntitiesIn(chunk->chunkX(), chunk->chunkZ());
        }
        return evicted;
    });
}

void LoadedWorld::stopGenerating(std::int64_t drainTimeoutMillis) {
    generationQueue_.stop();
    generationQueue_.awaitIdle(drainTimeoutMillis);
    // A proto chunk a neighbour already wrote into cannot be made again from the seed alone.
    if (protoChunks_ != nullptr) {
        protoChunks_->persistAll();
    }
    static_cast<void>(owner().runPending([this](const std::exception& failure) {
        logger_.error(
            [&] { return "A task for world '" + declaration_.name + "' failed while stopping: " + failure.what(); });
    }));
}

void LoadedWorld::closeOwner() { owner().close(); }

void LoadedWorld::flushRegions() { static_cast<void>(regions_.flush()); }

} // namespace basalt::server::runtime::world
