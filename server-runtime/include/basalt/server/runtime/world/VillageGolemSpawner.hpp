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

/// Villager social agreement and `SpawnUtil.LEGACY_IRON_GOLEM` placement from Minecraft 26.2.
///
/// A golem is not spawned by a village: it is spawned by villagers agreeing that they want one,
/// which is why panic makes them agree faster and why one that has just seen a golem does not.
namespace villageGolemSpawner {

/// Answers whether the golem was really made, since the caller may refuse it.
using Spawn = std::function<bool(const std::array<std::int32_t, 3>&)>;

void tick(LoadedWorld& world, entity::ServerEntity& villager, const std::vector<entity::ServerEntity*>& living,
          random::RandomSource& random, const Spawn& spawn);

/// parity: a villager wants a golem when it slept within the day and has not seen one since.
[[nodiscard]] bool wantsGolem(const entity::ServerEntity& villager, std::int64_t gameTime);

/// parity: `SpawnUtil.LEGACY_IRON_GOLEM`, ten probes around the villager, each walking down a
/// column for the first block that will hold a golem up.
[[nodiscard]] std::optional<std::array<std::int32_t, 3>>
findSpawnPosition(LoadedWorld& world, const entity::ServerEntity& villager, random::RandomSource& random);

} // namespace villageGolemSpawner

} // namespace basalt::server::runtime::world
