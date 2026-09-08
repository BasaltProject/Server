#pragma once

#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::world {

/// How strong a redstone signal is at a place, and how far the dust carries it.
///
/// parity: `Level.getBestNeighborSignal` for what reaches a block, and
/// `DefaultRedstoneWireEvaluator` for what the dust itself settles on: the strongest signal touching
/// it, or one less than the strongest dust beside it, whichever is greater. The step up and down a
/// block is the same rule vanilla uses, conductors included.
///
/// What each component *does* with that signal (a repeater's delay, a comparator's arithmetic, a
/// piston's push) is its own thing; this is only the wire and the sources that carry their strength
/// in their own state.
namespace redstone {

/// parity: `Redstone.SIGNAL_MAX`.
inline constexpr std::int32_t kMaxSignal = 15;

/// parity: `ComparatorBlockEntity`'s own `OutputSignal`, which is where the level is kept.
inline constexpr std::string_view kComparatorOutputKey = "OutputSignal";

/// What the block at these coordinates gives off when asked from `from`. parity:
/// `BlockState.getSignal`, whose direction runs from whoever is asking *to* this block, so the block
/// below is asked with `Down`. Getting that backwards silently powers everything from the wrong side.
///
/// `mutedWire` is `RedStoneWireBlock.shouldSignal`, the flag vanilla turns off while dust works out
/// its own strength. `playing` is the jukebox's, which no state property carries.
[[nodiscard]] std::int32_t signalOf(std::int32_t stateId, registry::BlockFace from, std::int32_t openers = 0,
                                    std::int32_t comparatorOutput = kMaxSignal, bool mutedWire = false,
                                    bool playing = true);

/// The *strong* signal a block pushes into the one at `from`: what travels through a solid block and
/// out the other side.
///
/// parity: each block's `getDirectSignal`. A lever, a button and a plate strongly power the block
/// they are attached to, a torch the block above it, and a repeater or comparator the block they
/// feed. Everything else is weak, which is why a block of redstone lights the dust beside it but
/// does not put out a torch on the block next to it.
[[nodiscard]] std::int32_t directSignalOf(std::int32_t stateId, registry::BlockFace from, std::int32_t openers = 0,
                                          std::int32_t comparatorOutput = kMaxSignal, bool mutedWire = false);

/// What one block hands to another, the way `SignalGetter.getSignal` asks it: its own signal, and
/// for a block that conducts, whatever is strongly powering it from any side.
[[nodiscard]] std::int32_t signalFrom(const WorldState& world, std::string_view worldName, std::int32_t x,
                                      std::int32_t y, std::int32_t z, registry::BlockFace from, bool mutedWire = false);

/// What the comparator at these coordinates last worked out, or fifteen when nothing is stored.
[[nodiscard]] std::int32_t comparatorOutputAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `SignalGetter.getDirectSignalTo`, the strongest strong signal reaching a block.
[[nodiscard]] std::int32_t directSignalTo(const WorldState& world, std::string_view worldName, std::int32_t x,
                                          std::int32_t y, std::int32_t z, bool mutedWire = false);

/// What a comparator reads off a container: how full it is, from zero to fifteen. Each pair is a
/// slot's count and what a full stack of it would be.
///
/// parity: `AbstractContainerMenu.getRedstoneSignalFromContainer`, which averages the fraction of a
/// full stack in every slot and then walks that fraction up to fifteen. One item in a furnace of
/// three slots is already a one, which is the quirk people build with.
[[nodiscard]] std::int32_t containerSignal(std::span<const std::pair<std::int32_t, std::int32_t>> slots,
                                           std::int32_t size);

/// parity: `Level.getBestNeighborSignal`: the strongest of the six faces around a block.
[[nodiscard]] std::int32_t bestNeighbourSignal(const WorldState& world, std::string_view worldName, std::int32_t x,
                                               std::int32_t y, std::int32_t z);

/// What a piece of dust at these coordinates should be carrying.
///
/// parity: `DefaultRedstoneWireEvaluator.calculateTargetStrength`: a block signal of fifteen wins
/// outright, and otherwise the dust takes the greater of the block signal and one less than the
/// strongest dust it can see, up over a conductor or down off a block that is not one.
[[nodiscard]] std::int32_t wireStrength(const WorldState& world, std::string_view worldName, std::int32_t x,
                                        std::int32_t y, std::int32_t z);

[[nodiscard]] bool isWire(std::int32_t stateId);

/// parity: `BlockBehaviour.isSignalSource`, the blocks that can feed a circuit.
[[nodiscard]] bool isSignalSource(std::int32_t stateId);

/// Every position a change here can be felt at: the six neighbours, and through any of them that
/// conducts, its own six.
///
/// parity: vanilla reaches the second ring because a lever, a button or a plate calls
/// `updateNeighborsAt` on the block it is attached to as well as on itself. Pulled at read time as
/// this build does, the same reach has to be walked here: a torch on the far side of the block a
/// lever sits on is two steps away, and that is the commonest gate in the game.
[[nodiscard]] std::vector<std::array<std::int32_t, 3>> reachOf(const WorldState& world, std::int32_t x, std::int32_t y,
                                                               std::int32_t z);

/// Told about every piece of dust that moved, so whatever schedules the components around it can.
using Notify = std::function<void(std::int32_t x, std::int32_t y, std::int32_t z)>;

/// Settles the dust at these coordinates and every piece it reaches, answering what changed.
///
/// parity: vanilla walks this out one neighbour update at a time, updating a wire and then the wires
/// around it. The order in which the pieces settle is not observable once the dust has stopped
/// moving, and the strengths it stops at are the same, so this walks the connected dust until
/// nothing moves.
std::vector<BlockChange> settle(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y,
                                std::int32_t z, const Notify& notify = {});

} // namespace redstone

} // namespace basalt::server::runtime::world
