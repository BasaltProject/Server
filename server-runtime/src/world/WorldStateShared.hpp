#pragma once

#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/world/EndCrystals.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/server/world/Chunk.hpp>

namespace basalt::server::runtime::world {

[[nodiscard]] inline bool becomesLive(const server::world::ChunkGeneratedEntity& entity) {
    return entity::mobs::isMob(entity.type) || endCrystals::isCrystal(entity.type);
}

} // namespace basalt::server::runtime::world
