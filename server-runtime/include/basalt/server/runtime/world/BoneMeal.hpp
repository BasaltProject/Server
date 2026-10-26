#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {

namespace boneMeal {

[[nodiscard]] std::optional<BlockChange> apply(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                               random::RandomSource& random);

[[nodiscard]] bool grows(std::int32_t stateId);

/// The same, plus the plants a world's gameplay rules let bone meal reach.
[[nodiscard]] bool grows(const WorldState& world, std::int32_t stateId);

} // namespace boneMeal

} // namespace basalt::server::runtime::world
