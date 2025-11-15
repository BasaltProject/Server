#pragma once

#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>
#include <span>

namespace basalt::server::runtime::world {

/// The rail that notices a minecart standing on it.
///
/// parity: `DetectorRailBlock.checkPressed`: while something is on it, it is on and books twenty
/// ticks to look again; the tick it finds nothing, it goes out. It feeds every side weakly and the
/// block underneath strongly, which is what the redstone pass already reads off its own state.
namespace detectorRails {

/// parity: the twenty ticks a pressed rail waits before checking again.
inline constexpr std::int32_t kRecheckTicks = 20;

/// parity: `AbstractMinecart`, of which any kind presses the rail.
[[nodiscard]] bool isMinecart(const wire::common::Identifier& type);

/// Looks at what is standing on the rail and answers the change, if it changed.
///
/// parity: `checkPressed` is called both by a cart arriving and by the rail's own scheduled tick,
/// which is why this takes the whole answer rather than a direction.
[[nodiscard]] std::optional<BlockChange> check(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                               std::span<entity::ServerEntity* const> entities);

} // namespace detectorRails

} // namespace basalt::server::runtime::world
