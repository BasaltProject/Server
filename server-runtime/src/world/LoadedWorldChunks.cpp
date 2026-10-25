#include "LoadedWorldShared.hpp"

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
using namespace loadedWorld;

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
        // A proto chunk set aside on disk is still a chunk nobody has seen finished.
        if (made.has_value()) {
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
