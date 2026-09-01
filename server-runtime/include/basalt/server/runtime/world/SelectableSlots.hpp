#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::world {

/// Which slot of a block a click landed on, worked out from where on the face it hit.
///
/// parity: `SelectableSlotContainer`. Two blocks use it, the chiseled bookshelf and the shelf, and
/// both only answer to a click on the face they look out of: a bookshelf poked from the side takes
/// nothing.
namespace selectableSlots {

/// The slot the click landed on, or nothing when it was not on the block's own face at all.
///
/// The coordinates are the ones the client sends, relative to the block itself, which is what
/// vanilla arrives at after subtracting the neighbouring position from the hit.
[[nodiscard]] std::optional<std::int32_t> hitSlot(std::string_view facing, std::string_view face, float clickX,
                                                  float clickY, float clickZ, std::int32_t rows, std::int32_t columns);

} // namespace selectableSlots

} // namespace basalt::server::runtime::world
