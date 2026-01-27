#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>

namespace basalt::server::runtime::world {

/// The button a stuck arrow holds down.
///
/// parity: `ButtonBlock.entityInside` into `checkPressed`, which looks for an `AbstractArrow` inside
/// the button's own outline and re-books its turn for as long as it finds one. A wooden button is
/// held down by an arrow; a stone one is not, because its `BlockSetType` says so.
namespace arrowButtons {

/// parity: `BlockSetType.canButtonBeActivatedByArrows`. The five sets that say no are iron, copper,
/// gold, stone and polished blackstone, and only the last two have a button, so naming those two is
/// the whole rule. Every other family uses the one-argument constructor, which says yes.
[[nodiscard]] bool acceptsArrows(std::string_view path);

/// The button's outline in the block, in sixteenths, as minX, minY, minZ, maxX, maxY, maxZ.
///
/// parity: `ButtonBlock.makeShapes`, which takes `Block.boxZ(6, 4, 8, 16)`, turns it onto the face
/// it is attached to and cuts away the twelve cube (fourteen while pressed). What is left is the
/// part that sticks out of the wall: two sixteenths, or one while pressed.
[[nodiscard]] std::array<double, 6> outline(std::int32_t stateId);

/// Whether an entity of this size, standing at this point, reaches the button at these coordinates.
[[nodiscard]] bool reaches(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId, double entityX,
                           double entityY, double entityZ, double halfWidth, double height);

/// Whether an arrow is holding the button at these coordinates down.
[[nodiscard]] bool heldByArrow(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId);

} // namespace arrowButtons

} // namespace basalt::server::runtime::world
