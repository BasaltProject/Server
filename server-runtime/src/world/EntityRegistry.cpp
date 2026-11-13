#include <basalt/server/runtime/entity/MobOverrides.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/world/EntityRegistry.hpp>
#include <basalt/server/runtime/world/NaturalSpawnObstruction.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

[[nodiscard]] std::int32_t chunkOf(double coordinate) { return wire::common::mth::floor(coordinate) >> 4; }

// Equal to `chunkOf(coordinate) == chunk`, with the two range compares standing in for the floor when it is exact.
[[nodiscard]] bool inChunk(double coordinate, std::int32_t chunk) {
    constexpr double kExactBound = 1.0e9;
    if (std::abs(coordinate) < kExactBound) {
        const double low = static_cast<double>(chunk) * 16.0;
        return coordinate >= low && coordinate < low + 16.0;
    }
    return chunkOf(coordinate) == chunk;
}

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

void EntityRegistry::beginTick() noexcept { indexValid_ = false; }

std::vector<entity::ServerEntity*> EntityRegistry::entitiesIn(std::int32_t chunkX, std::int32_t chunkZ) const {
    std::vector<entity::ServerEntity*> found;
    entitiesIn(chunkX, chunkZ, found);
    return found;
}

void EntityRegistry::entitiesIn(std::int32_t chunkX, std::int32_t chunkZ,
                                std::vector<entity::ServerEntity*>& out) const {
    const WorldWriteLock lock{*mutex_};
    if (!indexValid_) {
        index_.clear();
        for (std::size_t order = 0; order < entities_.size(); ++order) {
            entity::ServerEntity* entity = entities_[order].get();
            const std::int64_t bucket =
                (static_cast<std::int64_t>(chunkOf(entity->x)) << 32) | static_cast<std::uint32_t>(chunkOf(entity->z));
            index_[bucket].push_back({entity, order});
        }
        indexValid_ = true;
    }
    // Nothing moves a chunk's width in one tick, so the neighbours hold whatever walked in since the build.
    candidates_.clear();
    for (std::int32_t dx = -1; dx <= 1; ++dx) {
        for (std::int32_t dz = -1; dz <= 1; ++dz) {
            const auto bucket =
                index_.find((static_cast<std::int64_t>(chunkX + dx) << 32) | static_cast<std::uint32_t>(chunkZ + dz));
            if (bucket == index_.end()) {
                continue;
            }
            for (const Indexed& held : bucket->second) {
                if (!held.entity->removed && inChunk(held.entity->x, chunkX) && inChunk(held.entity->z, chunkZ)) {
                    candidates_.push_back(held);
                }
            }
        }
    }
    if (!std::ranges::is_sorted(candidates_, {}, &Indexed::order)) {
        std::ranges::sort(candidates_, {}, &Indexed::order);
    }
    for (const Indexed& held : candidates_) {
        out.push_back(held.entity);
    }
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
    indexValid_ = false;
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
    if (mobSettings_ != nullptr) {
        entity::mobOverrides::apply(*entity, *mobSettings_);
    }
    entity::cubeMobs::applySize(*entity);
    const std::int32_t id = entity->entityId();
    if (byId_.erase(id) != 0) {
        indexValid_ = false;
        std::erase_if(entities_, [id](const std::unique_ptr<entity::ServerEntity>& existing) {
            return existing->entityId() == id;
        });
    }
    entity::ServerEntity& spawned = *entity;
    entities_.push_back(std::move(entity));
    byId_.emplace(id, &spawned);
    if (indexValid_) {
        index_[(static_cast<std::int64_t>(chunkOf(spawned.x)) << 32) | static_cast<std::uint32_t>(chunkOf(spawned.z))]
            .push_back({&spawned, entities_.size() - 1});
    }
    return spawned;
}

std::vector<std::int32_t> EntityRegistry::collectRemovedEntities() {
    const WorldWriteLock lock{*mutex_};
    std::vector<std::int32_t> gone;
    for (const std::unique_ptr<entity::ServerEntity>& entity : entities_) {
        if (entity->removed) {
            indexValid_ = false;
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
