#pragma once

#include <basalt/server/world/loot/LootContext.hpp>

#include <cstdint>
#include <functional>

namespace basalt::server::random {
class RandomSource;
}

namespace basalt::server::runtime::world {

class WorldState;

namespace containerDrops {

/// parity: `Containers.dropItemStack`.
void dropItemStack(WorldState& world, double x, double y, double z, server::world::loot::ItemStack stack,
                   random::RandomSource& random, const std::function<std::int32_t()>& allocateEntityId);

} // namespace containerDrops

} // namespace basalt::server::runtime::world
