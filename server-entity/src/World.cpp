#include <basalt/server/entity/ecs/World.hpp>

#include <algorithm>
#include <cstdint>

namespace basalt::server::entity::ecs {

EntityId World::create() {
    std::int32_t index = 0;
    if (!freeIndices_.empty()) {
        index = freeIndices_.front();
        freeIndices_.pop_front();
    } else {
        index = nextIndex_;
        ensureCapacity(index + 1);
        ++nextIndex_;
    }
    alive_[static_cast<std::size_t>(index)] = true;
    return EntityId::of(index, generations_[static_cast<std::size_t>(index)]);
}

void World::destroy(EntityId id) {
    if (!isAlive(id)) {
        return;
    }
    const std::size_t index = static_cast<std::size_t>(id.index());
    alive_[index] = false;
    ++generations_[index];
    freeIndices_.push_back(id.index());
}

bool World::isAlive(EntityId id) const noexcept {
    const std::int32_t index = id.index();
    if (index < 0 || static_cast<std::size_t>(index) >= alive_.size()) {
        return false;
    }
    return alive_[static_cast<std::size_t>(index)] && generations_[static_cast<std::size_t>(index)] == id.generation();
}

void World::ensureCapacity(std::int32_t minSize) {
    if (static_cast<std::size_t>(minSize) <= generations_.size()) {
        return;
    }
    const std::size_t newSize = std::max(static_cast<std::size_t>(minSize), generations_.size() * 2 + 16);
    generations_.resize(newSize, 0);
    alive_.resize(newSize, false);
}

} // namespace basalt::server::entity::ecs
