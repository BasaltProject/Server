#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {

/// parity: `TurtleEggBlock.destroyEgg` and `decreaseEggs`.
namespace turtleEggs {

/// One in `randomness` chances to lose an egg, or the whole block when it was the last one.
[[nodiscard]] std::optional<BlockChange> trample(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                 std::int32_t randomness, random::RandomSource& random);

} // namespace turtleEggs

} // namespace basalt::server::runtime::world
