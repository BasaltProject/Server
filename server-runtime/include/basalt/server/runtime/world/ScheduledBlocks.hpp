#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::runtime::world {

/// What blocks do on a turn they asked for, rather than one the random tick handed them.
///
/// parity: the `tick` half of the block classes. Most of them are "if it cannot survive, break it",
/// which the neighbour update already does the moment the support goes, a tick earlier than vanilla
/// and to the same end. What is here is the rest: the ones that change into something else, count
/// their way out from a support, or melt.
namespace scheduledBlocks {

/// What a scheduled tick on each of these is filed under.
[[nodiscard]] const wire::common::Identifier& frostedIceTickType();

[[nodiscard]] const wire::common::Identifier& driedGhastTickType();

[[nodiscard]] const wire::common::Identifier& snifferEggTickType();

[[nodiscard]] const wire::common::Identifier& frogspawnTickType();

[[nodiscard]] const wire::common::Identifier& bubbleColumnTickType();

/// parity: `FarmlandBlock.turnToDirt` and `DirtPathBlock.tick`, both of which give way to plain dirt
/// when something solid lands on top rather than breaking.
[[nodiscard]] std::optional<std::int32_t> coveredSoil(WorldState& world, std::int32_t x, std::int32_t y,
                                                      std::int32_t z);

/// parity: `CoralBlock.tick` and the three `BaseCoralPlantTypeBlock` ones: coral out of water turns
/// into its dead twin instead of breaking.
[[nodiscard]] std::optional<std::int32_t> coral(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `ScaffoldingBlock.getDistance` and its `tick`: it counts out from whatever holds it, and
/// at seven it comes down.
[[nodiscard]] std::optional<std::int32_t> scaffolding(WorldState& world, std::int32_t x, std::int32_t y,
                                                      std::int32_t z);

/// Whether scaffolding here is holding on to anything at all.
[[nodiscard]] bool scaffoldingHasFallen(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `FrostedIceBlock.tick`: it thins a step at a time in the light and takes its neighbours
/// with it, and the ones that will not thin yet are asked again shortly.
void frostedIce(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random,
                std::vector<BlockChange>& changes, bool hasSkylight = true);

/// parity: `DriedGhastBlock.tick`: soaked it fills up, dry it dries out, and it keeps asking for the
/// next turn while there is anything left to change.
void driedGhast(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::vector<BlockChange>& changes);

/// parity: `SnifferEggBlock.onPlace`: three turns of a hatch that halves on moss, each one asked for
/// as the last one lands, plus its own random spread.
void scheduleHatch(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int64_t seed);

/// parity: `FrogspawnBlock.onPlace`, an hour or so away and never the same twice.
void scheduleFrogspawn(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int64_t seed);

/// parity: `FrogspawnBlock.tick`, which either breaks the spawn or hatches it away.
///
/// The tadpoles that swim out wait for entities: what this owns is the block going.
void frogspawn(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::vector<BlockChange>& changes);

/// parity: `SnifferEggBlock.tick`: it cracks twice and then goes.
///
/// The sniffer that walks out of it waits for entities, like a turtle egg's turtle.
void snifferEgg(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random,
                std::vector<BlockChange>& changes);

/// parity: `BubbleColumnBlock.updateColumn`: soul sand pushes a column up through the water above it
/// and magma drags one down, and the column climbs while there is water to fill.
///
/// What the column then does to whoever swims into it is `entityInside`, which waits for entities.
void bubbleColumn(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::vector<BlockChange>& changes);

} // namespace scheduledBlocks

} // namespace basalt::server::runtime::world
