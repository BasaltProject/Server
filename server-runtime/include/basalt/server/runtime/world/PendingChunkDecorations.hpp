#pragma once

#include <basalt/server/runtime/world/WorldMutex.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/ChunkTypes.hpp>
#include <basalt/wire/common/Uuid.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <vector>

namespace basalt::server::runtime::world {

/// One generated entity's runtime identity, handed out the first time a client asks about it.
struct GeneratedWorldEntity {
    std::int32_t entityId = 0;
    wire::common::Uuid uuid;
    server::world::ChunkGeneratedEntity descriptor;
};

/// Decorations that crossed a chunk border during generation (block writes, block entities,
/// scheduled ticks) and the mobs a template spawned, plus the runtime identities handed out for the
/// generated entities a chunk carries.
///
/// The deliver and apply methods take no lock of their own: callers already hold the world's mutex.
/// The two that are reached without one held take it themselves.
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

    /// When a queued write gets to land, relative to the receiving chunk's own decoration.
    enum class WritePhase : std::uint8_t {
        /// Everything a chunk that decorates earlier left for this one, so its decoration goes on
        /// top of them, exactly as it does in vanilla.
        BeforeDecoration,
        /// Whatever is left, which is what later chunks wrote and what arrived after the fact.
        AfterDecoration,
    };

    /// Delivers decorations that crossed out of their source chunk without ever wrapping them back
    /// into it. A target that has not arrived yet keeps the write until its own install.
    void deliverGeneratedBlockWrites(std::span<const server::world::ChunkGeneratedBlockWrite> writes,
                                     std::int32_t sourceChunkX, std::int32_t sourceChunkZ, const ChunkAt& chunkAt);

    void applyPendingGeneratedBlockWrites(server::world::Chunk& chunk, WritePhase phase);

    void deliverGeneratedBlockEntityWrites(std::span<const server::world::ChunkGeneratedBlockEntityWrite> writes,
                                           const ChunkAt& chunkAt);

    void applyPendingGeneratedBlockEntityWrites(server::world::Chunk& chunk);

    void deliverGeneratedBlockTicks(std::span<const server::world::ChunkGeneratedBlockTick> ticks,
                                    const ChunkAt& chunkAt);

    void applyPendingGeneratedBlockTicks(server::world::Chunk& chunk);

    /// Runtime identities for the entities of generated chunks within `radiusChunks` of the centre.
    ///
    /// Bounded because this runs once per player per tick holding the world's mutex, against the
    /// lock the generation workers need. Walked in coordinate order rather than sorted, so an
    /// entity's id still depends only on position; a chunk eviction is what forgets them.
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

    /// A queued write plus where it came from: which of the two chunks decorates first decides
    /// whether it lands under the receiver's own decoration or over it.
    struct QueuedBlockWrite {
        server::world::ChunkGeneratedBlockWrite write;
        std::int32_t sourceChunkX = 0;
        std::int32_t sourceChunkZ = 0;
    };

    std::map<std::int64_t, std::vector<QueuedBlockWrite>> pendingGeneratedBlockWrites_;
    std::map<std::int64_t, std::vector<server::world::ChunkGeneratedBlockEntityWrite>>
        pendingGeneratedBlockEntityWrites_;
    std::map<std::int64_t, std::vector<server::world::ChunkGeneratedBlockTick>> pendingGeneratedBlockTicks_;
};

} // namespace basalt::server::runtime::world
