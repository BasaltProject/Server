#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// The blocks that do something when the power around them changes: doors, lamps, bulbs and the
/// rest.
///
/// parity: each block's own `neighborChanged`, which vanilla runs the moment a neighbour moves
/// rather than on a scheduled tick. They all ask the same question first (`hasNeighborSignal`) and
/// differ only in what they do with the answer.
///
/// A redstone lamp is the exception worth naming: it lights at once and takes four ticks to go out,
/// which is what stops it flickering.
namespace redstoneReactors {

/// parity: `RedstoneLampBlock`'s own four ticks before it goes dark.
inline constexpr std::int32_t kLampOffDelay = 4;

[[nodiscard]] const wire::common::Identifier& lamp();

/// Lets every reactive block around a change look again, answering what moved. parity:
/// `Level.updateNeighborsAt` tells the six around a square and never the square itself, so `itself`
/// exists for the diode, which asks the square in front directly. `gameTime` stamps the game events
/// raised, a parameter so a caller cannot leave a vibration at turn zero; `tntExplodes` is the game
/// rule, and with it off a signal leaves the tnt alone.
[[nodiscard]] std::vector<BlockChange> update(WorldState& world, std::string_view worldName, std::int32_t x,
                                              std::int32_t y, std::int32_t z, std::int64_t gameTime,
                                              random::RandomSource& random, bool tntExplodes, bool itself = true,
                                              std::optional<std::int32_t> told = std::nullopt);

/// The half of a write only the block itself can answer, which is its `onPlace`. parity:
/// `Level.updateNeighborsAt` never tells the square that changed, so a block landing on a powered
/// square reacts only if its own `onPlace` reads the power. Four do: tnt primes, a hopper comes out
/// disabled, a copper bulb lights and a shelf joins its chain. A lamp, a door or a bell have none.
/// parity: every block that reads the signal in its `onPlace` gates it on
/// `!oldState.is(state.getBlock())`, so a write that only moves a property reads nothing.
[[nodiscard]] std::vector<BlockChange> placed(WorldState& world, std::string_view worldName, std::int32_t x,
                                              std::int32_t y, std::int32_t z, std::int64_t gameTime,
                                              random::RandomSource& random, bool tntExplodes,
                                              std::optional<std::int32_t> removed = std::nullopt);

/// The reactors on and around the square a diode feeds, less the way back to the diode.
///
/// parity: `DiodeBlock.updateNeighborsInFront`: a `neighborChanged` on that square and an
/// `updateNeighborsAtExceptFromFacing` on it, which is its six less one.
[[nodiscard]] std::vector<BlockChange> updateInFront(WorldState& world, std::string_view worldName, std::int32_t x,
                                                     std::int32_t y, std::int32_t z, server::registry::BlockFace back,
                                                     std::int64_t gameTime, random::RandomSource& random,
                                                     bool tntExplodes);

/// The same for a piece of dust whose strength moved, whose reach is two squares.
///
/// parity: `DefaultRedstoneWireEvaluator.updatePowerStrength` runs `updateNeighborsAt` on the dust
/// **and on each of its six squares**. A lamp stuck to a block that the dust powers from above is two
/// steps away, and with one it never heard: measured in the bench circuit `lamparalejos`.
[[nodiscard]] std::vector<BlockChange> updateAroundWire(WorldState& world, std::string_view worldName, std::int32_t x,
                                                        std::int32_t y, std::int32_t z, std::int64_t gameTime,
                                                        random::RandomSource& random, bool tntExplodes);

/// The scheduled half of the lamp: it only goes dark if the signal is still gone.
[[nodiscard]] std::optional<BlockChange> lampTick(WorldState& world, std::string_view worldName, std::int32_t x,
                                                  std::int32_t y, std::int32_t z);

} // namespace redstoneReactors

} // namespace basalt::server::runtime::world
