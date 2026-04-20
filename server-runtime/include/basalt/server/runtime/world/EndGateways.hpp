#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {

class WorldState;

/// parity: `TheEndGatewayBlockEntity`.
namespace endGateways {

inline constexpr std::int32_t kCooldownTicks = 40;

inline constexpr std::int64_t kAttentionIntervalTicks = 2400;

inline constexpr std::int32_t kPlayerDelayTicks = 10;

inline constexpr std::int32_t kBeamEvent = 1;

using BlockPos = std::array<std::int32_t, 3>;

[[nodiscard]] bool isGateway(std::int32_t stateId);

/// parity: `EndGatewayFeature` with its block entity, leading to `exit` if there is one.
void place(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::optional<BlockPos> exit, bool exact);

/// parity: `TheEndGatewayBlockEntity.getPortalPosition` answers the bottom centre of a block.
[[nodiscard]] std::optional<std::array<double, 3>> destination(WorldState& world, std::int32_t x, std::int32_t y,
                                                               std::int32_t z, bool inEnd);

} // namespace endGateways

} // namespace basalt::server::runtime::world
