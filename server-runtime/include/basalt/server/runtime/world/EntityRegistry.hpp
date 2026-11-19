#pragma once

#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/WorldMutex.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
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

    /// The first entity in registry order the predicate accepts; it runs under the lock and must only read.
    template <typename Predicate>
    [[nodiscard]] entity::ServerEntity* findFirst(Predicate&& accepts) const {
        const WorldWriteLock lock{*mutex_};
        for (const std::unique_ptr<entity::ServerEntity>& entity : entities_) {
            if (accepts(*entity)) {
                return entity.get();
            }
        }
        return nullptr;
    }

    [[nodiscard]] bool hasNaturalSpawnObstruction(const entity::Aabb& spawnBox, const entity::Aabb& entityBox) const;

    [[nodiscard]] std::vector<entity::ServerEntity*> entitiesIn(std::int32_t chunkX, std::int32_t chunkZ) const;

    /// Appends to `out`, which is not cleared, what `entitiesIn` would return.
    void entitiesIn(std::int32_t chunkX, std::int32_t chunkZ, std::vector<entity::ServerEntity*>& out) const;

    /// Chunk by chunk (x outer, z inner), each in spawn order: what `entitiesIn` yields for every chunk of the range.
    void entitiesInRange(std::int32_t minChunkX, std::int32_t maxChunkX, std::int32_t minChunkZ, std::int32_t maxChunkZ,
                         std::vector<entity::ServerEntity*>& out) const;

    [[nodiscard]] std::vector<std::pair<std::int32_t, std::int32_t>> chunksWithEntities() const;

    using Matches = std::shared_ptr<const std::vector<entity::ServerEntity*>>;

    /// Forgets what `snapshotMatching` cached for the previous tick's entity snapshot.
    void beginSnapshot();

    /// Drops the cache if it belongs to `data`, before that snapshot vector is freed and its address reused.
    void endSnapshot(const entity::ServerEntity* const* data);

    /// The members of `snapshot`, in its order, that `accepts` takes, scanned once per key and tick.
    [[nodiscard]] Matches snapshotMatching(const std::vector<entity::ServerEntity*>& snapshot, const std::string& key,
                                           const std::function<bool(const entity::ServerEntity&)>& accepts) const;

    /// Throws away the per-chunk index so the next `entitiesIn` rebuilds it from where everything stands now.
    void beginTick() noexcept;

    void forgetEntitiesIn(std::int32_t chunkX, std::int32_t chunkZ);

    void addPendingEntities(std::vector<wire::nbt::NbtCompound> saved);

    [[nodiscard]] std::vector<wire::nbt::NbtCompound> takePendingEntities();

    [[nodiscard]] entity::ServerEntity* entity(std::int32_t entityId) const;

    entity::ServerEntity& spawnEntity(std::unique_ptr<entity::ServerEntity> entity);

    void setMobSettings(const config::MobSettings* settings) noexcept { mobSettings_ = settings; }

    [[nodiscard]] std::vector<std::int32_t> collectRemovedEntities();

    struct RemovalNotice {
        std::int32_t id;
        std::string_view cause;
    };

    /// The entities flagged removed that nobody has announced yet, still resolvable by id.
    [[nodiscard]] std::vector<RemovalNotice> takeRemovalNotices();

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

    void rebuildIndexIfStale() const;

    mutable std::vector<Indexed> candidates_;

    struct RangeKey {
        std::int64_t cell;
        Indexed held;
    };

    mutable std::vector<RangeKey> rangeKeys_;

    mutable std::mutex snapshotMutex_;
    mutable const entity::ServerEntity* const* snapshotData_ = nullptr;
    mutable std::size_t snapshotSize_ = 0;
    mutable std::unordered_map<std::string, Matches> snapshotMatches_;
};

} // namespace basalt::server::runtime::world
