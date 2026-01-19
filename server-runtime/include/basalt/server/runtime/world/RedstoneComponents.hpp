#pragma once

#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

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
/// `Level.updateNeighbourForOutputSignal`, reached from `BlockEntity.setChanged`: four horizontal
/// squares, one more through a block that conducts, and only comparators listen.
void scheduleComparatorsAround(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `ObserverBlock.updateShape` runs on the observer itself the moment it lands, because
/// `updateFromNeighbourShapes` asks every direction and one of them is the one it watches. So an
/// observer books a turn as soon as it is placed, whatever is in front of it, and a chain of them
/// is one turn out of step without this.
void watchOnPlace(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

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
