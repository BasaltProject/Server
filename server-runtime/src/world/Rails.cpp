#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/Rails.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace basalt::server::runtime::world::rails {
namespace {

constexpr std::array<std::string_view, 3> kStraightOnly{"powered_rail", "detector_rail", "activator_rail"};

constexpr std::string_view kNorthSouth = "north_south";
constexpr std::string_view kEastWest = "east_west";

/// parity: `RailBlock.updateState`'s own three, the junction a signal can throw.
constexpr std::int32_t kThreeWay = 3;

using Position = std::array<std::int32_t, 3>;

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

[[nodiscard]] std::string shapeOf(std::int32_t stateId) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find("shape");
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] bool isStraightOnly(std::string_view path) {
    return std::ranges::find(kStraightOnly, path) != kStraightOnly.end();
}

/// parity: `updateConnections`, the two positions each shape reaches towards.
[[nodiscard]] std::vector<Position> connectionsOf(std::int32_t x, std::int32_t y, std::int32_t z,
                                                  std::string_view shape) {
    if (shape == kNorthSouth) {
        return {{x, y, z - 1}, {x, y, z + 1}};
    }
    if (shape == kEastWest) {
        return {{x - 1, y, z}, {x + 1, y, z}};
    }
    if (shape == "ascending_east") {
        return {{x - 1, y, z}, {x + 1, y + 1, z}};
    }
    if (shape == "ascending_west") {
        return {{x - 1, y + 1, z}, {x + 1, y, z}};
    }
    if (shape == "ascending_north") {
        return {{x, y + 1, z - 1}, {x, y, z + 1}};
    }
    if (shape == "ascending_south") {
        return {{x, y, z - 1}, {x, y + 1, z + 1}};
    }
    if (shape == "south_east") {
        return {{x + 1, y, z}, {x, y, z + 1}};
    }
    if (shape == "south_west") {
        return {{x - 1, y, z}, {x, y, z + 1}};
    }
    if (shape == "north_west") {
        return {{x - 1, y, z}, {x, y, z - 1}};
    }
    if (shape == "north_east") {
        return {{x + 1, y, z}, {x, y, z - 1}};
    }
    return {};
}

/// parity: `getRail`, which looks at the block, then one up, then one down.
[[nodiscard]] std::optional<Position> railAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    for (const std::int32_t offsetY : {0, 1, -1}) {
        const std::optional<std::int32_t> stateId = world.blockState(x, y + offsetY, z);
        if (stateId.has_value() && isRail(*stateId)) {
            return Position{x, y + offsetY, z};
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::vector<Position> softConnections(const WorldState& world, std::int32_t x, std::int32_t y,
                                                    std::int32_t z, std::string_view shape);

/// parity: `canConnectTo`: a rail already joined at both ends takes no more.
[[nodiscard]] bool canConnectTo(const WorldState& world, const Position& at, std::int32_t x, std::int32_t y,
                                std::int32_t z) {
    static_cast<void>(y);
    const std::optional<std::int32_t> stateId = world.blockState(at[0], at[1], at[2]);
    if (!stateId.has_value()) {
        return false;
    }
    const std::string shape = shapeOf(*stateId);
    if (shape.empty()) {
        return false;
    }
    const std::vector<Position> links = softConnections(world, at[0], at[1], at[2], shape);
    if (std::ranges::any_of(links, [x, z](const Position& link) { return link[0] == x && link[2] == z; })) {
        return true;
    }
    return links.size() != 2;
}

/// parity: `removeSoftConnections`.
std::vector<Position> softConnections(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                      std::string_view shape) {
    std::vector<Position> links;
    for (const Position& towards : connectionsOf(x, y, z, shape)) {
        const std::optional<Position> rail = railAt(world, towards[0], towards[1], towards[2]);
        if (!rail.has_value()) {
            continue;
        }
        const std::optional<std::int32_t> theirState = world.blockState((*rail)[0], (*rail)[1], (*rail)[2]);
        if (!theirState.has_value()) {
            continue;
        }
        const std::string theirShape = shapeOf(*theirState);
        if (theirShape.empty()) {
            continue;
        }
        const std::vector<Position> theirs = connectionsOf((*rail)[0], (*rail)[1], (*rail)[2], theirShape);
        if (std::ranges::any_of(theirs, [x, z](const Position& link) { return link[0] == x && link[2] == z; })) {
            links.push_back(*rail);
        }
    }
    return links;
}

/// parity: `hasNeighborRail`, which is a rail that can still take one more connection.
[[nodiscard]] bool hasNeighbourRail(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                    std::int32_t stepX, std::int32_t stepZ) {
    const std::optional<Position> at = railAt(world, x + stepX, y, z + stepZ);
    return at.has_value() && canConnectTo(world, *at, x, y, z);
}

/// parity: a straight rail climbs when the rail it points at is one block higher.
[[nodiscard]] std::string slope(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                const std::string& shape) {
    const auto railAbove = [&world](std::int32_t atX, std::int32_t atY, std::int32_t atZ) {
        const std::optional<std::int32_t> stateId = world.blockState(atX, atY, atZ);
        return stateId.has_value() && isRail(*stateId);
    };
    std::string result = shape;
    if (shape == kNorthSouth) {
        if (railAbove(x, y + 1, z - 1)) {
            result = "ascending_north";
        }
        if (railAbove(x, y + 1, z + 1)) {
            result = "ascending_south";
        }
    }
    if (shape == kEastWest) {
        if (railAbove(x + 1, y + 1, z)) {
            result = "ascending_east";
        }
        if (railAbove(x - 1, y + 1, z)) {
            result = "ascending_west";
        }
    }
    return result;
}

[[nodiscard]] std::optional<std::int32_t> withShape(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                    std::int32_t stateId, const std::string& shape) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> turned =
        registry::blockStateDefinitions::stateIdOf(*block, {{"shape", shape}}, stateId);
    if (!turned.has_value()) {
        return std::nullopt;
    }
    if (*turned == stateId) {
        return stateId;
    }
    if (!world.setBlockState(x, y, z, *turned)) {
        return std::nullopt;
    }
    return turned;
}

/// parity: `RailState.place`'s shape choice.
[[nodiscard]] std::optional<std::string> shapeFor(const WorldState& world, std::string_view worldName, std::int32_t x,
                                                  std::int32_t y, std::int32_t z, std::int32_t stateId) {
    const std::string defaultShape = shapeOf(stateId);
    if (defaultShape.empty()) {
        return std::nullopt;
    }
    const bool straight = isStraightOnly(pathOf(stateId));
    const bool north = hasNeighbourRail(world, x, y, z, 0, -1);
    const bool south = hasNeighbourRail(world, x, y, z, 0, 1);
    const bool west = hasNeighbourRail(world, x, y, z, -1, 0);
    const bool east = hasNeighbourRail(world, x, y, z, 1, 0);
    const bool alongZ = north || south;
    const bool alongX = west || east;

    std::string shape;
    if (alongZ && !alongX) {
        shape = kNorthSouth;
    }
    if (alongX && !alongZ) {
        shape = kEastWest;
    }
    if (!straight) {
        if (south && east && !north && !west) {
            shape = "south_east";
        }
        if (south && west && !north && !east) {
            shape = "south_west";
        }
        if (north && west && !south && !east) {
            shape = "north_west";
        }
        if (north && east && !south && !west) {
            shape = "north_east";
        }
    }
    if (shape.empty()) {
        if (alongZ && alongX) {
            shape = defaultShape;
        } else if (alongZ) {
            shape = kNorthSouth;
        } else if (alongX) {
            shape = kEastWest;
        }
        // parity: `place(hasSignal, ...)`.
        if (!straight) {
            const bool hasSignal = redstone::bestNeighbourSignal(world, worldName, x, y, z) > 0;
            if (hasSignal) {
                if (south && east) {
                    shape = "south_east";
                }
                if (south && west) {
                    shape = "south_west";
                }
                if (north && east) {
                    shape = "north_east";
                }
                if (north && west) {
                    shape = "north_west";
                }
            } else {
                if (north && west) {
                    shape = "north_west";
                }
                if (north && east) {
                    shape = "north_east";
                }
                if (south && west) {
                    shape = "south_west";
                }
                if (south && east) {
                    shape = "south_east";
                }
            }
        }
    }
    return slope(world, x, y, z, shape.empty() ? defaultShape : shape);
}

/// parity: `RailState.connectTo`.
void connectTo(WorldState& world, const Position& at, std::int32_t stateId, std::int32_t towardsX,
               std::int32_t towardsZ, const Changed& changed) {
    const std::string shape = shapeOf(stateId);
    if (shape.empty()) {
        return;
    }
    const bool straight = isStraightOnly(pathOf(stateId));
    const std::int32_t x = at[0];
    const std::int32_t y = at[1];
    const std::int32_t z = at[2];
    std::vector<Position> links = softConnections(world, x, y, z, shape);
    links.push_back({towardsX, y, towardsZ});

    const auto has = [&links](std::int32_t atX, std::int32_t atZ) {
        return std::ranges::any_of(links,
                                   [atX, atZ](const Position& link) { return link[0] == atX && link[2] == atZ; });
    };
    const bool north = has(x, z - 1);
    const bool south = has(x, z + 1);
    const bool west = has(x - 1, z);
    const bool east = has(x + 1, z);

    std::string wanted;
    if (north || south) {
        wanted = kNorthSouth;
    }
    if (west || east) {
        wanted = kEastWest;
    }
    if (!straight) {
        if (south && east && !north && !west) {
            wanted = "south_east";
        }
        if (south && west && !north && !east) {
            wanted = "south_west";
        }
        if (north && west && !south && !east) {
            wanted = "north_west";
        }
        if (north && east && !south && !west) {
            wanted = "north_east";
        }
    }
    const std::string settled = slope(world, x, y, z, wanted.empty() ? std::string{kNorthSouth} : wanted);
    if (const std::optional<std::int32_t> turned = withShape(world, x, y, z, stateId, settled);
        turned.has_value() && changed) {
        changed(x, y, z, *turned);
    }
}

/// parity: `countPotentialConnections`.
[[nodiscard]] std::int32_t potentialConnections(const WorldState& world, std::int32_t x, std::int32_t y,
                                                std::int32_t z) {
    std::int32_t count = 0;
    for (const std::array<std::int32_t, 2>& step :
         {std::array<std::int32_t, 2>{0, -1}, std::array<std::int32_t, 2>{0, 1}, std::array<std::int32_t, 2>{-1, 0},
          std::array<std::int32_t, 2>{1, 0}}) {
        if (railAt(world, x + step[0], y, z + step[1]).has_value()) {
            ++count;
        }
    }
    return count;
}

} // namespace

bool isRail(std::int32_t stateId) {
    const std::string path = pathOf(stateId);
    return path == "rail" || isStraightOnly(path);
}

std::optional<std::int32_t> place(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y,
                                  std::int32_t z, const Changed& changed) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || !isRail(*stateId)) {
        return std::nullopt;
    }
    const std::optional<std::string> shape = shapeFor(world, worldName, x, y, z, *stateId);
    if (!shape.has_value()) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> turned = withShape(world, x, y, z, *stateId, *shape);
    if (!turned.has_value()) {
        return std::nullopt;
    }
    if (changed) {
        changed(x, y, z, *turned);
    }
    // parity: `place` re-bends every rail it decided to connect to.
    for (const Position& at : connectionsOf(x, y, z, *shape)) {
        const std::optional<Position> neighbour = railAt(world, at[0], at[1], at[2]);
        if (!neighbour.has_value()) {
            continue;
        }
        const std::optional<std::int32_t> theirState =
            world.blockState((*neighbour)[0], (*neighbour)[1], (*neighbour)[2]);
        if (!theirState.has_value() || !canConnectTo(world, *neighbour, x, y, z)) {
            continue;
        }
        connectTo(world, *neighbour, *theirState, x, z, changed);
    }
    return turned;
}

std::optional<std::int32_t> onNeighbourChange(WorldState& world, std::string_view worldName, std::int32_t x,
                                              std::int32_t y, std::int32_t z, std::int32_t changedStateId) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    if (pathOf(*stateId) != "rail") {
        return std::nullopt;
    }
    if (!redstone::isSignalSource(changedStateId)) {
        return std::nullopt;
    }
    if (potentialConnections(world, x, y, z) != kThreeWay) {
        return std::nullopt;
    }
    const std::optional<std::string> shape = shapeFor(world, worldName, x, y, z, *stateId);
    if (!shape.has_value() || shapeOf(*stateId) == *shape) {
        return std::nullopt;
    }
    return withShape(world, x, y, z, *stateId, *shape);
}

} // namespace basalt::server::runtime::world::rails
