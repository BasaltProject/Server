#pragma once

#include <basalt/server/random/RandomSource.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::world {
class WorldState;
}

namespace basalt::server::runtime::entity::ai {

/// parity: `RandomPos`, `DefaultRandomPos` and `LandRandomPos`, draw for draw.
namespace randomPos {

using Spot = std::array<double, 3>;

/// parity: `PathfinderMob.getWalkTargetValue`.
using Weight = std::function<double(std::int32_t x, std::int32_t y, std::int32_t z)>;

/// parity: `DefaultRandomPos.getPos`.
[[nodiscard]] std::optional<Spot> defaultPos(world::WorldState& world, const ServerEntity& mob,
                                             random::RandomSource& random, std::int32_t horizontal,
                                             std::int32_t vertical);

/// parity: `DefaultRandomPos.getPosTowards`, within an angle of the way to `towards`.
[[nodiscard]] std::optional<Spot> defaultPosTowards(world::WorldState& world, const ServerEntity& mob,
                                                    random::RandomSource& random, std::int32_t horizontal,
                                                    std::int32_t vertical, const Spot& towards, double maxRadians);

/// parity: `DefaultRandomPos.getPosAway`.
[[nodiscard]] std::optional<Spot> defaultPosAway(world::WorldState& world, const ServerEntity& mob,
                                                 random::RandomSource& random, std::int32_t horizontal,
                                                 std::int32_t vertical, const Spot& away);

/// parity: `LandRandomPos.getPos`, lifted out of the ground and never into water.
[[nodiscard]] std::optional<Spot> landPos(world::WorldState& world, const ServerEntity& mob,
                                          random::RandomSource& random, std::int32_t horizontal, std::int32_t vertical,
                                          const Weight& weight = {});

/// parity: `LandRandomPos.getPosTowards`.
[[nodiscard]] std::optional<Spot> landPosTowards(world::WorldState& world, const ServerEntity& mob,
                                                 random::RandomSource& random, std::int32_t horizontal,
                                                 std::int32_t vertical, const Spot& towards);

/// parity: `LandRandomPos.getPosAway`.
[[nodiscard]] std::optional<Spot> landPosAway(world::WorldState& world, const ServerEntity& mob,
                                              random::RandomSource& random, std::int32_t horizontal,
                                              std::int32_t vertical, const Spot& away);

} // namespace randomPos

} // namespace basalt::server::runtime::entity::ai
