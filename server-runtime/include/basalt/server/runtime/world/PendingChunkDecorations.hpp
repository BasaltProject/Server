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

/// One generated entity's runtime identity, handed out the first time a client asks about it.
struct GeneratedWorldEntity {
    std::int32_t entityId = 0;
    wire::common::Uuid uuid;
    server::world::ChunkGeneratedEntity descriptor;
};

/// The mobs a template spawned and the runtime identities handed out for the generated entities a
/// chunk carries, plus what a chunk decorated on its own left past its edge.
///
/// The deliver methods take no lock of their own: callers already hold the world's mutex. The two
/// that are reached without one held take it themselves.
class PendingChunkDecorations {
public:
    /// What the world says is loaded at a packed chunk key, or nothing when it is not.
    using ChunkAt = std::function<server::world::Chunk*(std::int64_t chunkKey)>;

    explicit PendingChunkDecorations(WorldMutex& mutex) : mutex_(&mutex) {}

    /// The packed key both this and the chunk map are keyed by.
    [[nodiscard]] static std::int64_t key(std::int32_t chunkX, std::int32_t chunkZ);

    /// Queues the mobs a just-installed chunk's structures brought with it. Called with no lock
    /// held.
    void addPendingGeneratedEntities(std::vector<server::world::ChunkGeneratedEntity> entities);

    /// Forgets the generated-entity identities that belonged to this now-evicted chunk.
    void forgetGeneratedEntities(std::int64_t chunkKey);

    /// Writes a chunk decorated outside the generation pipeline left past its edge, landing in the
    /// neighbours that are loaded. The pipeline writes into its neighbours directly and leaves none.
    static void deliverGeneratedBlockWrites(std::span<const server::world::ChunkGeneratedBlockWrite> writes,
                                            const ChunkAt& chunkAt);

    static void deliverGeneratedBlockEntityWrites(std::span<const server::world::ChunkGeneratedBlockEntityWrite> writes,
                                                  const ChunkAt& chunkAt);

    static void deliverGeneratedBlockTicks(std::span<const server::world::ChunkGeneratedBlockTick> ticks,
                                           const ChunkAt& chunkAt);

    /// Runtime identities for the entities of generated chunks within `radiusChunks` of the centre.
    /// Bounded because it runs per player per tick under the world's mutex. Walked in coordinate
    /// order, so an id depends only on position; evicting the chunk forgets them.
    [[nodiscard]] std::vector<GeneratedWorldEntity>
    generatedEntities(const std::function<std::int32_t()>& allocateEntityId, std::int32_t centerChunkX,
                      std::int32_t centerChunkZ, std::int32_t radiusChunks, const ChunkAt& chunkAt);

    /// Mobs emitted by structure templates, waiting for the server tick to allocate runtime ids.
    ///
    /// Taking them removes the only pending copy; their chunk descriptor was removed at install so
    /// the live entity saved with the chunk cannot be duplicated on the next load.
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
