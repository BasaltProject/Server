#pragma once

#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

namespace basalt::server::runtime::world {

/// What a comparator reads out of the block in front of it, gathered in one place.
///
/// parity: `BlockState.hasAnalogOutputSignal` and each block's `getAnalogOutputSignal`. It lives
/// apart from both sides so the redstone pass keeps taking a plain callback and the container code
/// keeps not knowing what a comparator is.
namespace comparatorReading {

/// The reading for this world, ready to hand to the redstone pass.
[[nodiscard]] redstoneComponents::ContainerSignalAt of(WorldState& world);

} // namespace comparatorReading

} // namespace basalt::server::runtime::world
