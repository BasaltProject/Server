#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::runtime::world {

/// parity: `DragonEggBlock.teleport`.
namespace dragonEggs {

struct Moved {
    std::int32_t toX = 0;
    std::int32_t toY = 0;
    std::int32_t toZ = 0;
    std::int32_t stateId = 0;
    /// The data of level event 2015, the line the particles fly along.
    std::int32_t packedDifference = 0;
};

[[nodiscard]] std::optional<Moved> teleport(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                            std::int32_t minY, std::int32_t maxY, random::RandomSource& random,
                                            const std::function<bool(std::int32_t x, std::int32_t z)>& withinBorder);

} // namespace dragonEggs

} // namespace basalt::server::runtime::world
