#pragma once

#include <basalt/server/random/RandomSource.hpp>
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

/// Lets every reactive block around a change look again. Answers what moved, so the caller can tell
/// the clients.
///
/// `itself` says whether the square that changed is asked too. parity: `Level.updateNeighborsAt`
/// tells the six around a square and never the square itself, and a redstone lamp has no `onPlace`:
/// one put down by command next to a lit torch stays dark until something pokes it. Only a diode
/// asks the square in front of it directly, through `neighborChanged`.
/// `gameTime` is the world's clock, which the game events a reactor raises are stamped with: a door
/// swinging is a `BLOCK_OPEN` and a sculk sensor hears it. It is a parameter and not a default so
/// that a caller cannot forget it and leave a vibration stamped with turn zero.
/// `tntExplodes` is the game rule of the same name: with it off `TntBlock.prime` answers false and the
/// caller leaves the block alone, so a signal neither lights the tnt nor takes it away.
[[nodiscard]] std::vector<BlockChange> update(WorldState& world, std::string_view worldName, std::int32_t x,
                                              std::int32_t y, std::int32_t z, std::int64_t gameTime,
                                              random::RandomSource& random, bool tntExplodes, bool itself = true);

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
