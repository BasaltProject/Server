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

/// parity: `EyeblossomBlock.randomTick` over the `gameplay/eyeblossom_open` attribute, whose only
/// source in vanilla's data is the `day` timeline: true from tick 12600, false from 23401.
///
/// The attribute system itself is not modelled, but this attribute has exactly one track and it is
/// one this build already keeps: the world clock.
void eyeblossom(WorldState& world, const WorldClock& clock, std::int32_t x, std::int32_t y, std::int32_t z,
                std::int32_t stateId, std::vector<BlockChange>& changes);

} // namespace miscGrowth

} // namespace basalt::server::runtime::world
