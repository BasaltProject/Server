#pragma once

#include <basalt/server/world/loot/LootContext.hpp>

#include <cstdint>
#include <functional>

namespace basalt::server::random {
class RandomSource;
}

namespace basalt::server::runtime::world {

class WorldState;

/// What a container spills when it goes.
namespace containerDrops {

/// parity: `Containers.dropItemStack`: one point in the square, then the stack in handfuls of ten to
/// thirty, each thrown its own way.
void dropItemStack(WorldState& world, double x, double y, double z, server::world::loot::ItemStack stack,
                   random::RandomSource& random, const std::function<std::int32_t()>& allocateEntityId);

} // namespace containerDrops

} // namespace basalt::server::runtime::world
