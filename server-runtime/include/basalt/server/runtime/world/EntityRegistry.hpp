#pragma once

#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/WorldMutex.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace basalt::server::runtime::world {

/// Everything `/summon` (or a loaded chunk, or a plugin) put into this world: the live entity
/// table, plus the staging list a chunk load feeds before the tick loop can allocate runtime ids
/// for what it brought back.
///
/// Shares the world's single mutex rather than owning one of its own: splitting lock granularity
/// changes race outcomes between the tick thread and the save and eviction threads, which is a real
/// behaviour change even where it has nothing to do with vanilla parity.
class EntityRegistry {
public:
    explicit EntityRegistry(WorldMutex& mutex) : mutex_(&mutex) {}

    /// Everything `/summon` put here, alive or just removed and not yet forgotten by clients.
    [[nodiscard]] std::vector<entity::ServerEntity*> entities() const;

    /// Obstruction query without copying the whole table for every spawn attempt.
    [[nodiscard]] bool hasNaturalSpawnObstruction(const entity::Aabb& spawnBox, const entity::Aabb& entityBox) const;

    /// The live entities standing in this chunk, which is what gets written beside its blocks.
    [[nodiscard]] std::vector<entity::ServerEntity*> entitiesIn(std::int32_t chunkX, std::int32_t chunkZ) const;

    /// Which chunks hold entities, so a save knows to write them even when no block moved.
    [[nodiscard]] std::vector<std::pair<std::int32_t, std::int32_t>> chunksWithEntities() const;

    /// Forgets the entities in this chunk: they are on disk now, and the chunk is going.
    void forgetEntitiesIn(std::int32_t chunkX, std::int32_t chunkZ);

    /// Queues a chunk's saved entities.
    ///
    /// They cannot be spawned where the chunk is loaded: that happens on a generation thread, and
    /// an entity id belongs to the server. parity: vanilla defers the same work to the entity
    /// manager.
    void addPendingEntities(std::vector<wire::nbt::NbtCompound> saved);

    [[nodiscard]] std::vector<wire::nbt::NbtCompound> takePendingEntities();

    [[nodiscard]] entity::ServerEntity* entity(std::int32_t entityId) const;

    entity::ServerEntity& spawnEntity(std::unique_ptr<entity::ServerEntity> entity);

    /// Drops everything marked removed, and answers their ids so clients can be told once.
    [[nodiscard]] std::vector<std::int32_t> collectRemovedEntities();

private:
    WorldMutex* mutex_;
    /// In the order they were spawned, which is the order every listing here answers in.
    std::vector<std::unique_ptr<entity::ServerEntity>> entities_;
    std::vector<wire::nbt::NbtCompound> pendingEntities_;
};

} // namespace basalt::server::runtime::world
