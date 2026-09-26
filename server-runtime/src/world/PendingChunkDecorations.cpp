#include <basalt/server/runtime/world/PendingChunkDecorations.hpp>
#include <basalt/server/world/GeneratedBlockWrites.hpp>

#include <cstdint>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

using server::world::Chunk;
using server::world::ChunkBlockEntity;
using server::world::ChunkGeneratedBlockEntityWrite;
using server::world::ChunkGeneratedBlockTick;
using server::world::ChunkGeneratedBlockWrite;
using server::world::ChunkScheduledTick;

[[nodiscard]] bool insideChunk(const Chunk& chunk, std::int32_t y) {
    return y >= chunk.verticalBounds().minY() && y <= chunk.verticalBounds().maxY();
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

void PendingChunkDecorations::forgetGeneratedEntities(std::int64_t chunkKey) {
    std::erase_if(generatedEntitiesByKey_, [chunkKey](const auto& entry) { return entry.first.chunkKey == chunkKey; });
}

void PendingChunkDecorations::deliverGeneratedBlockWrites(std::span<const ChunkGeneratedBlockWrite> writes,
                                                          const ChunkAt& chunkAt) {
    for (const ChunkGeneratedBlockWrite& write : writes) {
        if (Chunk* target = chunkAt(key(write.x >> 4, write.z >> 4)); target != nullptr) {
            static_cast<void>(server::world::generatedBlockWrites::apply(*target, write, true));
        }
    }
}

void PendingChunkDecorations::deliverGeneratedBlockEntityWrites(std::span<const ChunkGeneratedBlockEntityWrite> writes,
                                                                const ChunkAt& chunkAt) {
    for (const ChunkGeneratedBlockEntityWrite& write : writes) {
        if (Chunk* target = chunkAt(key(write.x >> 4, write.z >> 4)); target != nullptr) {
            applyGeneratedBlockEntityWrite(*target, write);
        }
    }
}

void PendingChunkDecorations::deliverGeneratedBlockTicks(std::span<const ChunkGeneratedBlockTick> ticks,
                                                         const ChunkAt& chunkAt) {
    for (const ChunkGeneratedBlockTick& tick : ticks) {
        if (Chunk* target = chunkAt(key(tick.x >> 4, tick.z >> 4)); target != nullptr) {
            applyGeneratedBlockTick(*target, tick);
        }
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
