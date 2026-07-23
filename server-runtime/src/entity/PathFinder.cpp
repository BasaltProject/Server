#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/entity/PathFinder.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <string_view>

namespace basalt::server::runtime::entity::pathFinder {
namespace {

using Position = std::array<std::int32_t, 3>;

/// parity: `PathFinder`'s own 1.5 on the heuristic.
constexpr double kHeuristicWeight = 1.5;

/// parity: `WalkNodeEvaluator.getPathTypeStatic`'s malus for water and for what burns.
constexpr double kWaterMalus = 8.0;
constexpr double kDangerMalus = 16.0;
/// parity: `AmphibiousNodeEvaluator.prepare`'s own malus for dry land and for the water at a shore.
constexpr double kAmphibiousWalkableMalus = 6.0;
constexpr double kAmphibiousWaterBorderMalus = 4.0;

constexpr std::array<registry::BlockFace, 4> kHorizontal{registry::BlockFace::North, registry::BlockFace::South,
                                                         registry::BlockFace::West, registry::BlockFace::East};

[[nodiscard]] std::int64_t pack(std::int32_t x, std::int32_t y, std::int32_t z) {
    return ((static_cast<std::int64_t>(x) & 0x3ffffff) << 38) | ((static_cast<std::int64_t>(z) & 0x3ffffff) << 12) |
           (static_cast<std::int64_t>(y) & 0xfff);
}

[[nodiscard]] std::int32_t unpackX(std::int64_t packed) { return static_cast<std::int32_t>(packed >> 38); }

[[nodiscard]] std::int32_t unpackY(std::int64_t packed) { return static_cast<std::int32_t>(packed << 52 >> 52); }

[[nodiscard]] std::int32_t unpackZ(std::int64_t packed) { return static_cast<std::int32_t>(packed << 26 >> 38); }

[[nodiscard]] bool isOpen(world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                          bool doors = false) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    return stateId.has_value() && (registry::blockShapes::isEmptyShape(*stateId) || (doors && isMobDoor(*stateId)));
}

/// parity: `PathType.WATER`, water a mob can be in.
[[nodiscard]] bool isWater(world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    return stateId.has_value() && world::fluids::of(*stateId) == world::Submersion::Water &&
           registry::blockShapes::isEmptyShape(*stateId);
}

[[nodiscard]] bool standable(world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, bool doors) {
    const std::optional<std::int32_t> ground = world.blockState(x, y - 1, z);
    if (!ground.has_value() || registry::blockShapes::isEmptyShape(*ground) || (doors && isMobDoor(*ground))) {
        return false;
    }
    return isOpen(world, x, y, z, doors) && isOpen(world, x, y + 1, z, doors);
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

/// parity: `AmphibiousNodeEvaluator.getPathType` with its `prepare` malus.
[[nodiscard]] double amphibiousMalus(world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    if (isWater(world, x, y, z)) {
        constexpr std::array<std::array<std::int32_t, 3>, 6> kAround{
            {{0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}}};
        for (const std::array<std::int32_t, 3>& offset : kAround) {
            if (!isOpen(world, x + offset[0], y + offset[1], z + offset[2])) {
                return kAmphibiousWaterBorderMalus;
            }
        }
        return 0.0;
    }
    const double danger = malus(world, x, y, z);
    return danger > 0.0 ? danger : kAmphibiousWalkableMalus;
}

[[nodiscard]] std::optional<Position> step(world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                           bool doors = false, bool waterNodes = false) {
    // parity: `findAcceptedNode` hands an amphibious or floating mob the water node as it is.
    if (standable(world, x, y, z, doors) || (waterNodes && isWater(world, x, y, z))) {
        return Position{x, y, z};
    }
    if (standable(world, x, y + 1, z, doors) && isOpen(world, x, y + 2, z, doors)) {
        return Position{x, y + 1, z};
    }
    for (std::int32_t drop = 1; drop <= kMaxDrop; ++drop) {
        if (standable(world, x, y - drop, z, doors)) {
            return Position{x, y - drop, z};
        }
    }
    return std::nullopt;
}

/// parity: `WalkNodeEvaluator.getNeighbors`.
[[nodiscard]] std::vector<Position> neighbours(world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                               bool doors = false, bool amphibious = false, bool floats = false) {
    const bool waterNodes = amphibious || floats;
    std::vector<Position> found;
    for (const registry::BlockFace face : kHorizontal) {
        const std::optional<Position> landed =
            step(world, x + registry::stepX(face), y, z + registry::stepZ(face), doors, waterNodes);
        if (landed.has_value()) {
            found.push_back(*landed);
        }
    }
    // parity: a corner is walkable only when both its sides are, so a mob cannot cut an inside corner.
    for (const std::int32_t dx : {-1, 1}) {
        for (const std::int32_t dz : {-1, 1}) {
            if (!isOpen(world, x + dx, y, z, doors) || !isOpen(world, x, y, z + dz, doors)) {
                continue;
            }
            const std::optional<Position> landed = step(world, x + dx, y, z + dz, doors, waterNodes);
            if (landed.has_value()) {
                found.push_back(*landed);
            }
        }
    }
    // parity: `AmphibiousNodeEvaluator.getNeighbors`.
    if (amphibious) {
        if (isWater(world, x, y + 1, z)) {
            found.push_back(Position{x, y + 1, z});
        }
        if (isWater(world, x, y - 1, z)) {
            found.push_back(Position{x, y - 1, z});
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
    if (!path.empty()) {
        path.erase(path.begin());
    }
    return path;
}

} // namespace

bool canStandAt(world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    return standable(world, x, y, z, false);
}

std::array<std::int32_t, 3> amphibiousStartOf(world::WorldState& world, double x, double y, double z, double width,
                                              bool onGround, bool inWater) {
    if (!inWater) {
        return startOf(world, x, y, z, onGround);
    }
    const double half = width / 2.0;
    return {static_cast<std::int32_t>(std::floor(x - half)), static_cast<std::int32_t>(std::floor(y + 0.5)),
            static_cast<std::int32_t>(std::floor(z - half))};
}

bool isMobDoor(std::int32_t stateId) {
    // parity: `BlockTags.MOB_INTERACTABLE_DOORS`.
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && block->path().ends_with("_door") && block->path() != "iron_door";
}

std::array<std::int32_t, 3> startOf(world::WorldState& world, double x, double y, double z, bool onGround) {
    const auto blockX = static_cast<std::int32_t>(std::floor(x));
    const auto blockZ = static_cast<std::int32_t>(std::floor(z));
    if (onGround) {
        return {blockX, static_cast<std::int32_t>(std::floor(y + 0.5)), blockZ};
    }
    auto blockY = static_cast<std::int32_t>(std::floor(y + 1.0));
    for (std::int32_t step = 0; step < 64; ++step) {
        const std::optional<std::int32_t> below = world.blockState(blockX, blockY - 1, blockZ);
        if (!below.has_value() || (!registry::blocks::isAir(*below) && !registry::blockShapes::isEmptyShape(*below))) {
            break;
        }
        --blockY;
    }
    return {blockX, blockY, blockZ};
}

std::optional<Route> findRoute(world::WorldState& world, std::int32_t fromX, std::int32_t fromY, std::int32_t fromZ,
                               std::span<const Position> targets, const RouteOptions& options) {
    // parity: `WalkNodeEvaluator.getStart` takes the start as it comes, standable or not.
    if (targets.empty() || !world.blockState(fromX, fromY, fromZ).has_value()) {
        return std::nullopt;
    }

    struct Node {
        std::int64_t cameFrom = -1;
        float g = 0.0F;
        float f = 0.0F;
        float walked = 0.0F;
        bool closed = false;
    };

    struct Best {
        float h = std::numeric_limits<float>::max();
        std::int64_t node = 0;
        bool reached = false;
    };

    const auto distanceTo = [](const Position& one, const Position& other) {
        const auto dx = static_cast<float>(other[0] - one[0]);
        const auto dy = static_cast<float>(other[1] - one[1]);
        const auto dz = static_cast<float>(other[2] - one[2]);
        return std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
    };
    const auto unpack = [](std::int64_t key) { return Position{unpackX(key), unpackY(key), unpackZ(key)}; };
    std::map<std::int64_t, Node> nodes;
    std::vector<Best> best(targets.size());
    // parity: `getBestH`, which also remembers the node nearest each target.
    const auto bestH = [&](std::int64_t key) {
        const Position at = unpack(key);
        float h = std::numeric_limits<float>::max();
        for (std::size_t index = 0; index < targets.size(); ++index) {
            const float distance = distanceTo(at, targets[index]);
            if (distance < best[index].h) {
                best[index].h = distance;
                best[index].node = key;
            }
            h = std::min(h, distance);
        }
        return h;
    };
    const Position from{fromX, fromY, fromZ};
    const std::int64_t start = pack(fromX, fromY, fromZ);
    nodes[start].f = bestH(start);
    std::vector<std::int64_t> open{start};
    std::int32_t count = 0;
    bool anyReached = false;
    while (!open.empty()) {
        if (++count >= options.maxVisited) {
            break;
        }
        const auto cheapest = std::ranges::min_element(
            open, [&nodes](std::int64_t one, std::int64_t other) { return nodes[one].f < nodes[other].f; });
        const std::int64_t current = *cheapest;
        open.erase(cheapest);
        Node& node = nodes[current];
        node.closed = true;
        const Position at = unpack(current);
        for (std::size_t index = 0; index < targets.size(); ++index) {
            const Position& target = targets[index];
            const std::int32_t manhattan =
                std::abs(at[0] - target[0]) + std::abs(at[1] - target[1]) + std::abs(at[2] - target[2]);
            if (manhattan <= options.accuracy) {
                // parity: the path still ends at the node nearest the target, not at this one.
                best[index].reached = true;
                anyReached = true;
            }
        }
        if (anyReached) {
            break;
        }
        if (distanceTo(at, from) >= options.maxPathLength) {
            continue;
        }
        const float g = node.g;
        const float walked = node.walked;
        for (const Position& neighbour :
             neighbours(world, at[0], at[1], at[2], options.openDoors, options.amphibious, options.floats)) {
            const std::int64_t next = pack(neighbour[0], neighbour[1], neighbour[2]);
            const float distance = distanceTo(at, neighbour);
            const float nextWalked = walked + distance;
            const double stepMalus = options.amphibious
                                         ? amphibiousMalus(world, neighbour[0], neighbour[1], neighbour[2])
                                         : malus(world, neighbour[0], neighbour[1], neighbour[2]);
            const float tentative = g + distance + static_cast<float>(stepMalus);
            const auto known = nodes.find(next);
            const bool inOpen = std::ranges::find(open, next) != open.end();
            if (nextWalked >= options.maxPathLength ||
                (known != nodes.end() && (known->second.closed || (inOpen && tentative >= known->second.g)))) {
                continue;
            }
            Node& entry = nodes[next];
            entry.cameFrom = current;
            entry.g = tentative;
            entry.walked = nextWalked;
            entry.f = tentative + (bestH(next) * static_cast<float>(kHeuristicWeight));
            if (!inOpen) {
                open.push_back(next);
            }
        }
    }
    const auto build = [&](std::size_t index) {
        Route route;
        route.target = targets[index];
        route.reached = best[index].reached;
        for (std::int64_t key = best[index].node; key != -1 && key != start; key = nodes[key].cameFrom) {
            route.nodes.push_back(unpack(key));
        }
        std::ranges::reverse(route.nodes);
        return route;
    };
    std::optional<Route> chosen;
    float chosenDistance = std::numeric_limits<float>::max();
    for (std::size_t index = 0; index < targets.size(); ++index) {
        if (anyReached != best[index].reached) {
            continue;
        }
        Route route = build(index);
        const float distance = best[index].h;
        const bool better = !chosen.has_value() ||
                            (anyReached ? route.nodes.size() < chosen->nodes.size()
                                        : (distance < chosenDistance ||
                                           (distance == chosenDistance && route.nodes.size() < chosen->nodes.size())));
        if (better) {
            chosenDistance = distance;
            chosen = std::move(route);
        }
    }
    return chosen;
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
        // A plain scan for the cheapest open node: a path this short never pays for a heap.
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
