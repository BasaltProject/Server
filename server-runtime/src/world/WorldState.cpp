#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/world/NaturalSpawnObstruction.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/server/world/NoiseWorldGenerator.hpp>

#include <utility>

namespace basalt::server::runtime::world {

using server::world::Chunk;
using server::world::ChunkScheduledTick;

WorldState::WorldState(std::vector<Chunk> initialChunks, LoadOrGenerate loadOrGenerate)
    : loadOrGenerate_(
          loadOrGenerate
              ? std::move(loadOrGenerate)
              : [](std::int32_t chunkX,
                   std::int32_t chunkZ) { return server::world::noiseWorldGenerator::generate(chunkX, chunkZ); }),
      primedTntQueue_(mutex_), pendingDecorations_(mutex_), sculkSensors_(mutex_), entityRegistry_(mutex_),
      itemPhysics_(mutex_, lockedBlockAccessor()) {
    // Tests and embedders may seed a world with detached generated chunks. Production starts empty,
    // but those chunks still deserve the same cross-border handoff an install gives.
    for (Chunk& chunk : initialChunks) {
        const std::int64_t chunkKey = key(chunk.chunkX(), chunk.chunkZ());
        chunksByPos_.emplace(chunkKey, std::make_unique<Chunk>(std::move(chunk)));
    }
    const PendingChunkDecorations::ChunkAt chunkAt = [this](std::int64_t chunkKey) { return chunkAtLocked(chunkKey); };
    for (const auto& [chunkKey, chunk] : chunksByPos_) {
        pendingDecorations_.addPendingGeneratedEntities(chunk->takeGeneratedEntitiesMatching(
            [](const server::world::ChunkGeneratedEntity& entity) { return entity::mobs::isMob(entity.type); }));
        pendingDecorations_.applyPendingGeneratedBlockWrites(*chunk);
        pendingDecorations_.applyPendingGeneratedBlockEntityWrites(*chunk);
        pendingDecorations_.applyPendingGeneratedBlockTicks(*chunk);
        pendingDecorations_.deliverGeneratedBlockWrites(chunk->takeGeneratedBlockWrites(), chunkAt);
        pendingDecorations_.deliverGeneratedBlockEntityWrites(chunk->takeGeneratedBlockEntityWrites(), chunkAt);
        pendingDecorations_.deliverGeneratedBlockTicks(chunk->takeGeneratedBlockTicks(), chunkAt);
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
    // Snapshotted under the lock and filtered outside it: every generation worker needs this same
    // lock twice per chunk, and holding it across a scan of everything loaded is how the thread
    // doing the scanning ends up never being scheduled at all.
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

std::size_t WorldState::evictClean(const std::vector<Chunk*>& chunks) {
    const WorldWriteLock lock{mutex_};
    std::size_t removed = 0;
    for (Chunk* chunk : chunks) {
        if (chunk->isDirty()) {
            continue;
        }
        const std::int64_t chunkKey = key(chunk->chunkX(), chunk->chunkZ());
        if (chunksByPos_.erase(chunkKey) != 0) {
            pendingDecorations_.forgetGeneratedEntities(chunkKey);
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
    if (!generated.has_value()) {
        // It turned up while this was generating, so the one that arrived is the world's.
        return *chunkIfLoaded(chunkX, chunkZ);
    }
    return install(std::move(*generated));
}

Chunk& WorldState::installLocked(Chunk chunk) {
    const std::int32_t chunkX = chunk.chunkX();
    const std::int32_t chunkZ = chunk.chunkZ();
    const std::int64_t chunkKey = key(chunkX, chunkZ);
    if (Chunk* existing = chunkAtLocked(chunkKey); existing != nullptr) {
        return *existing;
    }

    Chunk& installed = *chunksByPos_.emplace(chunkKey, std::make_unique<Chunk>(std::move(chunk))).first->second;
    const PendingChunkDecorations::ChunkAt chunkAt = [this](std::int64_t otherKey) { return chunkAtLocked(otherKey); };
    pendingDecorations_.applyPendingGeneratedBlockWrites(installed);
    pendingDecorations_.applyPendingGeneratedBlockEntityWrites(installed);
    pendingDecorations_.applyPendingGeneratedBlockTicks(installed);
    pendingDecorations_.deliverGeneratedBlockWrites(installed.takeGeneratedBlockWrites(), chunkAt);
    pendingDecorations_.deliverGeneratedBlockEntityWrites(installed.takeGeneratedBlockEntityWrites(), chunkAt);
    pendingDecorations_.deliverGeneratedBlockTicks(installed.takeGeneratedBlockTicks(), chunkAt);
    chunkVersion_.fetch_add(1);
    entityRegistry_.addPendingEntities(installed.takeSavedEntities());
    pendingDecorations_.addPendingGeneratedEntities(installed.takeGeneratedEntitiesMatching(
        [](const server::world::ChunkGeneratedEntity& entity) { return entity::mobs::isMob(entity.type); }));

    // The neighbours were lit without this chunk, so their light let the sky in through where it
    // now stands. parity: vanilla relights a chunk's neighbourhood on load.
    for (std::int32_t offsetZ = -1; offsetZ <= 1; ++offsetZ) {
        for (std::int32_t offsetX = -1; offsetX <= 1; ++offsetX) {
            if (offsetX == 0 && offsetZ == 0) {
                continue;
            }
            if (Chunk* neighbour = chunkAtLocked(key(chunkX + offsetX, chunkZ + offsetZ)); neighbour != nullptr) {
                neighbour->invalidateLight();
            }
        }
    }
    return installed;
}

Chunk& WorldState::install(Chunk chunk) {
    if (installOwner_ == nullptr) {
        mine("The set of chunks");
    }
    const WorldWriteLock lock{mutex_};
    return installLocked(std::move(chunk));
}

void WorldState::installLater(Chunk chunk) {
    sched::Owner* target = installOwner_ != nullptr ? installOwner_ : owner_;
    if (target == nullptr) {
        Chunk& installed = install(std::move(chunk));
        if (onChunkInstalled_) {
            onChunkInstalled_(installed);
        }
        return;
    }
    // Moved into the task rather than captured by reference: whoever asked for the chunk is not
    // waiting for it, and by the time the owner runs this their own copy is gone.
    target->submit([this, moved = std::make_shared<Chunk>(std::move(chunk))]() mutable {
        Chunk& installed = install(std::move(*moved));
        if (onChunkInstalled_) {
            onChunkInstalled_(installed);
        }
    });
}

std::optional<std::int32_t> WorldState::blockState(std::int32_t x, std::int32_t y, std::int32_t z) const {
    const WorldReadLock lock{mutex_};
    return blockStateLocked(x, y, z);
}

std::optional<std::int32_t> WorldState::blockStateLocked(std::int32_t x, std::int32_t y, std::int32_t z) const {
    const Chunk* chunk = chunkAtLocked(key(x >> 4, z >> 4));
    if (chunk == nullptr) {
        return std::nullopt;
    }
    // Above the sky or below the bedrock is not a block, which is what a mob that fell out of the
    // world asks about: the chunk refuses the question rather than answering it.
    if (y < chunk->verticalBounds().minY() || y > chunk->verticalBounds().maxY()) {
        return std::nullopt;
    }
    return chunk->blockStateId(x & 15, y, z & 15);
}

entity::entityCollision::BlockAt WorldState::blockAccessor() const {
    return [this](std::int32_t x, std::int32_t y, std::int32_t z) { return blockState(x, y, z); };
}

entity::entityCollision::BlockAt WorldState::lockedBlockAccessor() const {
    return [this](std::int32_t x, std::int32_t y, std::int32_t z) { return blockStateLocked(x, y, z); };
}

bool WorldState::setBlockState(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
    mine("A block");
    const WorldWriteLock lock{mutex_};
    Chunk* chunk = chunkAtLocked(key(x >> 4, z >> 4));
    if (chunk == nullptr) {
        return false;
    }
    // parity: `Level.setBlock` refuses a position outside build height rather than throwing, which
    // is what a plugin or a piston reaching past the sky asks for. The frozen Kotlin let the chunk
    // throw instead, and the throw came out on the tick thread.
    if (y < chunk->verticalBounds().minY() || y > chunk->verticalBounds().maxY()) {
        return false;
    }
    const bool changed = chunk->setBlockStateId(x & 15, y, z & 15, stateId);
    if (changed) {
        // A skylight opening or a torch reaches across chunk borders. Cached whole-chunk light is
        // cheaper than incremental light here, but every cached neighbour that could have read the
        // old block must be discarded together.
        for (std::int32_t offsetX = -1; offsetX <= 1; ++offsetX) {
            for (std::int32_t offsetZ = -1; offsetZ <= 1; ++offsetZ) {
                if (Chunk* neighbour = chunkAtLocked(key((x >> 4) + offsetX, (z >> 4) + offsetZ));
                    neighbour != nullptr) {
                    neighbour->invalidateLight();
                }
            }
        }
    }
    sculkSensors_.remember(x, y, z, stateId);
    return true;
}

bool WorldState::hasSculkSensorNear(double x, double y, double z, std::int32_t range) const {
    return sculkSensors_.hasNear(x, y, z, range);
}

void WorldState::scheduleFluidTick(std::int32_t x, std::int32_t y, std::int32_t z, const wire::common::Identifier& type,
                                   std::int32_t delay) {
    mine("A scheduled fluid tick");
    const WorldWriteLock lock{mutex_};
    Chunk* chunk = chunkAtLocked(key(x >> 4, z >> 4));
    if (chunk == nullptr || y < chunk->verticalBounds().minY() || y > chunk->verticalBounds().maxY()) {
        return;
    }
    chunk->scheduleFluidTick(
        ChunkScheduledTick{.type = type, .localX = x & 15, .y = y, .localZ = z & 15, .delay = delay});
}

void WorldState::scheduleBlockTick(std::int32_t x, std::int32_t y, std::int32_t z, const wire::common::Identifier& type,
                                   std::int32_t delay) {
    mine("A scheduled tick");
    const WorldWriteLock lock{mutex_};
    Chunk* chunk = chunkAtLocked(key(x >> 4, z >> 4));
    if (chunk == nullptr || y < chunk->verticalBounds().minY() || y > chunk->verticalBounds().maxY()) {
        return;
    }
    chunk->scheduleBlockTick(
        ChunkScheduledTick{.type = type, .localX = x & 15, .y = y, .localZ = z & 15, .delay = delay});
}

bool WorldState::hasScheduledBlockTick(std::int32_t x, std::int32_t y, std::int32_t z,
                                       const wire::common::Identifier& type) const {
    const WorldReadLock lock{mutex_};
    const Chunk* chunk = chunkAtLocked(key(x >> 4, z >> 4));
    if (chunk == nullptr) {
        return false;
    }
    for (const ChunkScheduledTick& tick : chunk->scheduledBlockTicks()) {
        if (tick.type == type && tick.localX == (x & 15) && tick.y == y && tick.localZ == (z & 15)) {
            return true;
        }
    }
    return false;
}

std::vector<std::array<std::int32_t, 3>>
WorldState::dueTicks(const std::function<void(Chunk&, std::vector<ChunkScheduledTick>&)>& advance) {
    std::vector<ChunkScheduledTick> due;
    std::vector<std::array<std::int32_t, 3>> positions;
    const WorldWriteLock lock{mutex_};
    for (const auto& [chunkKey, chunk] : chunksByPos_) {
        // Nothing to advance, and most chunks have nothing: the same walk without this ran the
        // whole loaded world through two empty vectors twenty times a second.
        if (chunk->scheduledTickCount() == 0) {
            continue;
        }
        const std::size_t before = due.size();
        advance(*chunk, due);
        for (std::size_t index = before; index < due.size(); ++index) {
            const ChunkScheduledTick& tick = due[index];
            positions.push_back(std::array<std::int32_t, 3>{(chunk->chunkX() << 4) + tick.localX, tick.y,
                                                            (chunk->chunkZ() << 4) + tick.localZ});
        }
    }
    return positions;
}

std::vector<std::array<std::int32_t, 3>> WorldState::dueFluidTicks() {
    return dueTicks([](Chunk& chunk, std::vector<ChunkScheduledTick>& due) { chunk.advanceFluidTicks(due); });
}

std::vector<std::array<std::int32_t, 3>> WorldState::dueBlockTicks() {
    return dueTicks([](Chunk& chunk, std::vector<ChunkScheduledTick>& due) { chunk.advanceBlockTicks(due); });
}

std::vector<GeneratedWorldEntity> WorldState::generatedEntities(const std::function<std::int32_t()>& allocateEntityId,
                                                                std::int32_t centerChunkX, std::int32_t centerChunkZ,
                                                                std::int32_t radiusChunks) {
    return pendingDecorations_.generatedEntities(allocateEntityId, centerChunkX, centerChunkZ, radiusChunks,
                                                 [this](std::int64_t chunkKey) { return chunkAtLocked(chunkKey); });
}

DroppedItem& WorldState::spawnItem(DroppedItem dropped) {
    mine("A dropped item");
    return itemPhysics_.spawnItem(std::move(dropped));
}

std::vector<DroppedItem> WorldState::items() const { return itemPhysics_.items(); }

std::vector<DroppedItem> WorldState::nearbyItems(double x, double y, double z, double radius) const {
    return itemPhysics_.nearbyItems(x, y, z, radius);
}

std::optional<DroppedItem> WorldState::removeItem(std::int32_t entityId) {
    mine("A dropped item");
    return itemPhysics_.removeItem(entityId);
}

std::vector<std::int32_t> WorldState::tickItems(const sched::Region* region) { return itemPhysics_.tickItems(region); }

std::vector<entity::ServerEntity*> WorldState::entities() const { return entityRegistry_.entities(); }

bool WorldState::hasNaturalSpawnEntityObstruction(const entity::Aabb& spawnBox, const entity::Aabb& entityBox) const {
    return entityRegistry_.hasNaturalSpawnObstruction(spawnBox, entityBox);
}

std::vector<entity::ServerEntity*> WorldState::entitiesIn(std::int32_t chunkX, std::int32_t chunkZ) const {
    return entityRegistry_.entitiesIn(chunkX, chunkZ);
}

std::vector<std::pair<std::int32_t, std::int32_t>> WorldState::chunksWithEntities() const {
    return entityRegistry_.chunksWithEntities();
}

void WorldState::forgetEntitiesIn(std::int32_t chunkX, std::int32_t chunkZ) {
    entityRegistry_.forgetEntitiesIn(chunkX, chunkZ);
}

std::vector<wire::nbt::NbtCompound> WorldState::takePendingEntities() { return entityRegistry_.takePendingEntities(); }

std::vector<server::world::ChunkGeneratedEntity> WorldState::takePendingGeneratedEntities() {
    return pendingDecorations_.takePendingGeneratedEntities();
}

entity::ServerEntity* WorldState::entity(std::int32_t entityId) const { return entityRegistry_.entity(entityId); }

entity::ServerEntity& WorldState::spawnEntity(std::unique_ptr<entity::ServerEntity> entity) {
    mine("An entity");
    return entityRegistry_.spawnEntity(std::move(entity));
}

std::vector<std::int32_t> WorldState::collectRemovedEntities() { return entityRegistry_.collectRemovedEntities(); }

void WorldState::primeTnt(std::int32_t x, std::int32_t y, std::int32_t z) { primedTntQueue_.primeTnt(x, y, z); }

std::vector<std::array<std::int32_t, 3>> WorldState::takePrimedTnt() { return primedTntQueue_.takePrimedTnt(); }

} // namespace basalt::server::runtime::world
