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

/// Minecraft 26.2's thunder-created skeleton horse and its four-rider activation forest.
namespace skeletonHorseTrap {

/// How close a player has to be for the trap to go off.
inline constexpr double kPlayerRange = 10.0;

/// parity: `SkeletonHorse.trapTime`'s own limit, fifteen minutes before an untouched trap goes.
inline constexpr std::int32_t kMaxLife = 18000;

inline constexpr double kSpawnChanceScale = 0.01;

/// parity: the sixty ticks the freshly activated horses cannot be hurt for.
inline constexpr std::int32_t kActivationInvulnerableTicks = 60;

/// parity: `ServerLevel.tickThunder`: rain, biome and sky gate, then the local-difficulty roll, then
/// the lightning-rod exclusion. The roll comes before the rod test, so a protected strike still
/// consumes it.
[[nodiscard]] bool shouldCreateFromThunder(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                           random::RandomSource& random);

/// Mutates the original trap exactly once and builds its original rider plus three horse pairs.
[[nodiscard]] MobSpawnForest activate(LoadedWorld& world, entity::ServerEntity& horse,
                                      random::RandomSource& levelRandom, random::RandomSource& horseRandom,
                                      const std::function<random::RandomSource()>& entityRandomFactory,
                                      const SpawnEquipmentFinalizer& equipment);

/// The topology and finalization half on its own, without the world's local-difficulty lookup.
[[nodiscard]] MobSpawnForest buildActivationForest(entity::ServerEntity& horse, random::RandomSource& levelRandom,
                                                   random::RandomSource& horseRandom,
                                                   const std::function<random::RandomSource()>& entityRandomFactory,
                                                   const SpawnEquipmentFinalizer& equipment, float specialMultiplier,
                                                   const std::optional<wire::common::Identifier>& biome,
                                                   std::int64_t gameTime, std::string_view difficulty);

/// parity: the post-increment lifetime from `SkeletonHorse.aiStep`; activation has already cleared
/// the flag by the time this could remove one.
void tickLifetime(entity::ServerEntity& horse);

} // namespace skeletonHorseTrap

} // namespace basalt::server::runtime::world
