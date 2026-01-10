#include <basalt/server/registry/BlockConnections.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/Walls.hpp>

#include <cstdint>

namespace basalt::server::runtime::world::walls {
namespace {

using registry::BlockFace;

constexpr std::string_view kNone = "none";
constexpr std::string_view kLow = "low";
constexpr std::string_view kTall = "tall";

/// parity: `WallBlock.TEST_SHAPE_POST` and `TEST_SHAPES_WALL`, in units of a thirty-second: the
/// middle two-sixteenths column, and a two-wide strip from each edge to the middle.
constexpr std::int32_t kMiddleFrom = 14;
constexpr std::int32_t kMiddleTo = 18;
constexpr std::int32_t kHalf = 18;
constexpr std::int32_t kFull = 32;

/// A cobblestone wall's own state, used only to borrow the fence rule below.
///
/// The rule it borrows asks what the *neighbour* is, so which wall this is does not matter.
[[nodiscard]] std::int32_t wallLikeFence() {
    static const std::int32_t instance =
        registry::blockStateDefinitions::stateIdOf(wire::common::Identifier::vanilla("oak_fence"), {}, std::nullopt)
            .value_or(0);
    return instance;
}

/// parity: `WallBlock.connectsTo`, which is the fence rule plus every wall and every pane, whether
/// or not their faces are sturdy.
[[nodiscard]] bool connects(std::int32_t neighbour, BlockFace face) {
    return registry::blockConnections::isWall(neighbour) || registry::blockConnections::isPanes(neighbour) ||
           registry::blockConnections::connectsTo(wallLikeFence(), neighbour, face);
}

[[nodiscard]] std::string side(bool connected, std::int32_t above, BlockFace face) {
    if (!connected) {
        return std::string{kNone};
    }
    bool covered = false;
    switch (face) {
    case BlockFace::North:
        covered = registry::blockShapes::faceCovers(above, BlockFace::Down, 0, kHalf, kMiddleFrom, kMiddleTo);
        break;
    case BlockFace::South:
        covered = registry::blockShapes::faceCovers(above, BlockFace::Down, kMiddleFrom, kFull, kMiddleFrom, kMiddleTo);
        break;
    case BlockFace::West:
        covered = registry::blockShapes::faceCovers(above, BlockFace::Down, kMiddleFrom, kMiddleTo, 0, kHalf);
        break;
    default:
        covered = registry::blockShapes::faceCovers(above, BlockFace::Down, kMiddleFrom, kMiddleTo, kMiddleFrom, kFull);
        break;
    }
    return std::string{covered ? kTall : kLow};
}

/// parity: `WallBlock.shouldRaisePost`: a corner always keeps its post, a straight run may not.
[[nodiscard]] bool raisesPost(const std::map<std::string, std::string>& sides, std::int32_t above,
                              bool aboveOverridesPost) {
    if (registry::blockConnections::isWall(above)) {
        const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(above);
        const auto up = properties.find("up");
        if (up != properties.end() && up->second == "true") {
            return true;
        }
    }
    const bool northNone = sides.at("north") == kNone;
    const bool southNone = sides.at("south") == kNone;
    const bool westNone = sides.at("west") == kNone;
    const bool eastNone = sides.at("east") == kNone;
    const bool corner =
        (northNone && southNone && westNone && eastNone) || northNone != southNone || westNone != eastNone;
    if (corner) {
        return true;
    }
    const bool tallLine = (sides.at("north") == kTall && sides.at("south") == kTall) ||
                          (sides.at("east") == kTall && sides.at("west") == kTall);
    if (tallLine) {
        return false;
    }
    return aboveOverridesPost ||
           registry::blockShapes::faceCovers(above, BlockFace::Down, kMiddleFrom, kMiddleTo, kMiddleFrom, kMiddleTo);
}

} // namespace

std::optional<std::map<std::string, std::string>> shapeOf(std::int32_t stateId, std::int32_t north, std::int32_t south,
                                                          std::int32_t west, std::int32_t east, std::int32_t above,
                                                          bool aboveOverridesPost) {
    if (!registry::blockConnections::isWall(stateId)) {
        return std::nullopt;
    }
    std::map<std::string, std::string> sides{
        {"north", side(connects(north, BlockFace::North), above, BlockFace::North)},
        {"south", side(connects(south, BlockFace::South), above, BlockFace::South)},
        {"west", side(connects(west, BlockFace::West), above, BlockFace::West)},
        {"east", side(connects(east, BlockFace::East), above, BlockFace::East)},
    };
    sides.emplace("up", raisesPost(sides, above, aboveOverridesPost) ? "true" : "false");
    return sides;
}

} // namespace basalt::server::runtime::world::walls
