#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldClock.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {

/// Single-function block families with no private helpers of their own, kept together to avoid a
/// dozen near-empty files. Each still says which vanilla class it came from.
namespace miscGrowth {

/// parity: `NyliumBlock.randomTick`: nylium under something opaque goes back to netherrack.
void nylium(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::vector<BlockChange>& changes);

/// parity: `RedStoneOreBlock.randomTick`, which is only ever the light going out.
void redstoneOre(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                 std::vector<BlockChange>& changes);

/// parity: `RedStoneOreBlock.interact`, the other half: walking over it, hitting it or using it puts
/// the light on. Answers the change, or nothing when there is no ore there or it is already lit.
[[nodiscard]] std::optional<BlockChange> lightRedstoneOre(WorldState& world, std::int32_t x, std::int32_t y,
                                                          std::int32_t z);

/// parity: `SweetBerryBushBlock.randomTick`: light nine above it and one roll in five.
void sweetBerries(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                  random::RandomSource& random, std::vector<BlockChange>& changes, bool hasSkylight = true);

/// parity: `BuddingAmethystBlock.randomTick`: one roll in five, one of the six faces.
void buddingAmethyst(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random,
                     std::vector<BlockChange>& changes);

/// parity: `TurtleEggBlock.randomTick`, minus the turtles: the egg cracks its two stages and then
/// goes, and hatching a turtle waits for entities. The odds are the default of the
/// `turtle_egg_hatch_chance` attribute, which is what an unmodified world gives every block.
void turtleEgg(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
               random::RandomSource& random, std::vector<BlockChange>& changes);

/// parity: `DriedGhastBlock.randomTick`, which does nothing itself but ask to be looked at again in
/// five thousand ticks; the drying and soaking happen on that scheduled turn.
void driedGhast(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId);

/// parity: `EyeblossomBlock.randomTick` over `gameplay/eyeblossom_open`, whose only source is the
/// `day` timeline (open from 12600 to 23400). Also run from a booked turn (`EyeblossomBlock.tick`).
void eyeblossom(WorldState& world, const WorldClock& clock, std::int32_t x, std::int32_t y, std::int32_t z,
                std::int32_t stateId, random::RandomSource& random, std::vector<BlockChange>& changes);

/// Whether a state is one of the four eyeblossoms, the two in a pot included.
[[nodiscard]] bool isEyeblossom(std::int32_t stateId);

} // namespace miscGrowth

} // namespace basalt::server::runtime::world
