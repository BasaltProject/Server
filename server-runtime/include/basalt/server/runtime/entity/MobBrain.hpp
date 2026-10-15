#pragma once

#include <basalt/server/random/RandomSource.hpp>

#include <array>
#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {
class WorldState;
}

namespace basalt::server::runtime::entity {

class ServerEntity;

namespace mobBrain {

inline constexpr std::int32_t kStrollInterval = 120;

inline constexpr std::int32_t kStrollRange = 10;
inline constexpr std::int32_t kStrollHeight = 7;

void tick(world::WorldState& world, ServerEntity& mob, random::RandomSource& random, double speed,
          double modifier = 1.0);

bool walk(world::WorldState& world, ServerEntity& mob, double speed, double modifier = 1.0);

bool pathTo(world::WorldState& world, ServerEntity& mob, double x, double y, double z);

[[nodiscard]] std::optional<std::array<double, 3>> strollTarget(world::WorldState& world, const ServerEntity& mob,
                                                                random::RandomSource& random,
                                                                std::int32_t range = kStrollRange,
                                                                std::int32_t height = kStrollHeight);

[[nodiscard]] double distanceTo(const ServerEntity& mob, double x, double y, double z);

} // namespace mobBrain

} // namespace basalt::server::runtime::entity
