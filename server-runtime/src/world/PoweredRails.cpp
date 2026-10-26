#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/config/GameplayRules.hpp>
#include <basalt/server/runtime/world/PoweredRails.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <string>

namespace basalt::server::runtime::world::poweredRails {
namespace {

constexpr std::array<std::string_view, 2> kKinds{"powered_rail", "activator_rail"};
constexpr std::array<std::string_view, 3> kNorthSouthShapes{"north_south", "ascending_north", "ascending_south"};
constexpr std::array<std::string_view, 3> kEastWestShapes{"east_west", "ascending_east", "ascending_west"};

[[nodiscard]] std::string shapeAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return {};
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto found = properties.find("shape");
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] bool samePoweredRail(const WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y,
                                   std::int32_t z, const wire::common::Identifier& block, bool forward,
                                   std::int32_t depth, std::string_view axis);

/// parity: `findPoweredRailSignal`.
[[nodiscard]] bool findSignal(const WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y,
                              std::int32_t z, const wire::common::Identifier& block, bool forward, std::int32_t depth) {
    if (depth >= world.gameplay().blocks.poweredRailActivationRange) {
        return false;
    }
    const std::string shape = shapeAt(world, x, y, z);
    std::int32_t nextX = x;
    std::int32_t nextY = y;
    std::int32_t nextZ = z;
    bool checkBelow = true;
    std::string_view axis;

    if (shape == "north_south") {
        forward ? ++nextZ : --nextZ;
        axis = "north_south";
    } else if (shape == "east_west") {
        forward ? --nextX : ++nextX;
        axis = "east_west";
    } else if (shape == "ascending_east") {
        if (forward) {
            --nextX;
        } else {
            ++nextX;
            ++nextY;
            checkBelow = false;
        }
        axis = "east_west";
    } else if (shape == "ascending_west") {
        if (forward) {
            --nextX;
            ++nextY;
            checkBelow = false;
        } else {
            ++nextX;
        }
        axis = "east_west";
    } else if (shape == "ascending_north") {
        if (forward) {
            ++nextZ;
        } else {
            --nextZ;
            ++nextY;
            checkBelow = false;
        }
        axis = "north_south";
    } else if (shape == "ascending_south") {
        if (forward) {
            ++nextZ;
            ++nextY;
            checkBelow = false;
        } else {
            --nextZ;
        }
        axis = "north_south";
    } else {
        return false;
    }

    if (samePoweredRail(world, worldName, nextX, nextY, nextZ, block, forward, depth, axis)) {
        return true;
    }
    return checkBelow && samePoweredRail(world, worldName, nextX, nextY - 1, nextZ, block, forward, depth, axis);
}

/// parity: `isSameRailWithPower`.
bool samePoweredRail(const WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y,
                     std::int32_t z, const wire::common::Identifier& block, bool forward, std::int32_t depth,
                     std::string_view axis) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return false;
    }
    const wire::common::Identifier* theirs = registry::blockStateDefinitions::blockOf(*stateId);
    if (theirs == nullptr || *theirs != block) {
        return false;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto shapeAt = properties.find("shape");
    if (shapeAt == properties.end()) {
        return false;
    }
    const std::string_view shape = shapeAt->second;
    if (axis == "east_west" && std::ranges::find(kNorthSouthShapes, shape) != kNorthSouthShapes.end()) {
        return false;
    }
    if (axis == "north_south" && std::ranges::find(kEastWestShapes, shape) != kEastWestShapes.end()) {
        return false;
    }
    const auto powered = properties.find("powered");
    if (powered == properties.end() || powered->second != "true") {
        return false;
    }
    if (redstone::bestNeighbourSignal(world, worldName, x, y, z) > 0) {
        return true;
    }
    return findSignal(world, worldName, x, y, z, block, forward, depth + 1);
}

} // namespace

std::optional<BlockChange> update(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y,
                                  std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr || std::ranges::find(kKinds, block->path()) == kKinds.end()) {
        return std::nullopt;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto poweredAt = properties.find("powered");
    const bool powered = poweredAt != properties.end() && poweredAt->second == "true";
    const bool wanted = redstone::bestNeighbourSignal(world, worldName, x, y, z) > 0 ||
                        findSignal(world, worldName, x, y, z, *block, true, 0) ||
                        findSignal(world, worldName, x, y, z, *block, false, 0);
    if (wanted == powered) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> turned =
        registry::blockStateDefinitions::stateIdOf(*block, {{"powered", wanted ? "true" : "false"}}, *stateId);
    if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
        return std::nullopt;
    }
    return BlockChange{.x = x, .y = y, .z = z, .stateId = *turned};
}

} // namespace basalt::server::runtime::world::poweredRails
