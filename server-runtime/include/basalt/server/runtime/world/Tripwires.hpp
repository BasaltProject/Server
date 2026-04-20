#pragma once

#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `TripWireBlock.checkPressed`.
namespace tripwires {

inline constexpr std::int32_t kRecheckTicks = 10;

inline constexpr std::int32_t kMaxLength = 42;

[[nodiscard]] const wire::common::Identifier& wireBlock();
[[nodiscard]] const wire::common::Identifier& hookBlock();

[[nodiscard]] bool isWire(std::int32_t stateId);

[[nodiscard]] bool isHook(std::int32_t stateId);

/// parity: `TripWireBlock.shouldConnectTo`.
[[nodiscard]] bool connectsTo(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                              server::registry::BlockFace towards);

/// parity: `TripWireBlock.updateShape`.
[[nodiscard]] std::optional<std::int32_t> reshape(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                  std::int32_t stateId, std::optional<registry::BlockFace> towards);

/// parity: `TripWireHookBlock.updateShape`.
[[nodiscard]] bool hookFalls(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                             std::int32_t stateId, registry::BlockFace towards);

/// parity: `checkPressed`.
[[nodiscard]] bool crosses(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId, double entityX,
                           double entityY, double entityZ, double halfWidth, double height);

[[nodiscard]] bool isStandingOn(std::int32_t x, std::int32_t y, std::int32_t z, double entityX, double entityY,
                                double entityZ);

/// parity: `checkPressed`.
void press(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, bool occupied, std::int64_t gameTime,
           std::vector<BlockChange>& changes);

/// parity: `TripWireHookBlock`'s own ten ticks.
inline constexpr std::int32_t kHookRecheckTicks = 10;

void calculateHook(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int64_t gameTime,
                   std::vector<BlockChange>& changes, std::int32_t wireSource = -1,
                   std::optional<std::int32_t> wireSourceState = std::nullopt,
                   std::optional<std::int32_t> destroyedState = std::nullopt);

/// parity: `TripWireBlock.updateSource`.
void updateSource(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t wireStateId,
                  std::int64_t gameTime, std::vector<BlockChange>& changes);

} // namespace tripwires

} // namespace basalt::server::runtime::world
