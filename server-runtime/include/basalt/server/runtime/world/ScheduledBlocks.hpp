#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
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

[[nodiscard]] const wire::common::Identifier& sugarCaneTickType();

/// parity: `FarmlandBlock.updateShape` and `DirtPathBlock`'s, which book a turn one tick out for the
/// square **above** rather than giving way on the spot.
void scheduleCoveredSoil(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `BaseCoralPlantTypeBlock.tryScheduleDieTick`: coral out of water keeps its colour for
/// sixty ticks and a bit before its turn turns it white.
void scheduleCoralDeath(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int64_t seed);

/// parity: `FarmlandBlock.turnToDirt` and `DirtPathBlock.tick`, both of which give way to plain dirt
/// when something solid lands on top rather than breaking.
[[nodiscard]] std::optional<std::int32_t> coveredSoil(WorldState& world, std::int32_t x, std::int32_t y,
                                                      std::int32_t z);

/// parity: `CoralBlock.tick` and the three `BaseCoralPlantTypeBlock` ones: coral out of water turns
/// into its dead twin instead of breaking.
[[nodiscard]] std::optional<std::int32_t> coral(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `ScaffoldingBlock.updateShape`, which rewrites nothing and books a turn one tick out for
/// **any** neighbour that moved. The distance, the `bottom` and the fall all wait for that turn.
void scheduleScaffolding(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// What `ScaffoldingBlock.tick` decides: nothing, a new shape, or that it is time to come down.
struct ScaffoldingTurn {
    /// The state it became, when only the distance or the `bottom` moved.
    std::optional<std::int32_t> reshaped;
    /// Set when the piece has to go: it was already out of reach, so it falls as an entity.
    bool falls = false;
    /// Set when the piece has to go and it was still in reach a moment ago, so it just breaks.
    bool breaks = false;
};

/// parity: `ScaffoldingBlock.tick`, the turn the shape update booked.
[[nodiscard]] ScaffoldingTurn scaffoldingTick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `ScaffoldingBlock.getDistance` and its `tick`: it counts out from whatever holds it, and
/// at seven it comes down.
[[nodiscard]] std::optional<std::int32_t> scaffolding(WorldState& world, std::int32_t x, std::int32_t y,
                                                      std::int32_t z);

/// Whether scaffolding here is holding on to anything at all.
[[nodiscard]] bool scaffoldingHasFallen(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `FrostedIceBlock.tick`: it thins a step at a time in the light and takes its neighbours
/// with it, and the ones that will not thin yet are asked again shortly.
/// What a piece of frosted ice does when the neighbour that changed is more frosted ice: if it has
/// fewer than two of them around, it melts whole.
///
/// parity: `FrostedIceBlock.neighborChanged`.
[[nodiscard]] std::optional<BlockChange> frostedIceHeard(WorldState& world, std::int32_t x, std::int32_t y,
                                                         std::int32_t z);

/// The whole trail: every piece that melts tells its own six squares, and they melt too.
void frostedIceChain(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                     std::vector<BlockChange>& changes);

void frostedIce(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random,
                std::vector<BlockChange>& changes, bool hasSkylight = true);

/// parity: `DriedGhastBlock.tick`: soaked it fills up, dry it dries out, and it keeps asking for the
/// next turn while there is anything left to change.
void driedGhast(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int64_t gameTime,
                std::vector<BlockChange>& changes);

/// parity: `SnifferEggBlock.onPlace`: three turns of a hatch that halves on moss, each one asked for
/// as the last one lands, plus its own random spread.
void scheduleHatch(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int64_t seed);

/// parity: `FrostedIceBlock.onPlace`, which is one line: every piece of frosted ice that lands books
/// its own melt between sixty and a hundred and twenty ticks out. Without it a trail of it sat there
/// for ever, and the jar's own `block_ticks` had a turn on every square where ours had none.
void scheduleFrostedIce(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                        random::RandomSource& random);

/// parity: `FrogspawnBlock.onPlace`, an hour or so away and never the same twice.
void scheduleFrogspawn(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int64_t seed);

/// Whether a state is a pale oak log, which only the tag can answer.
using IsPaleOakLog = std::function<bool(std::int32_t stateId)>;

/// parity: `CreakingHeartBlock.updateShape`, which books a turn for any neighbour that moved.
void scheduleCreakingHeart(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `CreakingHeartBlock.tick` over `updateState`: an uprooted heart with a pale oak log at
/// both ends of its own axis wakes up, awake by night and dormant by day.
[[nodiscard]] std::optional<std::int32_t> creakingHeartTick(WorldState& world, std::int32_t x, std::int32_t y,
                                                            std::int32_t z, const IsPaleOakLog& isPaleOakLog,
                                                            bool creakingActive);

/// Whether the creaking is awake, which is not the same window as being able to sleep. parity:
/// `CREAKING_ACTIVE` runs 12600 to 23401 while a bed's window is 12542 to 23459, fifty-eight ticks
/// wider at each end, so reading one for the other put the heart wrong on 116 ticks a day.
[[nodiscard]] bool creakingIsActive(std::int64_t timeOfDay, bool hasDayTimeline = true);

/// What `SpeleothemBlock.tick` decides: nothing, a stalagmite broken where it stands, or a whole
/// stalactite coming down as entities.
struct SpeleothemTurn {
    bool destroys = false;
    bool falls = false;
};

/// parity: `SpeleothemBlock.updateShape`, which books two ticks for a stalactite and one for a
/// stalagmite, and leaves a stalactite alone once its turn is already booked.
void scheduleSpeleothem(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, bool supported);

/// Whether a stalactite hangs here, and whether that piece is its tip.
///
/// parity: `SpeleothemBlock.spawnFallingStalactite` walks down from the piece that lost its anchor
/// and turns every one into an entity, stopping after the tip.
[[nodiscard]] bool isStalactiteAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

[[nodiscard]] bool isStalactiteTipAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `SpeleothemBlock.tick`, the turn it booked.
[[nodiscard]] SpeleothemTurn speleothemTick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                            bool supported);

/// Whether this block answers a neighbour that moved by booking a turn instead of breaking. parity:
/// `SugarCaneBlock.updateShape` and the cactus and dripleaf stem are the same three lines: no
/// `canSurvive` means a turn one tick out, and the turn is what breaks the block, so a tower comes
/// apart one square per tick.
[[nodiscard]] bool booksItsOwnBreak(std::string_view path);

/// Books that turn. `supported` is the answer `canSurvive` would give.
void scheduleBreakIfLoose(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, bool supported);

/// The turn itself: it breaks only if the block still cannot stand.
void breakIfStillLoose(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, bool supported,
                       std::vector<BlockChange>& changes);

/// parity: `FrogspawnBlock.tick`, which either breaks the spawn or hatches it away.
///
/// The tadpoles that swim out wait for entities: what this owns is the block going.
void frogspawn(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::vector<BlockChange>& changes);

/// parity: `SnifferEggBlock.tick`: it cracks twice and then goes.
///
/// The sniffer that walks out of it waits for entities, like a turtle egg's turtle.
void snifferEgg(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random,
                std::vector<BlockChange>& changes);

/// parity: `LiquidBlock.tryScheduleBubbleBlockColumn`, twenty ticks booked on the water over soul
/// sand or magma, and `BubbleColumnBlock.updateShape`'s five for a column that is already there.
///
/// That second one is **not** booked on every shape update: only when the column has lost its
/// footing, when the news came from below, or when it came from above and the square up there is
/// occupiable but not already a column. `towards` is what the news came from, and nothing means the
/// arriving block reshaping itself, which walks all six sides and so always books.
void scheduleBubbleColumn(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                          std::optional<registry::BlockFace> towards);

/// parity: `BubbleColumnBlock.updateColumn`: soul sand pushes a column up through the water above it
/// and magma drags one down, and the column climbs while there is water to fill.
///
/// What the column then does to whoever swims into it is `entityInside`, which waits for entities.
///
/// `written` runs right after each square lands, because the jar's `setBlock(..., 2)` does its shape
/// walk there and not once the whole column is up: the square above is still water when the one below
/// is written, so it never hears a bubble column arriving underneath it.
void bubbleColumn(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::vector<BlockChange>& changes,
                  const std::function<void(std::int32_t, std::int32_t, std::int32_t)>& written = {});

} // namespace scheduledBlocks

} // namespace basalt::server::runtime::world
