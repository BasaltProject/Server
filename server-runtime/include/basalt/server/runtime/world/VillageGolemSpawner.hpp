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

/// The agreement itself is the villager brain's.
namespace villageGolemSpawner {

/// parity: `SpawnUtil.LEGACY_IRON_GOLEM`.
[[nodiscard]] std::optional<std::array<std::int32_t, 3>>
findSpawnPosition(LoadedWorld& world, const entity::ServerEntity& villager, random::RandomSource& random);

} // namespace villageGolemSpawner

} // namespace basalt::server::runtime::world
