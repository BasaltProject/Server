#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/PendingChunkDecorations.hpp>
#include <basalt/server/world/terrain/decoration/OreBlockRules.hpp>
#include <basalt/server/world/terrain/decoration/VegetationBlockRules.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeBlockRules.hpp>

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

[[nodiscard]] bool isOrePolicy(GeneratedBlockWritePolicy policy) {
    return policy == GeneratedBlockWritePolicy::OreBaseStone || policy == GeneratedBlockWritePolicy::OreStone ||
           policy == GeneratedBlockWritePolicy::OreDeepslate;
}

/// A queued write was issued while its target did not exist yet. parity: in vanilla the target is
/// always there as a proto chunk, so the write lands before that chunk decorates itself and whatever
/// its own decoration puts on top wins. Only the batch's earlier writes may be overwritten, because
/// among themselves they keep the order they were issued in.
using BatchWritten = std::unordered_set<std::int64_t>;

[[nodiscard]] std::int64_t cellKey(const ChunkGeneratedBlockWrite& write) {
    return (static_cast<std::int64_t>(write.x & 15) << 40) | (static_cast<std::int64_t>(write.z & 15) << 32) |
           (static_cast<std::int64_t>(write.y) & 0xFFFFFFFFLL);
}

bool applyGeneratedBlockWrite(Chunk& chunk, const ChunkGeneratedBlockWrite& write,
                             const BatchWritten* queued = nullptr) {
    if (!insideChunk(chunk, write.y)) {
        return false;
    }
    const std::int32_t localX = write.x & 15;
    const std::int32_t localZ = write.z & 15;
    const std::int32_t current = chunk.blockStateId(localX, write.y, localZ);
    const bool decorated = queued != nullptr && current != write.expectedStateId && !queued->contains(cellKey(write));
    const bool loosely = !(decorated && isOrePolicy(write.policy)) &&
                         server::world::terrain::decoration::oreBlockRules::canReplaceDeferred(write.policy, current);
    const bool canReplace =
        write.replaceAnyState || current == write.expectedStateId ||
        (!decorated && write.policy == GeneratedBlockWritePolicy::Tree &&
         server::world::terrain::decoration::tree::treeBlockRules::canReplaceDeferred(current, write.stateId)) ||
        loosely;
    if (!canReplace) {
        return false;
    }
    const std::int32_t stateId =
        write.policy == GeneratedBlockWritePolicy::Tree
            ? server::world::terrain::decoration::tree::treeBlockRules::resolveDeferredState(current, write.stateId)
            : write.stateId;
    // The ground this plant was validated against may have been refused, or replaced by whatever the
    // receiving chunk generated. A plant with nothing under it breaks on the first block update.
    const std::int32_t below = write.y > chunk.verticalBounds().minY() ? chunk.blockStateId(localX, write.y - 1, localZ)
                                                                       : registry::blocks::air().stateId;
    if (!server::world::terrain::decoration::vegetationBlockRules::survivesDeferred(stateId, below)) {
        return false;
    }
    chunk.setBlockStateId(localX, write.y, localZ, stateId);
    return true;
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
        const std::int64_t targetKey = key(write.x >> 4, write.z >> 4);
        Chunk* target = chunkAt(targetKey);
        if (target == nullptr) {
            pendingGeneratedBlockWrites_[targetKey].push_back(write);
        } else {
            applyGeneratedBlockWrite(*target, write);
        }
    }
}

void PendingChunkDecorations::applyPendingGeneratedBlockWrites(Chunk& chunk) {
    const auto found = pendingGeneratedBlockWrites_.find(key(chunk.chunkX(), chunk.chunkZ()));
    if (found == pendingGeneratedBlockWrites_.end()) {
        return;
    }
    const std::vector<ChunkGeneratedBlockWrite> writes = std::move(found->second);
    pendingGeneratedBlockWrites_.erase(found);
    BatchWritten queued;
    for (const ChunkGeneratedBlockWrite& write : writes) {
        if (applyGeneratedBlockWrite(chunk, write, &queued)) {
            queued.insert(cellKey(write));
        }
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
