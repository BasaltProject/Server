#pragma once

#include <basalt/server/registry/BlockShapes.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::runtime::world {

/// The walk vanilla's `BlockPattern` makes, shared by every figure a head or a pumpkin finishes.
namespace blockPatterns {

using Cell = std::array<std::int32_t, 3>;

/// parity: `BlockPattern.translateAndRotate`: right is forwards crossed with up, and `down` walks
/// against up, which is what makes an aisle read top row first.
[[nodiscard]] Cell translateAndRotate(const Cell& origin, registry::BlockFace forwards, registry::BlockFace up,
                                      std::int32_t right, std::int32_t down, std::int32_t ahead);

/// Answers whether the one-aisle figure matches with its top left at `origin` in this orientation.
using Matches = std::function<bool(const Cell& origin, registry::BlockFace forwards, registry::BlockFace up)>;

/// Where a match was found.
struct Found {
    Cell origin{};
    registry::BlockFace forwards = registry::BlockFace::North;
    registry::BlockFace up = registry::BlockFace::Up;
};

/// parity: `BlockPattern.find`: a cube as wide as the figure's longest side, towards positive x, y
/// and z only, z outermost, and every orientation in `Direction.values()` order.
[[nodiscard]] std::optional<Found> find(const Cell& at, std::int32_t size, const Matches& matches);

} // namespace blockPatterns

} // namespace basalt::server::runtime::world
