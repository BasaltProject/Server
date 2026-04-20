#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/MobSpawnForest.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>
#include <string_view>

namespace basalt::server::runtime::world {

class LoadedWorld;

/// Callbacks because the tags come from the operator's jar and the players from the registry; unset answers no.
struct SpawnRuleFacts {
    std::function<bool(std::string_view tag, const wire::common::Identifier& block)> blockTagged;
    std::function<bool(std::string_view tag, const wire::common::Identifier& biome)> biomeTagged;
    /// parity: `getNearestPlayer(x, y, z, range, true) != null`.
    std::function<bool(const LoadedWorld& world, double x, double y, double z, double range)> playerWithin;
};

/// parity: `SpawnPlacements.checkSpawnRules`.
namespace spawnPlacements {

/// parity: `EntitySpawnReason.isSpawner`.
[[nodiscard]] bool isSpawner(MobSpawnReason reason) noexcept;

/// parity: `EntitySpawnReason.ignoresLightRequirements`.
[[nodiscard]] bool ignoresLightRequirements(MobSpawnReason reason) noexcept;

/// parity: `EntityType.isAllowedInPeaceful`.
[[nodiscard]] bool isAllowedInPeaceful(const wire::common::Identifier& type);

/// parity: `SpawnPlacements.checkSpawnRules`.
[[nodiscard]] bool checkSpawnRules(LoadedWorld& world, const wire::common::Identifier& type, MobSpawnReason reason,
                                   std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random,
                                   const SpawnRuleFacts& facts);

/// parity: `Monster.isDarkEnoughToSpawn`.
[[nodiscard]] bool isDarkEnoughToSpawn(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                       random::RandomSource& random);

/// parity: `LevelReader.getMaxLocalRawBrightness`, darkened by the world's sky.
[[nodiscard]] std::int32_t maxLocalRawBrightness(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `LevelReader.getLightLevelDependentMagicValue`.
[[nodiscard]] float lightLevelDependentMagicValue(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `LevelReader.getEffectiveSkyBrightness`.
[[nodiscard]] std::int32_t effectiveSkyBrightness(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `LevelReader.getBrightness(LightLayer.BLOCK, pos)`.
[[nodiscard]] std::int32_t blockBrightness(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

} // namespace spawnPlacements

} // namespace basalt::server::runtime::world
