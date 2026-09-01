#pragma once

#include <basalt/server/entity/ecs/EntityId.hpp>

#include <cstdint>
#include <deque>
#include <vector>

namespace basalt::server::entity::ecs {

/// Owns entity id lifecycle: allocation, reusing destroyed indices before growing, and bumping an
/// index's generation on destroy so a stale id compares dead even after its slot is recycled. Owns
/// no component data: that lives in whichever `ComponentStore`s the caller composes alongside a
/// `World`, deliberately, so no single storage shape is locked in.
class World {
public:
    [[nodiscard]] EntityId create();

    void destroy(EntityId id);

    [[nodiscard]] bool isAlive(EntityId id) const noexcept;

private:
    void ensureCapacity(std::int32_t minSize);

    std::vector<std::int32_t> generations_;
    std::vector<bool> alive_;
    std::deque<std::int32_t> freeIndices_;
    std::int32_t nextIndex_ = 0;
};

/// A lightweight view over one entity: the shape a plugin binding sees, on top of the
/// struct-of-arrays storage everything here uses. Owns no state beyond a world and an id, so making
/// one is free and there is nothing to keep in sync.
class EntityHandle {
public:
    EntityHandle(World& world, EntityId id) : world_(&world), id_(id) {}

    [[nodiscard]] World& world() const noexcept { return *world_; }

    [[nodiscard]] EntityId id() const noexcept { return id_; }

    [[nodiscard]] bool isAlive() const noexcept { return world_->isAlive(id_); }

    void destroy() { world_->destroy(id_); }

private:
    World* world_;
    EntityId id_;
};

} // namespace basalt::server::entity::ecs
