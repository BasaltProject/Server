#pragma once

#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/WorldMutex.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace basalt::server::runtime::config {
struct MobSettings;
}

namespace basalt::server::runtime::world {

class EntityRegistry {
public:
    explicit EntityRegistry(WorldMutex& mutex) : mutex_(&mutex) {}

    [[nodiscard]] std::vector<entity::ServerEntity*> entities() const;

    [[nodiscard]] bool hasNaturalSpawnObstruction(const entity::Aabb& spawnBox, const entity::Aabb& entityBox) const;

    [[nodiscard]] std::vector<entity::ServerEntity*> entitiesIn(std::int32_t chunkX, std::int32_t chunkZ) const;

    [[nodiscard]] std::vector<std::pair<std::int32_t, std::int32_t>> chunksWithEntities() const;

    /// Throws away the per-chunk index so the next `entitiesIn` rebuilds it from where everything stands now.
    void beginTick() noexcept;

    void forgetEntitiesIn(std::int32_t chunkX, std::int32_t chunkZ);

    void addPendingEntities(std::vector<wire::nbt::NbtCompound> saved);

    [[nodiscard]] std::vector<wire::nbt::NbtCompound> takePendingEntities();

    [[nodiscard]] entity::ServerEntity* entity(std::int32_t entityId) const;

    entity::ServerEntity& spawnEntity(std::unique_ptr<entity::ServerEntity> entity);

    void setMobSettings(const config::MobSettings* settings) noexcept { mobSettings_ = settings; }

    [[nodiscard]] std::vector<std::int32_t> collectRemovedEntities();

private:
    WorldMutex* mutex_;
    const config::MobSettings* mobSettings_ = nullptr;
    std::vector<std::unique_ptr<entity::ServerEntity>> entities_;
    std::unordered_map<std::int32_t, entity::ServerEntity*> byId_;
    std::vector<wire::nbt::NbtCompound> pendingEntities_;

    struct Indexed {
        entity::ServerEntity* entity;
        std::size_t order;
    };

    /// Entities by the chunk they stood in when it was built. Stale by at most a tick, which `entitiesIn` makes up
    /// for by reading the neighbours and checking each hit against where it stands now. Guarded by `mutex_`.
    mutable std::unordered_map<std::int64_t, std::vector<Indexed>> index_;
    mutable bool indexValid_ = false;
};

} // namespace basalt::server::runtime::world
