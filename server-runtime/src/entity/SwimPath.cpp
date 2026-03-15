#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/entity/SwimPath.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <string_view>

namespace basalt::server::runtime::entity::swimPath {
namespace {

using Position = std::array<std::int32_t, 3>;

constexpr double kHeuristicWeight = 1.5;
/// parity: `PathType.BREACH`'s malus, and the eight `findAcceptedNode` adds for a dry node.
constexpr double kBreachMalus = 4.0 + 8.0;

[[nodiscard]] double distance(const Position& from, const Position& to) {
    const auto dx = static_cast<double>(to[0] - from[0]);
    const auto dy = static_cast<double>(to[1] - from[1]);
    const auto dz = static_cast<double>(to[2] - from[2]);
    return std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
}

[[nodiscard]] std::int32_t manhattan(const Position& from, const Position& to) {
    return std::abs(to[0] - from[0]) + std::abs(to[1] - from[1]) + std::abs(to[2] - from[2]);
}

/// The malus of a node, or nothing when it cannot be entered.
[[nodiscard]] std::optional<double> accepted(world::WorldState& world, const Position& at, bool breaching) {
    if (isSwimmable(world, at[0], at[1], at[2])) {
        return 0.0;
    }
    if (breaching && isBreach(world, at[0], at[1], at[2])) {
        return kBreachMalus;
    }
    return std::nullopt;
}

} // namespace

bool holdsWater(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return false;
    }
    const std::string_view path = block->path();
    if (path == "water" || path == "bubble_column" || path == "kelp" || path == "kelp_plant" || path == "seagrass" ||
        path == "tall_seagrass") {
        return true;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto waterlogged = properties.find("waterlogged");
    return waterlogged != properties.end() && waterlogged->second == "true";
}

bool isSwimmable(world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = world.blockState(x, y, z);
    // parity: `isPathfindable(WATER)`, water with nothing to bump into.
    return state.has_value() && holdsWater(*state) && registry::blockShapes::isEmptyShape(*state);
}

bool isBreach(world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = world.blockState(x, y, z);
    return state.has_value() && registry::blocks::isAir(*state) && isSwimmable(world, x, y - 1, z);
}

std::optional<std::vector<Position>> find(world::WorldState& world, const Position& from, const Position& to,
                                          bool breaching) {
    std::map<Position, Position> cameFrom;
    std::map<Position, double> cost;
    std::map<Position, double> score;
    std::vector<Position> open{from};
    cost[from] = 0.0;
    score[from] = distance(from, to) * kHeuristicWeight;
    Position best = from;
    double bestDistance = distance(from, to);
    const auto valueOr = [](const std::map<Position, double>& values, const Position& at) {
        const auto found = values.find(at);
        return found == values.end() ? std::numeric_limits<double>::max() : found->second;
    };
    // parity: `moveTo(x, y, z, speed)` asks for an accuracy of one block.
    bool reached = manhattan(from, to) <= 1;
    std::int32_t examined = 0;
    while (!reached && !open.empty() && examined++ < kMaxNodes) {
        const auto cheapest = std::ranges::min_element(open, [&](const Position& one, const Position& other) {
            return valueOr(score, one) < valueOr(score, other);
        });
        const Position current = *cheapest;
        open.erase(cheapest);
        if (manhattan(current, to) <= 1) {
            best = current;
            reached = true;
            break;
        }
        // parity: `getNeighbors`, the faces in `Direction.values()` order and then the flat corners
        // whose two sides are both open.
        std::vector<Position> next;
        const std::array<Position, 6> faces{Position{0, -1, 0}, Position{0, 1, 0},  Position{0, 0, -1},
                                            Position{0, 0, 1},  Position{-1, 0, 0}, Position{1, 0, 0}};
        std::map<Position, bool> openFace;
        for (const Position& face : faces) {
            const Position at{current[0] + face[0], current[1] + face[1], current[2] + face[2]};
            openFace[face] = accepted(world, at, breaching).has_value();
            if (openFace[face]) {
                next.push_back(at);
            }
        }
        const std::array<std::pair<Position, Position>, 4> corners{
            std::pair{Position{0, 0, -1}, Position{1, 0, 0}}, std::pair{Position{1, 0, 0}, Position{0, 0, 1}},
            std::pair{Position{0, 0, 1}, Position{-1, 0, 0}}, std::pair{Position{-1, 0, 0}, Position{0, 0, -1}}};
        for (const auto& [one, other] : corners) {
            const Position at{current[0] + one[0] + other[0], current[1], current[2] + one[2] + other[2]};
            if (openFace[one] && openFace[other] && accepted(world, at, breaching).has_value()) {
                next.push_back(at);
            }
        }
        for (const Position& neighbour : next) {
            const double stepCost = valueOr(cost, current) + distance(current, neighbour) +
                                    accepted(world, neighbour, breaching).value_or(0.0);
            if (stepCost >= valueOr(cost, neighbour)) {
                continue;
            }
            cameFrom[neighbour] = current;
            cost[neighbour] = stepCost;
            score[neighbour] = stepCost + (distance(neighbour, to) * kHeuristicWeight);
            if (const double left = distance(neighbour, to); left < bestDistance) {
                bestDistance = left;
                best = neighbour;
            }
            if (std::ranges::find(open, neighbour) == open.end()) {
                open.push_back(neighbour);
            }
        }
    }
    if (best == from) {
        return reached ? std::optional{std::vector<Position>{}} : std::nullopt;
    }
    std::vector<Position> path;
    for (Position at = best; at != from; at = cameFrom.at(at)) {
        path.push_back(at);
    }
    std::ranges::reverse(path);
    return path;
}

} // namespace basalt::server::runtime::entity::swimPath
