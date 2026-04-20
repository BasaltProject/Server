#pragma once

#include <basalt/server/runtime/world/WorldMutex.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/ChunkTypes.hpp>
#include <basalt/wire/common/Uuid.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <span>
#include <utility>
#include <vector>

namespace basalt::server::runtime::world {

struct GeneratedWorldEntity {
    std::int32_t entityId = 0;
    wire::common::Uuid uuid;
    server::world::ChunkGeneratedEntity descriptor;
};

class PendingChunkDecorations {
public:
    using ChunkAt = std::function<server::world::Chunk*(std::int64_t chunkKey)>;

    explicit PendingChunkDecorations(WorldMutex& mutex) : mutex_(&mutex) {}

    [[nodiscard]] static std::int64_t key(std::int32_t chunkX, std::int32_t chunkZ);

    /// Called with no lock held.
    void addPendingGeneratedEntities(std::vector<server::world::ChunkGeneratedEntity> entities);

    void forgetGeneratedEntities(std::int64_t chunkKey);

    static void deliverGeneratedBlockWrites(std::span<const server::world::ChunkGeneratedBlockWrite> writes,
                                            const ChunkAt& chunkAt);

    static void deliverGeneratedBlockEntityWrites(std::span<const server::world::ChunkGeneratedBlockEntityWrite> writes,
                                                  const ChunkAt& chunkAt);

    static void deliverGeneratedBlockTicks(std::span<const server::world::ChunkGeneratedBlockTick> ticks,
                                           const ChunkAt& chunkAt);

    [[nodiscard]] std::vector<GeneratedWorldEntity>
    generatedEntities(const std::function<std::int32_t()>& allocateEntityId, std::int32_t centerChunkX,
                      std::int32_t centerChunkZ, std::int32_t radiusChunks, const ChunkAt& chunkAt);

    [[nodiscard]] std::vector<server::world::ChunkGeneratedEntity> takePendingGeneratedEntities();

private:
    struct GeneratedEntityKey {
        std::int64_t chunkKey = 0;
        std::int32_t index = 0;

        friend auto operator<=>(const GeneratedEntityKey&, const GeneratedEntityKey&) = default;
    };

    WorldMutex* mutex_;
    std::map<GeneratedEntityKey, GeneratedWorldEntity> generatedEntitiesByKey_;
    std::vector<server::world::ChunkGeneratedEntity> pendingGeneratedEntities_;
};

} // namespace basalt::server::runtime::world
