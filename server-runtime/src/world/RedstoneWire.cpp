#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>
#include <basalt/server/runtime/world/RedstoneWire.hpp>

#include <array>
#include <cstdint>
#include <string_view>

namespace basalt::server::runtime::world::redstoneWire {
namespace {

using registry::BlockFace;

constexpr std::string_view kNone = "none";
constexpr std::string_view kSide = "side";
constexpr std::string_view kUp = "up";

constexpr std::array<BlockFace, 4> kSides{BlockFace::North, BlockFace::South, BlockFace::East, BlockFace::West};

[[nodiscard]] std::string_view nameOf(BlockFace face) {
    switch (face) {
    case BlockFace::Down:
        return "down";
    case BlockFace::Up:
        return "up";
    case BlockFace::North:
        return "north";
    case BlockFace::South:
        return "south";
    case BlockFace::West:
        return "west";
    case BlockFace::East:
        break;
    }
    return "east";
}

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] std::int32_t wireState() {
    static const std::int32_t instance =
        registry::blockStateDefinitions::stateIdOf(block(), {}, std::nullopt).value_or(0);
    return instance;
}

/// parity: `shouldConnectTo`: dust joins dust, a diode only end on, an observer only at its face,
/// and anything else that gives off a signal from a side.
[[nodiscard]] bool connectsTo(std::int32_t stateId, const BlockFace* from) {
    const wire::common::Identifier* id = registry::blockStateDefinitions::blockOf(stateId);
    if (id == nullptr) {
        return false;
    }
    const std::string_view path = id->path();
    if (path == "redstone_wire") {
        return true;
    }
    if (path == "repeater") {
        const std::string facing = propertyOf(stateId, "facing");
        if (facing.empty() || from == nullptr) {
            return false;
        }
        return facing == nameOf(*from) || facing == nameOf(registry::opposite(*from));
    }
    if (path == "observer") {
        return from != nullptr && propertyOf(stateId, "facing") == nameOf(*from);
    }
    return from != nullptr && redstone::isSignalSource(stateId);
}

[[nodiscard]] bool conducts(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    return stateId.has_value() && registry::blockSupport::conductsRedstone(*stateId);
}

/// parity: `getConnectingSide`, the one that decides side, up or nothing.
[[nodiscard]] std::string connectingSide(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                         BlockFace face, bool canGoUp) {
    const std::int32_t nx = x + registry::stepX(face);
    const std::int32_t nz = z + registry::stepZ(face);
    const std::optional<std::int32_t> neighbour = world.blockState(nx, y, nz);
    if (!neighbour.has_value()) {
        return std::string{kNone};
    }
    if (canGoUp) {
        const std::optional<std::int32_t> above = world.blockState(nx, y + 1, nz);
        if (!above.has_value()) {
            return std::string{kNone};
        }
        // parity: a trapdoor is the exception that a dust can always climb over.
        const wire::common::Identifier* id = registry::blockStateDefinitions::blockOf(*neighbour);
        const bool climbable = (id != nullptr && std::string_view{id->path()}.ends_with("_trapdoor")) ||
                               registry::blockSupport::canSurvive(wireState(), *neighbour, 0, 0, 0, 0, 0, false);
        if (climbable && connectsTo(*above, nullptr)) {
            const BlockFace back = registry::opposite(face);
            return std::string{registry::blockShapes::isFaceSturdy(*neighbour, back) ? kUp : kSide};
        }
    }
    if (connectsTo(*neighbour, &face)) {
        return std::string{kSide};
    }
    if (registry::blockSupport::conductsRedstone(*neighbour)) {
        return std::string{kNone};
    }
    const std::optional<std::int32_t> below = world.blockState(nx, y - 1, nz);
    if (!below.has_value()) {
        return std::string{kNone};
    }
    return std::string{connectsTo(*below, nullptr) ? kSide : kNone};
}

} // namespace

const wire::common::Identifier& block() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("redstone_wire");
    return instance;
}

bool isWire(std::int32_t stateId) {
    const wire::common::Identifier* id = registry::blockStateDefinitions::blockOf(stateId);
    return id != nullptr && *id == block();
}

std::map<std::string, std::string> connectionsFor(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                  std::int32_t stateId) {
    const std::map<std::string, std::string> was = registry::blockStateDefinitions::propertiesOf(stateId);
    bool wasDot = true;
    for (const BlockFace face : kSides) {
        const auto found = was.find(std::string{nameOf(face)});
        wasDot = wasDot && (found == was.end() || found->second == kNone);
    }
    // parity: `canConnectUp`, which is one question asked once: is the way up clear at all.
    const bool canGoUp = !conducts(world, x, y + 1, z);
    std::map<std::string, std::string> sides;
    bool allNone = true;
    for (const BlockFace face : kSides) {
        std::string side = connectingSide(world, x, y, z, face, canGoUp);
        allNone = allNone && side == kNone;
        sides.emplace(std::string{nameOf(face)}, std::move(side));
    }
    if (wasDot && allNone) {
        return sides;
    }
    const bool north = sides.at("north") != kNone;
    const bool south = sides.at("south") != kNone;
    const bool east = sides.at("east") != kNone;
    const bool west = sides.at("west") != kNone;
    const bool noZ = !north && !south;
    const bool noX = !east && !west;
    // parity: a dust with nothing on one axis draws itself straight along the other.
    if (!west && noZ) {
        sides["west"] = std::string{kSide};
    }
    if (!east && noZ) {
        sides["east"] = std::string{kSide};
    }
    if (!north && noX) {
        sides["north"] = std::string{kSide};
    }
    if (!south && noX) {
        sides["south"] = std::string{kSide};
    }
    return sides;
}

std::optional<std::int32_t> reshape(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                    std::int32_t stateId) {
    if (!isWire(stateId)) {
        return std::nullopt;
    }
    const std::map<std::string, std::string> wanted = connectionsFor(world, x, y, z, stateId);
    const std::optional<std::int32_t> turned = registry::blockStateDefinitions::stateIdOf(block(), wanted, stateId);
    if (!turned.has_value() || *turned == stateId || !world.setBlockState(x, y, z, *turned)) {
        return std::nullopt;
    }
    return *turned;
}

std::vector<std::array<std::int32_t, 3>> acrossSteps(WorldState& world, std::int32_t x, std::int32_t y,
                                                     std::int32_t z) {
    std::vector<std::array<std::int32_t, 3>> reach;
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || !isWire(*stateId)) {
        return reach;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    for (const BlockFace face : kSides) {
        const auto found = properties.find(std::string{nameOf(face)});
        if (found == properties.end() || found->second == kNone) {
            continue;
        }
        const std::int32_t nx = x + registry::stepX(face);
        const std::int32_t nz = z + registry::stepZ(face);
        // Only across something that is not dust itself: two dusts side by side already see each
        // other through their own six sides.
        const std::optional<std::int32_t> beside = world.blockState(nx, y, nz);
        if (beside.has_value() && isWire(*beside)) {
            continue;
        }
        for (const std::int32_t step : {-1, 1}) {
            const std::optional<std::int32_t> other = world.blockState(nx, y + step, nz);
            if (other.has_value() && isWire(*other)) {
                reach.push_back({nx, y + step, nz});
            }
        }
    }
    return reach;
}

} // namespace basalt::server::runtime::world::redstoneWire
