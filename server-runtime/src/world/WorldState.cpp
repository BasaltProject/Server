#include "WorldStateShared.hpp"

#include <basalt/server/registry/BlockConnections.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/config/GameplayRules.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/plugins/HookEvents.hpp>
#include <basalt/server/runtime/world/BlockEntityDefaults.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/EndCrystals.hpp>
#include <basalt/server/runtime/world/FallingBlocks.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/Furnaces.hpp>
#include <basalt/server/runtime/world/NaturalSpawnObstruction.hpp>
#include <basalt/server/runtime/world/Shulkers.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/server/world/NoiseWorldGenerator.hpp>
#include <basalt/server/world/terrain/decoration/BushSurvival.hpp>
#include <basalt/server/world/terrain/decoration/tree/LeafDistances.hpp>
#include <basalt/wire/io/Platform.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <map>
#include <span>
#include <utility>

namespace basalt::server::runtime::world {
using server::world::Chunk;
using server::world::ChunkScheduledTick;

const config::GameplayRules& WorldState::gameplay() const noexcept {
    static const config::GameplayRules kVanilla;
    return gameplay_ != nullptr ? *gameplay_ : kVanilla;
}

WorldState::WorldState(std::vector<Chunk> initialChunks, LoadOrGenerate loadOrGenerate)
    : loadOrGenerate_(
          loadOrGenerate
              ? std::move(loadOrGenerate)
              : [](std::int32_t chunkX,
                   std::int32_t chunkZ) { return server::world::noiseWorldGenerator::generate(chunkX, chunkZ); }),
      primedTntQueue_(mutex_), pendingDecorations_(mutex_), sculkSensors_(mutex_), entityRegistry_(mutex_),
      itemPhysics_(mutex_, lockedBlockAccessor()) {
    for (Chunk& chunk : initialChunks) {
        const std::int64_t chunkKey = key(chunk.chunkX(), chunk.chunkZ());
        chunksByPos_.emplace(chunkKey, std::make_unique<Chunk>(std::move(chunk)));
    }
    const PendingChunkDecorations::ChunkAt chunkAt = [this](std::int64_t chunkKey) { return chunkAtLocked(chunkKey); };
    for (const auto& [chunkKey, chunk] : chunksByPos_) {
        pendingDecorations_.addPendingGeneratedEntities(chunk->takeGeneratedEntitiesMatching(
            [](const server::world::ChunkGeneratedEntity& entity) { return becomesLive(entity); }));
        PendingChunkDecorations::deliverGeneratedBlockWrites(chunk->takeGeneratedBlockWrites(), chunkAt);
        PendingChunkDecorations::deliverGeneratedBlockEntityWrites(chunk->takeGeneratedBlockEntityWrites(), chunkAt);
        PendingChunkDecorations::deliverGeneratedBlockTicks(chunk->takeGeneratedBlockTicks(), chunkAt);
    }
}

std::int64_t WorldState::key(std::int32_t chunkX, std::int32_t chunkZ) {
    return (static_cast<std::int64_t>(chunkX) & 0xFFFFFFFFLL) | (static_cast<std::int64_t>(chunkZ) << 32);
}

void WorldState::mine(std::string_view what) const {
    if (regionCount_ && regionCount_() > 1) {
        return;
    }
    if (owner_ != nullptr) {
        owner_->check(what);
    }
}

Chunk* WorldState::chunkAtLocked(std::int64_t chunkKey) const {
    const auto found = chunksByPos_.find(chunkKey);
    return found == chunksByPos_.end() ? nullptr : found->second.get();
}

std::vector<Chunk*> WorldState::snapshot() const {
    const WorldReadLock lock{mutex_};
    std::vector<Chunk*> every;
    every.reserve(chunksByPos_.size());
    for (const auto& [chunkKey, chunk] : chunksByPos_) {
        every.push_back(chunk.get());
    }
    return every;
}

std::vector<Chunk*> WorldState::chunks() const { return snapshot(); }

std::size_t WorldState::loadedChunkCount() const {
    const WorldReadLock lock{mutex_};
    return chunksByPos_.size();
}

WorldState::ChunkMemory WorldState::chunkMemory() const {
    const WorldReadLock lock{mutex_};
    ChunkMemory memory;
    for (const auto& [chunkKey, chunk] : chunksByPos_) {
        memory.bytes += chunk->residentBytes();
        memory.blockEntities += static_cast<std::int32_t>(chunk->blockEntityCount());
        memory.scheduledTicks += static_cast<std::int32_t>(chunk->scheduledTickCount());
        memory.generatedEntities += static_cast<std::int32_t>(chunk->generatedEntities().size());
    }
    return memory;
}

std::vector<Chunk*> WorldState::dirtyChunks() const {
    std::vector<Chunk*> dirty;
    for (Chunk* chunk : snapshot()) {
        if (chunk->isDirty()) {
            dirty.push_back(chunk);
        }
    }
    return dirty;
}

std::vector<Chunk*> WorldState::chunksWithStashedEntities() const {
    std::vector<Chunk*> stashed;
    for (Chunk* chunk : snapshot()) {
        if (!chunk->savedEntities().empty()) {
            stashed.push_back(chunk);
        }
    }
    return stashed;
}

std::vector<Chunk*>
WorldState::evictionCandidates(const std::function<bool(std::int32_t, std::int32_t)>& shouldKeep) const {
    std::vector<Chunk*> candidates;
    for (Chunk* chunk : snapshot()) {
        if (!shouldKeep(chunk->chunkX(), chunk->chunkZ())) {
            candidates.push_back(chunk);
        }
    }
    return candidates;
}

std::size_t WorldState::countMissing(const std::vector<std::pair<std::int32_t, std::int32_t>>& positions) const {
    const WorldReadLock lock{mutex_};
    std::size_t missing = 0;
    for (const auto& [chunkX, chunkZ] : positions) {
        if (!chunksByPos_.contains(key(chunkX, chunkZ))) {
            ++missing;
        }
    }
    return missing;
}

bool WorldState::lightWindowFor(std::int32_t chunkX, std::int32_t chunkZ, std::array<server::world::Chunk*, 9>& window,
                                const bool requireAll) {
    lightBusy_.fetch_add(1);
    const WorldReadLock lock{mutex_};
    for (std::int32_t offsetZ = -1; offsetZ <= 1; ++offsetZ) {
        for (std::int32_t offsetX = -1; offsetX <= 1; ++offsetX) {
            Chunk* found = chunkAtLocked(key(chunkX + offsetX, chunkZ + offsetZ));
            if (found == nullptr && (requireAll || (offsetX == 0 && offsetZ == 0))) {
                return false;
            }
            window[static_cast<std::size_t>((offsetZ + 1) * 3 + offsetX + 1)] = found;
        }
    }
    return true;
}

void WorldState::requestLight(std::int32_t chunkX, std::int32_t chunkZ) {
    if (!onNeighbourhoodComplete_) {
        return;
    }
    {
        const WorldReadLock lock{mutex_};
        for (std::int32_t offsetZ = -1; offsetZ <= 1; ++offsetZ) {
            for (std::int32_t offsetX = -1; offsetX <= 1; ++offsetX) {
                if (chunkAtLocked(key(chunkX + offsetX, chunkZ + offsetZ)) == nullptr) {
                    return;
                }
            }
        }
    }
    onNeighbourhoodComplete_(chunkX, chunkZ, false);
}

void WorldState::markLightAround(const std::int32_t chunkX, const std::int32_t chunkZ) {
    const WorldReadLock lock{mutex_};
    for (std::int32_t offsetX = -1; offsetX <= 1; ++offsetX) {
        for (std::int32_t offsetZ = -1; offsetZ <= 1; ++offsetZ) {
            if (Chunk* neighbour = chunkAtLocked(key(chunkX + offsetX, chunkZ + offsetZ)); neighbour != nullptr) {
                neighbour->markLightStale();
                if (onNeighbourhoodComplete_) {
                    onNeighbourhoodComplete_(neighbour->chunkX(), neighbour->chunkZ(), true);
                }
            }
        }
    }
}

void WorldState::lightDone() {
    if (lightBusy_.fetch_sub(1) > 1) {
        return;
    }
    const std::lock_guard<std::mutex> graveyard{graveyardMutex_};
    graveyard_.clear();
}

std::size_t WorldState::evictClean(const std::vector<Chunk*>& chunks) {
    const WorldWriteLock lock{mutex_};
    std::size_t removed = 0;
    for (Chunk* chunk : chunks) {
        if (chunk->isDirty()) {
            continue;
        }
        const std::int64_t chunkKey = key(chunk->chunkX(), chunk->chunkZ());
        const std::int32_t chunkX = chunk->chunkX();
        const std::int32_t chunkZ = chunk->chunkZ();
        if (auto node = chunksByPos_.extract(chunkKey); !node.empty()) {
            if (lightBusy_.load() > 0) {
                const std::lock_guard<std::mutex> graveyard{graveyardMutex_};
                graveyard_.push_back(std::move(node.mapped()));
            }
            pendingDecorations_.forgetGeneratedEntities(chunkKey);
            sculkSensors_.forgetChunk(chunkX, chunkZ);
            blockVersion_.fetch_add(1, std::memory_order_release);
            ++removed;
        }
    }
    if (removed > 0) {
        chunkVersion_.fetch_add(1);
    }
    return removed;
}

std::vector<std::int64_t> WorldState::loadedChunkKeys() const {
    const WorldReadLock lock{mutex_};
    std::vector<std::int64_t> keys;
    keys.reserve(chunksByPos_.size());
    for (const auto& [chunkKey, chunk] : chunksByPos_) {
        keys.push_back(chunkKey);
    }
    return keys;
}

bool WorldState::isLoaded(std::int32_t chunkX, std::int32_t chunkZ) const {
    const WorldReadLock lock{mutex_};
    return chunksByPos_.contains(key(chunkX, chunkZ));
}

Chunk* WorldState::chunkIfLoaded(std::int32_t chunkX, std::int32_t chunkZ) const {
    const WorldReadLock lock{mutex_};
    return chunkAtLocked(key(chunkX, chunkZ));
}

std::optional<Chunk> WorldState::generateDetached(std::int32_t chunkX, std::int32_t chunkZ) {
    if (isLoaded(chunkX, chunkZ)) {
        return std::nullopt;
    }
    return loadOrGenerate_(chunkX, chunkZ);
}

Chunk& WorldState::getOrGenerateChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    if (Chunk* loaded = chunkIfLoaded(chunkX, chunkZ); loaded != nullptr) {
        return *loaded;
    }
    std::optional<Chunk> generated = generateDetached(chunkX, chunkZ);
    if (generated.has_value()) {
        Chunk& placed = install(std::move(*generated));
        if (const std::function<void(Chunk&)> handler = chunkInstalledHandler()) {
            handler(placed);
        }
        return placed;
    }
    Chunk* arrived = chunkIfLoaded(chunkX, chunkZ);
    while (arrived == nullptr) {
        sched::Owner* target = installOwner_ != nullptr ? installOwner_ : owner_;
        if (target != nullptr && target->isCurrent()) {
            static_cast<void>(target->runPending());
        }
        wire::io::sleepFor(std::chrono::milliseconds{1});
        arrived = chunkIfLoaded(chunkX, chunkZ);
    }
    return *arrived;
}

void WorldState::queueBlockEvent(BlockEvent event) {
    const WorldWriteLock lock{mutex_};
    // parity: `ServerLevel.blockEvents` is a set.
    const bool already = std::ranges::any_of(blockEvents_, [&event](const BlockEvent& queued) {
        return queued.x == event.x && queued.y == event.y && queued.z == event.z && queued.block == event.block &&
               queued.first == event.first && queued.second == event.second;
    });
    if (already) {
        return;
    }
    blockEvents_.push_back(std::move(event));
}

void WorldState::recordGameEvent(GameEventRecord record) {
    const WorldWriteLock lock{mutex_};
    const auto firstKept = std::ranges::find_if(
        gameEvents_, [&record](const GameEventRecord& kept) { return kept.gameTime >= record.gameTime - 1; });
    gameEvents_.erase(gameEvents_.begin(), firstKept);
    gameEvents_.push_back(std::move(record));
}

std::vector<GameEventRecord> WorldState::recentGameEvents() const {
    const WorldReadLock lock{mutex_};
    return gameEvents_;
}

bool WorldState::vibrationParticlePending(std::int64_t packedPos) const {
    const WorldReadLock lock{mutex_};
    return pendingVibrationParticles_.contains(packedPos);
}

void WorldState::setVibrationParticlePending(std::int64_t packedPos, bool pending) {
    const WorldWriteLock lock{mutex_};
    if (pending) {
        pendingVibrationParticles_.insert(packedPos);
    } else {
        pendingVibrationParticles_.erase(packedPos);
    }
}

std::optional<BlockEvent> WorldState::takeFirstBlockEvent() {
    const WorldWriteLock lock{mutex_};
    if (blockEvents_.empty()) {
        return std::nullopt;
    }
    BlockEvent first = std::move(blockEvents_.front());
    blockEvents_.erase(blockEvents_.begin());
    return first;
}

std::vector<BlockEvent> WorldState::takeBlockEvents() {
    const WorldWriteLock lock{mutex_};
    return std::exchange(blockEvents_, {});
}

void WorldState::addTorchToggle(TorchToggle toggle) {
    const WorldWriteLock lock{mutex_};
    torchToggles_.push_back(toggle);
}

void WorldState::pruneTorchToggles(const std::int64_t now, const std::int64_t keepFor) {
    const WorldWriteLock lock{mutex_};
    while (!torchToggles_.empty() && now - torchToggles_.front().when > keepFor) {
        torchToggles_.pop_front();
    }
}

std::int32_t WorldState::torchToggleCount(const std::int32_t x, const std::int32_t y, const std::int32_t z) const {
    const WorldReadLock lock{mutex_};
    return static_cast<std::int32_t>(std::ranges::count_if(
        torchToggles_, [x, y, z](const TorchToggle& one) { return one.x == x && one.y == y && one.z == z; }));
}

} // namespace basalt::server::runtime::world
