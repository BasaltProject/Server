#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {

class WorldState;

/// The End's gateways: where one leads, and the far one it builds the first time it is used.
///
/// parity: `TheEndGatewayBlockEntity`.
namespace endGateways {

/// parity: `TheEndGatewayBlockEntity.COOLDOWN_TIME`, the two seconds a used gateway rests.
inline constexpr std::int32_t kCooldownTicks = 40;

/// parity: `Player.getDimensionChangingDelay`, the ticks before a player can use a portal again.
inline constexpr std::int32_t kPlayerDelayTicks = 10;

/// parity: the block event a gateway sends to flash its beam.
inline constexpr std::int32_t kBeamEvent = 1;

using BlockPos = std::array<std::int32_t, 3>;

[[nodiscard]] bool isGateway(std::int32_t stateId);

/// parity: `EndGatewayFeature` with its block entity, leading to `exit` if there is one.
void place(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::optional<BlockPos> exit, bool exact);

/// Where the gateway at this position sends whoever enters, or nothing if it leads nowhere. In the
/// End a gateway without an exit first builds one about a thousand blocks out.
///
/// parity: `TheEndGatewayBlockEntity.getPortalPosition`, which answers the bottom centre of a block.
[[nodiscard]] std::optional<std::array<double, 3>> destination(WorldState& world, std::int32_t x, std::int32_t y,
                                                               std::int32_t z, bool inEnd);

} // namespace endGateways

} // namespace basalt::server::runtime::world
