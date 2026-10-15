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

/// parity: `RedstoneTorchBlock.tick`.
namespace redstoneComponents {

/// parity: `ObserverBlock`.
inline constexpr std::int32_t kObserverDelay = 2;

/// parity: `RedstoneTorchBlock.RECENT_TOGGLE_TIMER`.
inline constexpr std::int64_t kTorchToggleWindow = 60;
inline constexpr std::int32_t kMaxTorchToggles = 8;
inline constexpr std::int32_t kTorchRestartDelay = 160;
inline constexpr std::int32_t kComparatorDelay = 2;
inline constexpr std::int32_t kTorchDelay = 2;

/// parity: `DiodeBlock.setPlacedBy` books its one tick and not the diode's own delay.
inline constexpr std::int32_t kPlacementDelay = 1;

[[nodiscard]] const wire::common::Identifier& torch();
[[nodiscard]] const wire::common::Identifier& repeater();
[[nodiscard]] const wire::common::Identifier& comparator();
[[nodiscard]] const wire::common::Identifier& observer();

/// A callback so this file stays below the one that knows what a hopper holds.
using ContainerSignalAt =
    std::function<std::optional<std::int32_t>(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view from)>;

[[nodiscard]] bool isComponent(std::int32_t stateId);

/// parity: `DiodeBlock.checkTickOnNeighbor`.
void scheduleAround(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
                    bool arrived = true, const ContainerSignalAt& containerSignal = {});

/// parity: `Level.updateNeighbourForOutputSignal`.
void scheduleComparatorsAround(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y,
                               std::int32_t z, const ContainerSignalAt& containerSignal = {});

/// parity: `updatePowerStrength`.
[[nodiscard]] std::vector<std::array<std::int32_t, 3>> wireReach(std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `DiodeBlock.updateNeighborsInFront`.
struct Reach {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    /// parity: `updateNeighborsAtExceptFromFacing`.
    std::optional<server::registry::BlockFace> skip;
};

[[nodiscard]] std::vector<Reach> outputReach(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `BaseRailBlock.affectNeighborsAfterRemoval`.
[[nodiscard]] std::vector<Reach> removalReach(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `Containers.updateNeighboursAfterDestroy`.
[[nodiscard]] bool tellsComparatorsOnRemoval(std::int32_t stateId);

/// parity: `DiodeBlock.updateNeighborsInFront`.
void scheduleInFront(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
                     server::registry::BlockFace back, const ContainerSignalAt& containerSignal = {});

/// parity: `updatePowerStrength`.
void scheduleAroundWire(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
                        const ContainerSignalAt& containerSignal = {});

/// parity: `RepeaterBlock.getStateForPlacement`.
[[nodiscard]] bool wouldBeLocked(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                 const std::map<std::string, std::string>& properties);

[[nodiscard]] std::optional<std::int32_t> relock(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `Level.setBlock`.
[[nodiscard]] std::vector<BlockChange> relockAround(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `ObserverBlock.updateShape`.
void watchOnPlace(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `ObserverBlock.updateShape`.
void watchFor(WorldState& world, std::int32_t changedX, std::int32_t changedY, std::int32_t changedZ);

[[nodiscard]] std::optional<BlockChange> tick(WorldState& world, std::string_view worldName, std::int32_t x,
                                              std::int32_t y, std::int32_t z, std::int64_t gameTime,
                                              const ContainerSignalAt& containerSignal = {});

/// parity: `DiodeBlock.setPlacedBy`.
void bookOnPlacement(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
                     const ContainerSignalAt& containerSignal = {});

/// parity: `ComparatorBlock.getInputSignal`.
[[nodiscard]] std::int32_t comparatorInput(const WorldState& world, std::string_view worldName, std::int32_t x,
                                           std::int32_t y, std::int32_t z,
                                           const std::map<std::string, std::string>& properties,
                                           const ContainerSignalAt& containerSignal = {});

} // namespace redstoneComponents

} // namespace basalt::server::runtime::world
