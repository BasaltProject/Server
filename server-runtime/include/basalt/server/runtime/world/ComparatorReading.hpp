#pragma once

#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::world {

/// parity: `BlockState.hasAnalogOutputSignal` and each block's `getAnalogOutputSignal`.
namespace comparatorReading {

[[nodiscard]] std::optional<std::int32_t> at(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                             std::string_view from);

/// parity: `BlockState.hasAnalogOutputSignal` is a property of the block alone.
[[nodiscard]] bool exists(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

[[nodiscard]] redstoneComponents::ContainerSignalAt of(WorldState& world);

} // namespace comparatorReading

} // namespace basalt::server::runtime::world
