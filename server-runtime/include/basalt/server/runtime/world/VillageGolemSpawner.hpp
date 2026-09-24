#pragma once

#include <basalt/server/random/RandomSource.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::world {

class LoadedWorld;

/// Where the iron golem villagers agreed on stands. The agreement itself is the villager brain's.
namespace villageGolemSpawner {

/// parity: `SpawnUtil.LEGACY_IRON_GOLEM`, ten probes around the villager, each walking down a
/// column for the first block that will hold a golem up.
[[nodiscard]] std::optional<std::array<std::int32_t, 3>>
findSpawnPosition(LoadedWorld& world, const entity::ServerEntity& villager, random::RandomSource& random);

} // namespace villageGolemSpawner

} // namespace basalt::server::runtime::world
