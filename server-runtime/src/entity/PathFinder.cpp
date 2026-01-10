#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/entity/PathFinder.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <string_view>

namespace basalt::server::runtime::entity::pathFinder {
namespace {

using Position = std::array<std::int32_t, 3>;

/// parity: `PathFinder`'s own 1.5 on the heuristic, which is what makes it take the direct way.
constexpr double kHeuristicWeight = 1.5;

/// parity: `WalkNodeEvaluator.getPathTypeStatic`'s malus for water and for what burns.
constexpr double kWaterMalus = 8.0;
constexpr double kDangerMalus = 16.0;

constexpr std::array<registry::BlockFace, 4> kHorizontal{registry::BlockFace::North, registry::BlockFace::South,
                                                         registry::BlockFace::West, registry::BlockFace::East};

[[nodiscard]] std::int64_t pack(std::int32_t x, std::int32_t y, std::int32_t z) {
    return ((static_cast<std::int64_t>(x) & 0x3ffffff) << 38) | ((static_cast<std::int64_t>(z) & 0x3ffffff) << 12) |
           (static_cast<std::int64_t>(y) & 0xfff);
}

[[nodiscard]] std::int32_t unpackX(std::int64_t packed) { return static_cast<std::int32_t>(packed >> 38); }

[[nodiscard]] std::int32_t unpackY(std::int64_t packed) { return static_cast<std::int32_t>(packed << 52 >> 52); }

[[nodiscard]] std::int32_t unpackZ(std::int64_t packed) { return static_cast<std::int32_t>(packed << 26 >> 38); }

[[nodiscard]] bool isOpen(world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    return stateId.has_value() && registry::blockShapes::isEmptyShape(*stateId);
}

/// parity: `Node.distanceTo`, the straight line the heuristic uses.
[[nodiscard]] double distance(std::int32_t fromX, std::int32_t fromY, std::int32_t fromZ, std::int32_t toX,
                              std::int32_t toY, std::int32_t toZ) {
    const auto dx = static_cast<double>(toX - fromX);
    const auto dy = static_cast<double>(toY - fromY);
    const auto dz = static_cast<double>(toZ - fromZ);
    return std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
}

/// parity: the cost a mob pays for walking through something it would rather not.
[[nodiscard]] double malus(world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return 0.0;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr) {
        return 0.0;
    }
    const std::string_view path = block->path();
    if (path == "water") {
        return kWaterMalus;
    }
    if (path == "lava" || path == "fire" || path == "soul_fire" || path == "magma_block" || path == "campfire" ||
        path == "soul_campfire" || path == "sweet_berry_bush") {
        return kDangerMalus;
    }
    return 0.0;
}

/// Where a step in this direction lands: the same height, one up, or up to three down.
[[nodiscard]] std::optional<Position> step(world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    if (canStandAt(world, x, y, z)) {
        return Position{x, y, z};
    }
    if (canStandAt(world, x, y + 1, z) && isOpen(world, x, y + 2, z)) {
        return Position{x, y + 1, z};
    }
    for (std::int32_t drop = 1; drop <= kMaxDrop; ++drop) {
        if (canStandAt(world, x, y - drop, z)) {
            return Position{x, y - drop, z};
        }
    }
    return std::nullopt;
}

/// parity: `WalkNodeEvaluator.getNeighbors`: the four sides and the four corners, up one or down
/// three.
[[nodiscard]] std::vector<Position> neighbours(world::WorldState& world, std::int32_t x, std::int32_t y,
                                               std::int32_t z) {
    std::vector<Position> found;
    for (const registry::BlockFace face : kHorizontal) {
        const std::optional<Position> landed = step(world, x + registry::stepX(face), y, z + registry::stepZ(face));
        if (landed.has_value()) {
            found.push_back(*landed);
        }
    }
    // parity: a corner is only walkable when both sides of it are, which is what stops a mob cutting
    // through the inside of a corner.
    for (const std::int32_t dx : {-1, 1}) {
        for (const std::int32_t dz : {-1, 1}) {
            if (!isOpen(world, x + dx, y, z) || !isOpen(world, x, y, z + dz)) {
                continue;
            }
            const std::optional<Position> landed = step(world, x + dx, y, z + dz);
            if (landed.has_value()) {
                found.push_back(*landed);
            }
        }
    }
    return found;
}

[[nodiscard]] std::vector<Position> reconstruct(const std::map<std::int64_t, std::int64_t>& cameFrom,
                                                std::int64_t goal) {
    std::vector<Position> path;
    std::int64_t current = goal;
    while (true) {
        path.push_back(Position{unpackX(current), unpackY(current), unpackZ(current)});
        const auto found = cameFrom.find(current);
        if (found == cameFrom.end()) {
            break;
        }
        current = found->second;
    }
    std::ranges::reverse(path);
    // The first entry is where it already is, which is not a step.
    if (!path.empty()) {
        path.erase(path.begin());
    }
    return path;
}

} // namespace

bool canStandAt(world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> ground = world.blockState(x, y - 1, z);
    if (!ground.has_value() || registry::blockShapes::isEmptyShape(*ground)) {
        return false;
    }
    return isOpen(world, x, y, z) && isOpen(world, x, y + 1, z);
}

std::optional<std::vector<Position>> find(world::WorldState& world, std::int32_t fromX, std::int32_t fromY,
                                          std::int32_t fromZ, std::int32_t toX, std::int32_t toY, std::int32_t toZ) {
    if (!canStandAt(world, fromX, fromY, fromZ)) {
        return std::nullopt;
    }
    const std::int64_t start = pack(fromX, fromY, fromZ);
    const std::int64_t goal = pack(toX, toY, toZ);
    if (start == goal) {
        return std::vector<Position>{};
    }

    std::map<std::int64_t, std::int64_t> cameFrom;
    std::map<std::int64_t, double> cost;
    std::map<std::int64_t, double> score;
    std::vector<std::int64_t> open;
    cost[start] = 0.0;
    score[start] = distance(fromX, fromY, fromZ, toX, toY, toZ);
    open.push_back(start);

    const auto scoreOf = [&score](std::int64_t node) {
        const auto found = score.find(node);
        return found == score.end() ? std::numeric_limits<double>::max() : found->second;
    };
    const auto costOf = [&cost](std::int64_t node) {
        const auto found = cost.find(node);
        return found == cost.end() ? std::numeric_limits<double>::max() : found->second;
    };

    std::int32_t examined = 0;
    while (!open.empty() && examined++ < kMaxNodes) {
        // A plain scan for the cheapest open node: a path this short never has enough of them for a
        // heap to pay for itself.
        const auto cheapest = std::ranges::min_element(
            open, [&scoreOf](std::int64_t one, std::int64_t other) { return scoreOf(one) < scoreOf(other); });
        const std::int64_t current = *cheapest;
        open.erase(cheapest);
        if (current == goal) {
            return reconstruct(cameFrom, current);
        }

        const std::int32_t x = unpackX(current);
        const std::int32_t y = unpackY(current);
        const std::int32_t z = unpackZ(current);
        for (const Position& neighbour : neighbours(world, x, y, z)) {
            const std::int64_t next = pack(neighbour[0], neighbour[1], neighbour[2]);
            const double stepCost = costOf(current) + distance(x, y, z, neighbour[0], neighbour[1], neighbour[2]) +
                                    malus(world, neighbour[0], neighbour[1], neighbour[2]);
            if (stepCost >= costOf(next)) {
                continue;
            }
            cameFrom[next] = current;
            cost[next] = stepCost;
            score[next] =
                stepCost + (distance(neighbour[0], neighbour[1], neighbour[2], toX, toY, toZ) * kHeuristicWeight);
            if (std::ranges::find(open, next) == open.end()) {
                open.push_back(next);
            }
        }
    }
    return std::nullopt;
}

} // namespace basalt::server::runtime::entity::pathFinder
