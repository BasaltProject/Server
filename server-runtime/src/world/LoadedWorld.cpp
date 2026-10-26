#include "LoadedWorldShared.hpp"

#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/runtime/config/GameplayRules.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/plugins/EventSink.hpp>
#include <basalt/server/runtime/sched/OnOwner.hpp>
#include <basalt/server/runtime/world/Beds.hpp>
#include <basalt/server/runtime/world/BiomeZoomSeed.hpp>
#include <basalt/server/runtime/world/DaylightDetectors.hpp>
#include <basalt/server/runtime/world/DroppedItemNbt.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
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
using namespace loadedWorld;

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
    tunables_.lootRefill = settings_.lootRefill;
    tunables_.lootRelootSeconds = settings_.lootRelootSeconds;
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
    state_.setGameplay(settings_.gameplay);
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
    // parity: Purpur's day and night lengths, a tick of either phase being a twelve-thousandth of the cycle.
    const config::GameplayRules::Mechanics& cycle = state_.gameplay().mechanics;
    const float phaseTicks = static_cast<float>(beds::isNight(clock_.totalTicks()) ? cycle.daylightNighttimeTicks
                                                                                   : cycle.daylightDaytimeTicks);
    const bool advanced = config::hasClock(declaration_.dimension) &&
                          clock_.tick(gameRules_.get(gameRules::kAdvanceTime), 12000.0F / phaseTicks);
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

} // namespace basalt::server::runtime::world
