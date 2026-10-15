#pragma once

#include <basalt/server/registry/BlockShapes.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::runtime::world {

namespace blockPatterns {

using Cell = std::array<std::int32_t, 3>;

[[nodiscard]] Cell translateAndRotate(const Cell& origin, registry::BlockFace forwards, registry::BlockFace up,
                                      std::int32_t right, std::int32_t down, std::int32_t ahead);

using Matches = std::function<bool(const Cell& origin, registry::BlockFace forwards, registry::BlockFace up)>;

struct Found {
    Cell origin{};
    registry::BlockFace forwards = registry::BlockFace::North;
    registry::BlockFace up = registry::BlockFace::Up;
};

[[nodiscard]] std::optional<Found> find(const Cell& at, std::int32_t size, const Matches& matches);

} // namespace blockPatterns

} // namespace basalt::server::runtime::world
