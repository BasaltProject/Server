#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::world {

/// parity: `SelectableSlotContainer`.
namespace selectableSlots {

/// The coordinates are the client's, relative to the block, as vanilla gets after subtracting the neighbour position.
[[nodiscard]] std::optional<std::int32_t> hitSlot(std::string_view facing, std::string_view face, float clickX,
                                                  float clickY, float clickZ, std::int32_t rows, std::int32_t columns);

} // namespace selectableSlots

} // namespace basalt::server::runtime::world
