#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/MobSpawnForest.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>
#include <string_view>

namespace basalt::server::runtime::world {

class LoadedWorld;

/// What a spawn rule asks of the world that the world itself does not keep. Callbacks, because the
/// tags come out of the operator's jar and the players out of the registry; unset answers no.
struct SpawnRuleFacts {
    /// Whether a block is in a block tag, by the tag's full name.
    std::function<bool(std::string_view tag, const wire::common::Identifier& block)> blockTagged;
    /// Whether a biome is in a biome tag, by the tag's full name.
    std::function<bool(std::string_view tag, const wire::common::Identifier& biome)> biomeTagged;
    /// parity: `getNearestPlayer(x, y, z, range, true) != null`, anyone not in creative or spectator.
    std::function<bool(const LoadedWorld& world, double x, double y, double z, double range)> playerWithin;
};

/// The rule a type's spawn placement names, asked for one reason at one block.
///
/// parity: `SpawnPlacements.checkSpawnRules` and every predicate its table registers. Unlike the
/// natural spawner's own shortcut, this answers for any `EntitySpawnReason`, which is what a monster
/// spawner needs: most rules relax for `isSpawner`, and only a trial spawner ignores the light.
namespace spawnPlacements {

/// parity: `EntitySpawnReason.isSpawner`.
[[nodiscard]] bool isSpawner(MobSpawnReason reason) noexcept;

/// parity: `EntitySpawnReason.ignoresLightRequirements`.
[[nodiscard]] bool ignoresLightRequirements(MobSpawnReason reason) noexcept;

/// parity: `EntityType.isAllowedInPeaceful`, off for the types built with `notInPeaceful`.
[[nodiscard]] bool isAllowedInPeaceful(const wire::common::Identifier& type);

/// parity: `SpawnPlacements.checkSpawnRules`: peaceful first, then the registered predicate, and
/// true for a type with none. Draws from `random` exactly where the predicate does.
[[nodiscard]] bool checkSpawnRules(LoadedWorld& world, const wire::common::Identifier& type, MobSpawnReason reason,
                                   std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random,
                                   const SpawnRuleFacts& facts);

/// parity: `Monster.isDarkEnoughToSpawn`, with the dimension's own monster settings and the
/// thunder's darker sky.
[[nodiscard]] bool isDarkEnoughToSpawn(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                       random::RandomSource& random);

/// parity: `LevelReader.getMaxLocalRawBrightness`, darkened by the world's sky.
[[nodiscard]] std::int32_t maxLocalRawBrightness(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `LevelReader.getLightLevelDependentMagicValue`, what a mob's walk target weighs light by.
[[nodiscard]] float lightLevelDependentMagicValue(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `LevelReader.getEffectiveSkyBrightness`, the sky light less the world's darkening.
[[nodiscard]] std::int32_t effectiveSkyBrightness(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `LevelReader.getBrightness(LightLayer.BLOCK, pos)`.
[[nodiscard]] std::int32_t blockBrightness(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

} // namespace spawnPlacements

} // namespace basalt::server::runtime::world
