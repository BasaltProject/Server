#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/PendingChunkDecorations.hpp>
#include <basalt/server/world/GeneratedBlockWrites.hpp>
#include <basalt/server/world/terrain/decoration/OreBlockRules.hpp>
#include <basalt/server/world/terrain/decoration/VegetationBlockRules.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeBlockRules.hpp>

#include <cstdint>
#include <unordered_set>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

using server::world::Chunk;
using server::world::ChunkBlockEntity;
using server::world::ChunkGeneratedBlockEntityWrite;
using server::world::ChunkGeneratedBlockTick;
using server::world::ChunkGeneratedBlockWrite;
using server::world::ChunkScheduledTick;
using server::world::GeneratedBlockWritePolicy;

[[nodiscard]] bool insideChunk(const Chunk& chunk, std::int32_t y) {
    return y >= chunk.verticalBounds().minY() && y <= chunk.verticalBounds().maxY();
}

/// parity: vanilla gets through a region's chunks in a raster, x outermost and ascending and z
/// within it. Measured on the jar over 441 chunks and two different load shapes: it is the pattern
/// the great majority of them follow, and it is what decides whose decoration sits on top where two
/// chunks write the same cell.
[[nodiscard]] bool decoratesBefore(std::int32_t chunkX, std::int32_t chunkZ, std::int32_t otherX, std::int32_t otherZ) {
    return chunkX != otherX ? chunkX < otherX : chunkZ < otherZ;
}

using server::world::generatedBlockWrites::BatchWritten;
using server::world::generatedBlockWrites::cellKey;

bool applyGeneratedBlockWrite(Chunk& chunk, const ChunkGeneratedBlockWrite& write, bool sourceGoesOnTop,
                              const BatchWritten* queued = nullptr) {
    return server::world::generatedBlockWrites::apply(chunk, write, sourceGoesOnTop, queued);
}

void applyGeneratedBlockEntityWrite(Chunk& chunk, const ChunkGeneratedBlockEntityWrite& write) {
    if (!insideChunk(chunk, write.y)) {
        return;
    }
    const std::int32_t localX = write.x & 15;
    const std::int32_t localZ = write.z & 15;
    // The block it belongs to has to still be the one that was generated: anything else means the
    // world moved on, and a chest with no chest under it is worse than no chest.
    if (chunk.blockStateId(localX, write.y, localZ) != write.expectedStateId) {
        return;
    }
    chunk.setBlockEntity(ChunkBlockEntity{localX, write.y, localZ, write.type, write.data});
}

void applyGeneratedBlockTick(Chunk& chunk, const ChunkGeneratedBlockTick& tick) {
    if (!insideChunk(chunk, tick.y)) {
        return;
    }
    const std::int32_t localX = tick.x & 15;
    const std::int32_t localZ = tick.z & 15;
    if (chunk.blockStateId(localX, tick.y, localZ) != tick.expectedStateId) {
        return;
    }
    chunk.scheduleBlockTick(ChunkScheduledTick{.type = tick.type,
                                               .localX = localX,
                                               .y = tick.y,
                                               .localZ = localZ,
                                               .delay = tick.delay,
                                               .priority = tick.priority});
}

} // namespace

std::int64_t PendingChunkDecorations::key(std::int32_t chunkX, std::int32_t chunkZ) {
    return (static_cast<std::int64_t>(chunkX) & 0xFFFFFFFFLL) | (static_cast<std::int64_t>(chunkZ) << 32);
}

void PendingChunkDecorations::addPendingGeneratedEntities(std::vector<server::world::ChunkGeneratedEntity> entities) {
    for (server::world::ChunkGeneratedEntity& entity : entities) {
        pendingGeneratedEntities_.push_back(std::move(entity));
    }
}

bool PendingChunkDecorations::sourceMayWrite(const std::int64_t chunkKey, const std::int64_t cellKey,
                                             const std::int32_t sourceChunkX, const std::int32_t sourceChunkZ) {
    const std::pair<std::int32_t, std::int32_t> source{sourceChunkX, sourceChunkZ};
    auto& cells = writeSourceByCell_[chunkKey];
    const auto found = cells.find(cellKey);
    if (found == cells.end()) {
        cells.emplace(cellKey, source);
        return true;
    }
    if (found->second == source) {
        return true;
    }
    // The one that decorates last wins, whichever arrived first. Reading the winner off the cell
    // instead made the result depend on which generation thread got there first.
    if (decoratesBefore(sourceChunkX, sourceChunkZ, found->second.first, found->second.second)) {
        return false;
    }
    found->second = source;
    return true;
}

std::optional<std::int32_t> PendingChunkDecorations::queuedWriteAt(const std::int32_t worldX, const std::int32_t y,
                                                                   const std::int32_t worldZ) const {
    const WorldReadLock lock{*mutex_};
    const auto found = pendingGeneratedBlockWrites_.find(key(worldX >> 4, worldZ >> 4));
    if (found == pendingGeneratedBlockWrites_.end()) {
        return std::nullopt;
    }
    // Later writes to one cell replace earlier ones, so the last match is what the proto chunk holds.
    std::optional<std::int32_t> held;
    for (const QueuedBlockWrite& entry : found->second) {
        if (entry.write.x == worldX && entry.write.y == y && entry.write.z == worldZ) {
            held = entry.write.stateId;
        }
    }
    return held;
}

void PendingChunkDecorations::forgetGeneratedWriteSources(const std::int64_t chunkKey) {
    writeSourceByCell_.erase(chunkKey);
}

void PendingChunkDecorations::forgetGeneratedEntities(std::int64_t chunkKey) {
    std::erase_if(generatedEntitiesByKey_, [chunkKey](const auto& entry) { return entry.first.chunkKey == chunkKey; });
}

void PendingChunkDecorations::deliverGeneratedBlockWrites(std::span<const ChunkGeneratedBlockWrite> writes,
                                                          const std::int32_t sourceChunkX,
                                                          const std::int32_t sourceChunkZ, const ChunkAt& chunkAt) {
    for (const ChunkGeneratedBlockWrite& write : writes) {
        const std::int32_t targetChunkX = write.x >> 4;
        const std::int32_t targetChunkZ = write.z >> 4;
        const std::int64_t targetKey = key(targetChunkX, targetChunkZ);
        Chunk* target = chunkAt(targetKey);
        if (target == nullptr) {
            pendingGeneratedBlockWrites_[targetKey].push_back(
                QueuedBlockWrite{.write = write, .sourceChunkX = sourceChunkX, .sourceChunkZ = sourceChunkZ});
        } else if (sourceMayWrite(targetKey, cellKey(write), sourceChunkX, sourceChunkZ)) {
            applyGeneratedBlockWrite(*target, write,
                                     decoratesBefore(targetChunkX, targetChunkZ, sourceChunkX, sourceChunkZ));
        }
    }
}

void PendingChunkDecorations::applyPendingGeneratedBlockWrites(Chunk& chunk, const WritePhase phase) {
    const auto found = pendingGeneratedBlockWrites_.find(key(chunk.chunkX(), chunk.chunkZ()));
    if (found == pendingGeneratedBlockWrites_.end()) {
        return;
    }
    std::vector<QueuedBlockWrite> writes = std::move(found->second);
    pendingGeneratedBlockWrites_.erase(found);
    std::vector<QueuedBlockWrite> notYet;
    BatchWritten queued;
    for (const QueuedBlockWrite& entry : writes) {
        const bool onTop = decoratesBefore(chunk.chunkX(), chunk.chunkZ(), entry.sourceChunkX, entry.sourceChunkZ);
        // What a later chunk wrote has to wait for this one to decorate, or its decoration would
        // cover a write that in vanilla went on last.
        if (phase == WritePhase::BeforeDecoration && onTop) {
            notYet.push_back(entry);
            continue;
        }
        if (!sourceMayWrite(key(chunk.chunkX(), chunk.chunkZ()), cellKey(entry.write), entry.sourceChunkX,
                            entry.sourceChunkZ)) {
            continue;
        }
        if (applyGeneratedBlockWrite(chunk, entry.write, onTop, &queued)) {
            queued.insert(cellKey(entry.write));
        }
    }
    if (!notYet.empty()) {
        pendingGeneratedBlockWrites_[key(chunk.chunkX(), chunk.chunkZ())] = std::move(notYet);
    }
}

void PendingChunkDecorations::deliverGeneratedBlockEntityWrites(std::span<const ChunkGeneratedBlockEntityWrite> writes,
                                                                const ChunkAt& chunkAt) {
    for (const ChunkGeneratedBlockEntityWrite& write : writes) {
        const std::int64_t targetKey = key(write.x >> 4, write.z >> 4);
        Chunk* target = chunkAt(targetKey);
        if (target == nullptr) {
            pendingGeneratedBlockEntityWrites_[targetKey].push_back(write);
        } else {
            applyGeneratedBlockEntityWrite(*target, write);
        }
    }
}

void PendingChunkDecorations::applyPendingGeneratedBlockEntityWrites(Chunk& chunk) {
    const auto found = pendingGeneratedBlockEntityWrites_.find(key(chunk.chunkX(), chunk.chunkZ()));
    if (found == pendingGeneratedBlockEntityWrites_.end()) {
        return;
    }
    const std::vector<ChunkGeneratedBlockEntityWrite> writes = std::move(found->second);
    pendingGeneratedBlockEntityWrites_.erase(found);
    for (const ChunkGeneratedBlockEntityWrite& write : writes) {
        applyGeneratedBlockEntityWrite(chunk, write);
    }
}

void PendingChunkDecorations::deliverGeneratedBlockTicks(std::span<const ChunkGeneratedBlockTick> ticks,
                                                         const ChunkAt& chunkAt) {
    for (const ChunkGeneratedBlockTick& tick : ticks) {
        const std::int64_t targetKey = key(tick.x >> 4, tick.z >> 4);
        Chunk* target = chunkAt(targetKey);
        if (target == nullptr) {
            pendingGeneratedBlockTicks_[targetKey].push_back(tick);
        } else {
            applyGeneratedBlockTick(*target, tick);
        }
    }
}

void PendingChunkDecorations::applyPendingGeneratedBlockTicks(Chunk& chunk) {
    const auto found = pendingGeneratedBlockTicks_.find(key(chunk.chunkX(), chunk.chunkZ()));
    if (found == pendingGeneratedBlockTicks_.end()) {
        return;
    }
    const std::vector<ChunkGeneratedBlockTick> ticks = std::move(found->second);
    pendingGeneratedBlockTicks_.erase(found);
    for (const ChunkGeneratedBlockTick& tick : ticks) {
        applyGeneratedBlockTick(chunk, tick);
    }
}

std::vector<GeneratedWorldEntity>
PendingChunkDecorations::generatedEntities(const std::function<std::int32_t()>& allocateEntityId,
                                           std::int32_t centerChunkX, std::int32_t centerChunkZ,
                                           std::int32_t radiusChunks, const ChunkAt& chunkAt) {
    const WorldWriteLock lock{*mutex_};
    std::vector<GeneratedWorldEntity> result;
    for (std::int32_t chunkX = centerChunkX - radiusChunks; chunkX <= centerChunkX + radiusChunks; ++chunkX) {
        for (std::int32_t chunkZ = centerChunkZ - radiusChunks; chunkZ <= centerChunkZ + radiusChunks; ++chunkZ) {
            const std::int64_t chunkKey = key(chunkX, chunkZ);
            const Chunk* chunk = chunkAt(chunkKey);
            if (chunk == nullptr) {
                continue;
            }
            const std::span<const server::world::ChunkGeneratedEntity> entities = chunk->generatedEntities();
            for (std::size_t index = 0; index < entities.size(); ++index) {
                const GeneratedEntityKey entityKey{.chunkKey = chunkKey, .index = static_cast<std::int32_t>(index)};
                const auto existing = generatedEntitiesByKey_.find(entityKey);
                if (existing != generatedEntitiesByKey_.end()) {
                    result.push_back(existing->second);
                    continue;
                }
                // The identity is handed out once and kept: a client that sees the same mob twice
                // in two ticks would draw two of it.
                GeneratedWorldEntity identity{.entityId = allocateEntityId(),
                                              .uuid = wire::common::Uuid::random(),
                                              .descriptor = entities[index]};
                generatedEntitiesByKey_.emplace(entityKey, identity);
                result.push_back(std::move(identity));
            }
        }
    }
    return result;
}

std::vector<server::world::ChunkGeneratedEntity> PendingChunkDecorations::takePendingGeneratedEntities() {
    const WorldWriteLock lock{*mutex_};
    std::vector<server::world::ChunkGeneratedEntity> taken;
    taken.swap(pendingGeneratedEntities_);
    return taken;
}

} // namespace basalt::server::runtime::world
