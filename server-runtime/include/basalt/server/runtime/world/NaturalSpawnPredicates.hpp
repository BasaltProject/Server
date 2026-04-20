#pragma once

#include <cstdint>
#include <functional>
#include <string_view>

namespace basalt::server::runtime::world {

/// parity: `SpawnPlacements`.
namespace naturalSpawnPredicates {

/// parity: the `NO_RESTRICTIONS` registrations that appear in a natural CREATURE biome list.
[[nodiscard]] bool usesGroundPlacement(std::string_view type);

/// parity: the block and biome tests of the animal spawn rule that was registered for `type`.
[[nodiscard]] bool allows(std::string_view type, std::string_view ground, bool groundIsLeaves, std::string_view biome,
                          std::int32_t y, std::int32_t seaLevel);

/// parity: the `Monster.checkSurfaceMonstersSpawnRules` registrations that appear in a biome list.
[[nodiscard]] bool isSurfaceMonster(std::string_view type);

/// parity: `SpawnPlacementTypes.IN_WATER`.
[[nodiscard]] bool waterPlacementAllows(bool atHasWater, bool aboveConductsRedstone);

/// parity: a glow squid also wants the block itself to be water, not merely to hold some.
[[nodiscard]] bool glowSquidAllows(std::int32_t y, std::int32_t seaLevel, std::int32_t rawBrightness,
                                   bool atIsWaterBlock);

/// parity: `Guardian.checkGuardianSpawnRules`.
[[nodiscard]] bool guardianAllows(std::int32_t oneInTwentyRoll, std::string_view difficulty, bool atHasWater,
                                  bool belowHasWater, const std::function<bool()>& canSeeSkyFromBelowWater);

} // namespace naturalSpawnPredicates

} // namespace basalt::server::runtime::world
