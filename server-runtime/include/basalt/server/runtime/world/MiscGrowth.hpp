#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldClock.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {

namespace miscGrowth {

/// parity: `NyliumBlock.randomTick`.
void nylium(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::vector<BlockChange>& changes);

/// parity: `RedStoneOreBlock.randomTick`, which is only ever the light going out.
void redstoneOre(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                 std::vector<BlockChange>& changes);

/// parity: `RedStoneOreBlock.interact`.
[[nodiscard]] std::optional<BlockChange> lightRedstoneOre(WorldState& world, std::int32_t x, std::int32_t y,
                                                          std::int32_t z);

/// parity: `SweetBerryBushBlock.randomTick`: light nine above it and one roll in five.
void sweetBerries(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                  random::RandomSource& random, std::vector<BlockChange>& changes, bool hasSkylight = true);

/// parity: `BuddingAmethystBlock.randomTick`: one roll in five, one of the six faces.
void buddingAmethyst(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random,
                     std::vector<BlockChange>& changes);

/// parity: `TurtleEggBlock.randomTick`.
void turtleEgg(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
               random::RandomSource& random, std::vector<BlockChange>& changes);

/// parity: `DriedGhastBlock.randomTick` only asks to be looked at again in 5000 ticks.
void driedGhast(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId);

/// parity: `EyeblossomBlock.randomTick`.
void eyeblossom(WorldState& world, const WorldClock& clock, std::int32_t x, std::int32_t y, std::int32_t z,
                std::int32_t stateId, random::RandomSource& random, std::vector<BlockChange>& changes);

[[nodiscard]] bool isEyeblossom(std::int32_t stateId);

} // namespace miscGrowth

} // namespace basalt::server::runtime::world
