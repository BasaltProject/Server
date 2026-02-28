#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/world/EntityRegistry.hpp>
#include <basalt/server/runtime/world/NaturalSpawnObstruction.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

[[nodiscard]] std::int32_t chunkOf(double coordinate) { return wire::common::mth::floor(coordinate) >> 4; }

} // namespace

std::vector<entity::ServerEntity*> EntityRegistry::entities() const {
    const WorldWriteLock lock{*mutex_};
    std::vector<entity::ServerEntity*> every;
    every.reserve(entities_.size());
    for (const std::unique_ptr<entity::ServerEntity>& entity : entities_) {
        every.push_back(entity.get());
    }
    return every;
}

bool EntityRegistry::hasNaturalSpawnObstruction(const entity::Aabb& spawnBox, const entity::Aabb& entityBox) const {
    const WorldWriteLock lock{*mutex_};
    for (const std::unique_ptr<entity::ServerEntity>& entity : entities_) {
        if (naturalSpawnObstruction::entityBlocksSpawn(*entity, spawnBox, entityBox)) {
            return true;
        }
    }
    return false;
}

std::vector<entity::ServerEntity*> EntityRegistry::entitiesIn(std::int32_t chunkX, std::int32_t chunkZ) const {
    const WorldWriteLock lock{*mutex_};
    std::vector<entity::ServerEntity*> found;
    for (const std::unique_ptr<entity::ServerEntity>& entity : entities_) {
        if (!entity->removed && chunkOf(entity->x) == chunkX && chunkOf(entity->z) == chunkZ) {
            found.push_back(entity.get());
        }
    }
    return found;
}

std::vector<std::pair<std::int32_t, std::int32_t>> EntityRegistry::chunksWithEntities() const {
    const WorldWriteLock lock{*mutex_};
    std::vector<std::pair<std::int32_t, std::int32_t>> chunks;
    for (const std::unique_ptr<entity::ServerEntity>& entity : entities_) {
        if (entity->removed) {
            continue;
        }
        const std::pair<std::int32_t, std::int32_t> chunk{chunkOf(entity->x), chunkOf(entity->z)};
        if (std::ranges::find(chunks, chunk) == chunks.end()) {
            chunks.push_back(chunk);
        }
    }
    return chunks;
}

void EntityRegistry::forgetEntitiesIn(std::int32_t chunkX, std::int32_t chunkZ) {
    const WorldWriteLock lock{*mutex_};
    // Removed or not: the chunk is going, and what was in it has just been written to disk.
    std::erase_if(entities_, [this, chunkX, chunkZ](const std::unique_ptr<entity::ServerEntity>& entity) {
        if (chunkOf(entity->x) != chunkX || chunkOf(entity->z) != chunkZ) {
            return false;
        }
        byId_.erase(entity->entityId());
        return true;
    });
}

void EntityRegistry::addPendingEntities(std::vector<wire::nbt::NbtCompound> saved) {
    // Called from the install path, which already holds the lock.
    for (wire::nbt::NbtCompound& entity : saved) {
        pendingEntities_.push_back(std::move(entity));
    }
}

std::vector<wire::nbt::NbtCompound> EntityRegistry::takePendingEntities() {
    const WorldWriteLock lock{*mutex_};
    std::vector<wire::nbt::NbtCompound> taken;
    taken.swap(pendingEntities_);
    return taken;
}

entity::ServerEntity* EntityRegistry::entity(std::int32_t entityId) const {
    const WorldWriteLock lock{*mutex_};
    const auto found = byId_.find(entityId);
    return found == byId_.end() ? nullptr : found->second;
}

entity::ServerEntity& EntityRegistry::spawnEntity(std::unique_ptr<entity::ServerEntity> entity) {
    const WorldWriteLock lock{*mutex_};
    // A cube's speed, health and damage all derive from the size chosen when it was finalized.
    entity::cubeMobs::applySize(*entity);
    const std::int32_t id = entity->entityId();
    // Spawning the same id twice replaces, as putting into a map by id did. The index says whether
    // there is anything to replace, so the usual case never walks the list.
    if (byId_.erase(id) != 0) {
        std::erase_if(entities_, [id](const std::unique_ptr<entity::ServerEntity>& existing) {
            return existing->entityId() == id;
        });
    }
    entity::ServerEntity& spawned = *entity;
    entities_.push_back(std::move(entity));
    byId_.emplace(id, &spawned);
    return spawned;
}

std::vector<std::int32_t> EntityRegistry::collectRemovedEntities() {
    const WorldWriteLock lock{*mutex_};
    std::vector<std::int32_t> gone;
    for (const std::unique_ptr<entity::ServerEntity>& entity : entities_) {
        if (entity->removed) {
            gone.push_back(entity->entityId());
        }
    }
    std::erase_if(entities_, [this](const std::unique_ptr<entity::ServerEntity>& entity) {
        if (!entity->removed) {
            return false;
        }
        byId_.erase(entity->entityId());
        return true;
    });
    return gone;
}

} // namespace basalt::server::runtime::world
