#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/MobSpawnFinalization.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::world {

class LoadedWorld;

/// 26.2 thunder-created skeleton horse and its four-rider activation forest.
namespace skeletonHorseTrap {

inline constexpr double kPlayerRange = 10.0;

inline constexpr std::int32_t kMaxLife = 18000;

inline constexpr double kSpawnChanceScale = 0.01;

inline constexpr std::int32_t kActivationInvulnerableTicks = 60;

/// parity: `ServerLevel.tickThunder`.
[[nodiscard]] bool shouldCreateFromThunder(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                           random::RandomSource& random);

[[nodiscard]] MobSpawnForest activate(LoadedWorld& world, entity::ServerEntity& horse,
                                      random::RandomSource& levelRandom, random::RandomSource& horseRandom,
                                      const std::function<random::RandomSource()>& entityRandomFactory,
                                      const SpawnEquipmentFinalizer& equipment);

[[nodiscard]] MobSpawnForest buildActivationForest(entity::ServerEntity& horse, random::RandomSource& levelRandom,
                                                   random::RandomSource& horseRandom,
                                                   const std::function<random::RandomSource()>& entityRandomFactory,
                                                   const SpawnEquipmentFinalizer& equipment, float specialMultiplier,
                                                   const std::optional<wire::common::Identifier>& biome,
                                                   std::int64_t gameTime, std::string_view difficulty);

/// parity: the post-increment lifetime from `SkeletonHorse.aiStep`.
void tickLifetime(entity::ServerEntity& horse);

} // namespace skeletonHorseTrap

} // namespace basalt::server::runtime::world
