#pragma once

#include <basalt/server/entity/ecs/EntityId.hpp>

#include <cstdint>
#include <deque>
#include <vector>

namespace basalt::server::entity::ecs {
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
