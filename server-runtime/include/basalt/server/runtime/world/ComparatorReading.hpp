#pragma once

#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::world {

/// What a comparator reads out of the block in front of it, gathered in one place.
///
/// parity: `BlockState.hasAnalogOutputSignal` and each block's `getAnalogOutputSignal`. It lives
/// apart from both sides so the redstone pass keeps taking a plain callback and the container code
/// keeps not knowing what a comparator is.
namespace comparatorReading {

/// What a comparator sitting at `from` reads off the block at these coordinates, or nothing when
/// that block is not one a comparator reads at all.
[[nodiscard]] std::optional<std::int32_t> at(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                             std::string_view from);

/// Whether a comparator reads this block at all. parity: `BlockState.hasAnalogOutputSignal`, which
/// is a property of the block and not of what is in it nor of where the comparator stands.
[[nodiscard]] bool exists(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// The same thing as the callback the redstone pass takes. It captures nothing but the world, so
/// building one per call is free: the readers behind it are made only when something asks.
[[nodiscard]] redstoneComponents::ContainerSignalAt of(WorldState& world);

} // namespace comparatorReading

} // namespace basalt::server::runtime::world
