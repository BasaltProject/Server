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

/// The two things a reactor needs that live in files above this one.
///
/// Callbacks rather than calls: the sound a door makes belongs to the code that also plays it for a
/// hand, and a shelf's chain belongs to the shelf. Left empty, a door still swings and a shelf is
/// simply not one of the blocks that react.
struct Hooks {
    /// parity: `DoorBlock.playSound` and its two cousins: the same sound a hand makes.
    std::function<std::optional<std::string>(std::int32_t stateId, bool open)> openSound;

    /// parity: `ShelfBlock.neighborChanged`, which joins or breaks its chain with the signal.
    std::function<void(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                       std::vector<BlockChange>& changes)>
        reactShelf;
};

/// Lets every reactive block around a change look again. Answers what moved, so the caller can tell
/// the clients.
///
/// `itself` says whether the square that changed is asked too. parity: `Level.updateNeighborsAt`
/// tells the six around a square and never the square itself, and a redstone lamp has no `onPlace`:
/// one put down by command next to a lit torch stays dark until something pokes it. Only a diode
/// asks the square in front of it directly, through `neighborChanged`.
[[nodiscard]] std::vector<BlockChange> update(WorldState& world, std::string_view worldName, std::int32_t x,
                                              std::int32_t y, std::int32_t z, random::RandomSource& random,
                                              const Hooks& hooks = {}, bool itself = true);

/// The scheduled half of the lamp: it only goes dark if the signal is still gone.
[[nodiscard]] std::optional<BlockChange> lampTick(WorldState& world, std::string_view worldName, std::int32_t x,
                                                  std::int32_t y, std::int32_t z);

} // namespace redstoneReactors

} // namespace basalt::server::runtime::world
