#pragma once

#include <basalt/server/runtime/entity/PathTypes.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::runtime::entity {

/// parity: `PathFinder`.
namespace flyingPathFinder {

using Node = std::array<std::int32_t, 3>;

/// parity: `floor(width + 1)` across and `floor(height + 1)` up.
struct Footprint {
    std::int32_t width = 1;
    std::int32_t height = 1;
};

struct Mob {
    pathTypes::Maluses maluses = pathTypes::defaults();
    std::optional<Node> at;
    std::int32_t minY = -64;
};

[[nodiscard]] std::vector<Node> find(world::WorldState& world, const Footprint& size, const Node& from, const Node& to,
                                     std::int32_t maxNodes, double reach, const Mob& mob = {});

} // namespace flyingPathFinder

} // namespace basalt::server::runtime::entity
