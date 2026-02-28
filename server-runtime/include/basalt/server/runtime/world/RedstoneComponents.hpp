#pragma once

#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world {

/// The pieces that read a signal and answer with one of their own: the torch that inverts and the
/// repeater that delays.
///
/// parity: `RedstoneTorchBlock.tick` and `DiodeBlock.tick`, both of which act on a scheduled tick
/// rather than the moment their neighbour moves. That delay is not a detail: it is what makes a
/// repeater a repeater and what stops a torch from oscillating within one tick.
///
/// The burn-out of a torch flicked more than eight times in sixty ticks is not modelled yet, and is
/// marked as such where it belongs.
namespace redstoneComponents {

/// parity: `ObserverBlock`, `ComparatorBlock` and `RedstoneTorchBlock`, each with its own delay.
inline constexpr std::int32_t kObserverDelay = 2;

/// parity: `RedstoneTorchBlock.RECENT_TOGGLE_TIMER`, `MAX_RECENT_TOGGLES` and `RESTART_DELAY`: eight
/// times out inside sixty ticks burns a torch, and it does not look again for a hundred and sixty.
inline constexpr std::int64_t kTorchToggleWindow = 60;
inline constexpr std::int32_t kMaxTorchToggles = 8;
inline constexpr std::int32_t kTorchRestartDelay = 160;
inline constexpr std::int32_t kComparatorDelay = 2;
inline constexpr std::int32_t kTorchDelay = 2;

[[nodiscard]] const wire::common::Identifier& torch();
[[nodiscard]] const wire::common::Identifier& repeater();
[[nodiscard]] const wire::common::Identifier& comparator();
[[nodiscard]] const wire::common::Identifier& observer();

/// How full the container at a position is, as a comparator reads it, or nothing when the block is
/// not a container this build keeps contents for.
///
/// A callback rather than a call into the container code: what a comparator needs is a number, and
/// taking it this way keeps this file below the one that knows what a hopper holds.
using ContainerSignalAt =
    std::function<std::optional<std::int32_t>(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view from)>;

/// Every block this knows how to tick, so a caller can ask before scheduling.
[[nodiscard]] bool isComponent(std::int32_t stateId);

/// Asks each component around a change to look again, which is how vanilla's neighbour updates
/// reach them. parity: `DiodeBlock.checkTickOnNeighbor` and the torch's own scheduling.
///
/// `arrived` tells a block put down or taken away from one that changed its own state: parity, a
/// flick writes `UPDATE_ALL` and reaches six squares, `onPlace` reaches further. `containerSignal`
/// is what a comparator here would read, so one behind a container books no turn it does not need.
void scheduleAround(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
                    bool arrived = true, const ContainerSignalAt& containerSignal = {});

/// The turn a comparator books when the container it reads changed hands. parity:
/// `Level.updateNeighbourForOutputSignal`: four horizontal squares, one more through a conducting
/// block, and only comparators listen. `worldName` may be empty where the container code has no name
/// to hand; it only reaches a trapped chest's viewer count, which moving an item cannot change.
void scheduleComparatorsAround(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y,
                               std::int32_t z, const ContainerSignalAt& containerSignal = {});

/// The dust's own square and its six, in the order the jar walks them.
///
/// parity: `DefaultRedstoneWireEvaluator.updatePowerStrength` puts them in a `Sets.newHashSet()` and
/// then iterates it, so what comes out is the hash map's bucket order and not the order they went
/// in. Seven entries never grow a table of sixteen, so the order is a pure function of where the
/// dust is.
[[nodiscard]] std::vector<std::array<std::int32_t, 3>> wireReach(std::int32_t x, std::int32_t y, std::int32_t z);

/// The squares a component tells when its own state moves, past the six around it. parity:
/// `DiodeBlock.updateNeighborsInFront` and `ObserverBlock` tell the square they feed and that
/// square's own neighbours. Without that second step a lamp stuck to the block a repeater feeds
/// never hears the repeater turn on.
struct Reach {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    /// The face of this square that must **not** be told, which is the way back to the component.
    /// parity: `updateNeighborsAtExceptFromFacing`. Empty for a torch, which excludes nothing.
    std::optional<server::registry::BlockFace> skip;
};

[[nodiscard]] std::vector<Reach> outputReach(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z);

/// The squares a block that has just gone tells on top of its own six. parity:
/// `BaseRailBlock.affectNeighborsAfterRemoval` reaches one square past itself, and `LecternBlock`'s
/// tells the square under a lectern that was still powered.
[[nodiscard]] std::vector<Reach> removalReach(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z);

/// Whether the block that just left answers its removal with `Containers.updateNeighboursAfterDestroy`.
///
/// parity: thirteen containers do, and the call carries the state that **went**, which is the only
/// thing that reaches a comparator sitting one square past a block that conducts. `Level.setBlock`
/// makes the same call looking at the state that arrived, and air is not read by anybody.
[[nodiscard]] bool tellsComparatorsOnRemoval(std::int32_t stateId);

/// The turn every component on and around the square a diode feeds books, less the way back to the
/// diode itself. parity: `DiodeBlock.updateNeighborsInFront`, which is a `neighborChanged` on that
/// square and an `updateNeighborsAtExceptFromFacing` on it.
void scheduleInFront(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
                     server::registry::BlockFace back, const ContainerSignalAt& containerSignal = {});

/// The turn a component books when a piece of dust beside it changed strength.
///
/// parity: `DefaultRedstoneWireEvaluator.updatePowerStrength` ends by running `updateNeighborsAt` on
/// the dust **and on each of its six squares**, so its reach is two steps. That second step is how a
/// repeater one square past the dust hears that the dust lit up; without it a chain of repeaters
/// stops at the first one.
void scheduleAroundWire(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
                        const ContainerSignalAt& containerSignal = {});

/// Rewrites a repeater's `locked` property from what its sides say, answering the new state when it
/// moved. parity: `RepeaterBlock.updateShape`, which recomputes it whenever a shape update arrives
/// from an axis that is not the repeater's own. It is only a property: what the lock actually does
/// is read live on the repeater's turn, so this is the half a player sees and a snapshot records.
/// Whether a repeater with these properties would arrive held, for a square that has no repeater in
/// it yet. parity: `RepeaterBlock.getStateForPlacement`, which asks `isLocked` before the block is
/// written, so a repeater placed between two powered ones is locked from its very first tick.
[[nodiscard]] bool wouldBeLocked(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                 const std::map<std::string, std::string>& properties);

[[nodiscard]] std::optional<std::int32_t> relock(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// The same for the six squares around a block that just wrote its own state, which is the shape
/// update `Level.setBlock` runs even with the flag that skips the neighbour updates. Answers what
/// moved. parity: a repeater beside another one that turned on locks on that same turn, and without
/// this it only locked later, when something else happened to touch it.
[[nodiscard]] std::vector<BlockChange> relockAround(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `ObserverBlock.updateShape` runs on the observer itself the moment it lands, so one books
/// a turn as soon as it is placed and a chain of them is otherwise a turn out of step.
///
/// `removed` is what stood there before: an observer written over an observer books nothing, the way
/// `onPlace` does nothing for a block that did not change. One that arrives powered belongs to
/// `neighbourUpdates::settleArrivedSignal`, which can also give the notice that goes with it.
void watchOnPlace(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                  std::optional<std::int32_t> removed);

/// parity: `ObserverBlock.updateShape`, which only fires for the block it faces: an observer
/// watches one direction and ignores everything else that moves around it.
void watchFor(WorldState& world, std::int32_t changedX, std::int32_t changedY, std::int32_t changedZ);

/// One component's turn. Answers the new state when it moved, so the caller can tell the clients and
/// carry the change on to the dust around it.
[[nodiscard]] std::optional<BlockChange> tick(WorldState& world, std::string_view worldName, std::int32_t x,
                                              std::int32_t y, std::int32_t z, std::int64_t gameTime,
                                              const ContainerSignalAt& containerSignal = {});

/// What a comparator reads behind itself: a container's fullness first, and otherwise the plain
/// signal. parity: `ComparatorBlock.getInputSignal`.
[[nodiscard]] std::int32_t comparatorInput(const WorldState& world, std::string_view worldName, std::int32_t x,
                                           std::int32_t y, std::int32_t z,
                                           const std::map<std::string, std::string>& properties,
                                           const ContainerSignalAt& containerSignal = {});

} // namespace redstoneComponents

} // namespace basalt::server::runtime::world
