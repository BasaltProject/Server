#include <basalt/server/registry/BlockConnections.hpp>
#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/BlockPlacement.hpp>
#include <basalt/server/runtime/world/NoteBlocks.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/Reshapes.hpp>
#include <basalt/server/runtime/world/Tripwires.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <map>
#include <numbers>
#include <optional>
#include <string>

namespace basalt::server::runtime::world::blockPlacement {
namespace {

using wire::protocol::play::Direction;

constexpr float kHalf = 0.5F;
constexpr float kQuarterTurn = 90.0F;
constexpr double kHalfTurn = 180.0;
constexpr float kFullTurn = 360.0F;

/// parity: `RotationSegment`'s sixteen.
constexpr std::int32_t kSegments = 16;

/// parity: `GrowingPlantHeadBlock.getStateForPlacement`'s twenty-five.
constexpr std::int32_t kGrowingAges = 25;

/// parity: `ScaffoldingBlock.getDistance`'s own seven.
constexpr std::int32_t kScaffoldReach = 7;

/// South first, the way vanilla numbers the quarters of a yaw.
constexpr auto kHorizontal =
    std::to_array<Direction>({Direction::South, Direction::West, Direction::North, Direction::East});

/// The blocks that pile another of themselves into the same space.
constexpr auto kStacking =
    std::to_array<std::string_view>({"candles", "pickles", "eggs", "layers", "flower_amount", "segment_amount"});

/// Six-way blocks that point at the face that was clicked rather than away from the player.
constexpr auto kClickedFaceBlocks =
    std::to_array<std::string_view>({"lightning_rod", "end_rod", "shulker_box", "amethyst_cluster", "amethyst_bud"});

/// The blocks whose `getStateForPlacement` uses `getHorizontalDirection()` as it comes, taken from
/// the jar rather than guessed: everything else takes its opposite, because a furnace or a chest
/// faces whoever put it down while a bed or a stair runs away from them.
constexpr auto kPlayerFacingBlocks =
    std::to_array<std::string_view>({"campfire", "bell", "decorated_pot", "calibrated_sculk_sensor"});

constexpr auto kPlayerFacingSuffixes = std::to_array<std::string_view>({"_stairs", "_door", "_bed", "_fence_gate"});

constexpr auto kGrowingHeads =
    std::to_array<std::string_view>({"kelp", "twisting_vines", "weeping_vines", "cave_vines"});

/// parity: `Mth.sin`'s 65,536-entry table; a quarter of it on is the cosine.
constexpr double kSinScale = 10430.378350470453;
constexpr std::int64_t kQuarterTable = 16384;

[[nodiscard]] float tableSin(float radians, std::int64_t offset) {
    const std::int64_t index =
        (static_cast<std::int64_t>((static_cast<double>(radians) * kSinScale) + static_cast<double>(offset))) & 65535;
    return static_cast<float>(std::sin(static_cast<double>(index) / kSinScale));
}

[[nodiscard]] std::string_view nameOf(Direction face) {
    switch (face) {
    case Direction::Down:
        return "down";
    case Direction::Up:
        return "up";
    case Direction::North:
        return "north";
    case Direction::South:
        return "south";
    case Direction::West:
        return "west";
    default:
        return "east";
    }
}

[[nodiscard]] std::optional<Direction> fromName(std::string_view name) {
    for (const Direction face :
         {Direction::Down, Direction::Up, Direction::North, Direction::South, Direction::West, Direction::East}) {
        if (nameOf(face) == name) {
            return face;
        }
    }
    return std::nullopt;
}

[[nodiscard]] Direction opposite(Direction face) {
    switch (face) {
    case Direction::Down:
        return Direction::Up;
    case Direction::Up:
        return Direction::Down;
    case Direction::North:
        return Direction::South;
    case Direction::South:
        return Direction::North;
    case Direction::West:
        return Direction::East;
    default:
        return Direction::West;
    }
}

[[nodiscard]] Direction clockwise(Direction face) {
    switch (face) {
    case Direction::North:
        return Direction::East;
    case Direction::East:
        return Direction::South;
    case Direction::South:
        return Direction::West;
    case Direction::West:
        return Direction::North;
    default:
        return face;
    }
}

[[nodiscard]] Direction counterClockwise(Direction face) { return clockwise(clockwise(clockwise(face))); }

[[nodiscard]] std::string_view axisOf(Direction face) {
    switch (face) {
    case Direction::Up:
    case Direction::Down:
        return "y";
    case Direction::North:
    case Direction::South:
        return "z";
    default:
        return "x";
    }
}

[[nodiscard]] std::int32_t stepX(Direction direction) { return wire::protocol::play::normalX(direction); }

[[nodiscard]] std::int32_t stepZ(Direction direction) { return wire::protocol::play::normalZ(direction); }

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* id = registry::blockStateDefinitions::blockOf(stateId);
    return id == nullptr ? std::string{} : std::string{id->path()};
}

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] std::optional<std::int32_t> numberOf(const std::string& text) {
    std::int32_t value = 0;
    const std::from_chars_result result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] std::int32_t stateAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    return world.blockState(x, y, z).value_or(0);
}

[[nodiscard]] bool isSnow(std::int32_t stateId) {
    const std::string path = pathOf(stateId);
    return path == "snow" || path == "snow_block";
}

/// parity: `Level.getFluidState`, which answers water for water and for anything logged with it.
[[nodiscard]] bool isWater(std::int32_t stateId) {
    return pathOf(stateId) == "water" || propertyOf(stateId, "waterlogged") == "true";
}

[[nodiscard]] bool endsWithAny(std::string_view path, std::span<const std::string_view> suffixes) {
    return std::ranges::any_of(suffixes, [path](std::string_view suffix) { return path.ends_with(suffix); });
}

/// The block itself, or any of its wood and colour variants: `end_rod` and `oak_shulker_box` alike.
[[nodiscard]] bool named(std::string_view path, std::span<const std::string_view> names) {
    return std::ranges::any_of(names, [path](std::string_view name) {
        return path == name || path.ends_with(std::string{"_"} + std::string{name});
    });
}

/// parity: `SegmentedAnglePrecision`'s sixteen, which is what a sign, a banner and a skull use.
[[nodiscard]] std::int32_t segmentOf(float degrees) {
    const auto turned = static_cast<std::int32_t>(std::lround(degrees * kSegments / kFullTurn));
    return turned & (kSegments - 1);
}

[[nodiscard]] bool isLowerDoor(std::int32_t stateId) {
    const std::string path = pathOf(stateId);
    return path.ends_with("_door") && propertyOf(stateId, "half") == "lower";
}

/// parity: `DoorBlock.getHinge`: a door beside another door, or with solid blocks on one side, hangs
/// away from them; with nothing to go by it hangs on the side that was clicked.
[[nodiscard]] std::string hingeOf(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                  Direction towards, float clickX, float clickZ) {
    const Direction left = counterClockwise(towards);
    const Direction right = clockwise(towards);
    const std::int32_t leftLower = stateAt(world, x + stepX(left), y, z + stepZ(left));
    const std::int32_t leftUpper = stateAt(world, x + stepX(left), y + 1, z + stepZ(left));
    const std::int32_t rightLower = stateAt(world, x + stepX(right), y, z + stepZ(right));
    const std::int32_t rightUpper = stateAt(world, x + stepX(right), y + 1, z + stepZ(right));
    const std::int32_t balance = (registry::blockShapes::isFullCube(leftLower) ? -1 : 0) +
                                 (registry::blockShapes::isFullCube(leftUpper) ? -1 : 0) +
                                 (registry::blockShapes::isFullCube(rightLower) ? 1 : 0) +
                                 (registry::blockShapes::isFullCube(rightUpper) ? 1 : 0);
    const bool doorLeft = isLowerDoor(leftLower);
    const bool doorRight = isLowerDoor(rightLower);
    if ((doorLeft && !doorRight) || balance > 0) {
        return "right";
    }
    if ((doorRight && !doorLeft) || balance < 0) {
        return "left";
    }
    const std::int32_t towardsX = stepX(towards);
    const std::int32_t towardsZ = stepZ(towards);
    const bool clicked = (towardsX >= 0 || clickZ >= kHalf) && (towardsX <= 0 || clickZ <= kHalf) &&
                         (towardsZ >= 0 || clickX <= kHalf) && (towardsZ <= 0 || clickX >= kHalf);
    return clicked ? "left" : "right";
}

/// parity: `BellBlock`'s own test: a sturdy face on both sides bolts the bell to the pair.
[[nodiscard]] bool bothWalls(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, Direction face) {
    const bool alongX = face == Direction::West || face == Direction::East;
    const std::array<std::pair<Direction, registry::BlockFace>, 2> sides =
        alongX ? std::array{std::pair{Direction::West, registry::BlockFace::East},
                            std::pair{Direction::East, registry::BlockFace::West}}
               : std::array{std::pair{Direction::North, registry::BlockFace::South},
                            std::pair{Direction::South, registry::BlockFace::North}};
    for (const auto& [side, towards] : sides) {
        if (!registry::blockShapes::isFaceSturdy(stateAt(world, x + stepX(side), y, z + stepZ(side)), towards)) {
            return false;
        }
    }
    return true;
}

/// parity: `ScaffoldingBlock.getDistance`, the same count its scheduled tick keeps.
[[nodiscard]] std::int32_t scaffoldDistance(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::int32_t below = stateAt(world, x, y - 1, z);
    std::int32_t nearest = kScaffoldReach;
    // parity: scaffolding is asked about first, and the sturdy face only when it is not scaffolding.
    // The top of a scaffold is sturdy, so the other order answers zero for a whole tower.
    if (pathOf(below) == "scaffolding") {
        nearest = numberOf(propertyOf(below, "distance")).value_or(kScaffoldReach);
    } else if (registry::blockShapes::isFaceSturdy(below, registry::BlockFace::Up)) {
        return 0;
    }
    for (const Direction side : {Direction::North, Direction::South, Direction::West, Direction::East}) {
        const std::int32_t neighbour = stateAt(world, x + stepX(side), y, z + stepZ(side));
        if (pathOf(neighbour) != "scaffolding") {
            continue;
        }
        nearest = std::min(nearest, numberOf(propertyOf(neighbour, "distance")).value_or(kScaffoldReach) + 1);
    }
    return std::min(nearest, kScaffoldReach);
}

/// parity: `candidatePartnerFacing`: only a *single* chest of the same kind is a candidate.
[[nodiscard]] std::optional<Direction> partnerFacing(const WorldState& world, std::int32_t x, std::int32_t y,
                                                     std::int32_t z, const wire::common::Identifier& block,
                                                     Direction towards) {
    const std::int32_t stateId = stateAt(world, x + stepX(towards), y, z + stepZ(towards));
    const wire::common::Identifier* id = registry::blockStateDefinitions::blockOf(stateId);
    if (id == nullptr || *id != block || propertyOf(stateId, "type") != "single") {
        return std::nullopt;
    }
    return fromName(propertyOf(stateId, "facing"));
}

/// parity: `ChestBlock.getChestType`, which looks left first and right second.
[[nodiscard]] std::string chestType(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                    const wire::common::Identifier& block, Direction facing) {
    if (partnerFacing(world, x, y, z, block, clockwise(facing)) == facing) {
        return "left";
    }
    if (partnerFacing(world, x, y, z, block, counterClockwise(facing)) == facing) {
        return "right";
    }
    return "single";
}

/// The per-block half of placement: what each `getStateForPlacement` sets that no general rule
/// covers. Answers a state id only when the block turns into a different one (bamboo), and otherwise
/// writes into `properties`.
[[nodiscard]] std::optional<std::int32_t> special(const wire::common::Identifier& block,
                                                  const registry::blockStateDefinitions::Definition& definition,
                                                  std::map<std::string, std::string>& properties,
                                                  const Placement& placement, random::RandomSource& random) {
    const std::string path{block.path()};
    const WorldState* world = placement.world;
    const std::int32_t x = placement.x;
    const std::int32_t y = placement.y;
    const std::int32_t z = placement.z;
    // parity: the blocks whose `getStateForPlacement` reads the power that is already in the square.
    // A trapdoor or a fence gate put down where a lever is on arrives **open**, and a shelf, a skull
    // and a crafter arrive switched on. Nothing reaches them afterwards to correct it: a block that
    // has just landed only hears its own `onPlace`, and none of these has one.
    const auto poweredHere = [&]() {
        return world != nullptr && redstone::bestNeighbourSignal(*world, placement.worldName, x, y, z) > 0;
    };
    if (path.ends_with("_trapdoor")) {
        // parity: the trapdoor only ever **sets** these two, so an unpowered square leaves the
        // default alone rather than writing false over it.
        if (poweredHere()) {
            properties["open"] = "true";
            properties["powered"] = "true";
        }
    } else if (path.ends_with("_fence_gate")) {
        properties["open"] = properties["powered"] = poweredHere() ? "true" : "false";
        // parity: `FenceGateBlock.getStateForPlacement`'s `IN_WALL`, which looks along the axis the
        // gate opens **across**, not the one it faces. A gate is in neither the fence nor the pane
        // family, so nothing joined it as it landed and one dropped between two walls sat a block
        // too high until something else touched it.
        if (world != nullptr) {
            const bool acrossZ = properties["facing"] == "north" || properties["facing"] == "south";
            const std::int32_t first = acrossZ ? stateAt(*world, x - 1, y, z) : stateAt(*world, x, y, z - 1);
            const std::int32_t second = acrossZ ? stateAt(*world, x + 1, y, z) : stateAt(*world, x, y, z + 1);
            properties["in_wall"] =
                registry::blockConnections::isWall(first) || registry::blockConnections::isWall(second) ? "true"
                                                                                                        : "false";
        }
    } else if (path.ends_with("_shelf") || path == "shelf" || path.ends_with("_head") || path.ends_with("_skull")) {
        properties["powered"] = poweredHere() ? "true" : "false";
    } else if (path == "crafter") {
        properties["triggered"] = poweredHere() ? "true" : "false";
    } else if (path == "tripwire" && world != nullptr) {
        // parity: `TripWireBlock.getStateForPlacement` sets all four sides from `shouldConnectTo`,
        // the same question `updateShape` asks one side at a time. Only fences and panes were joined
        // as they landed, so a line of string laid by hand stayed a row of stubs.
        for (const auto& [face, name] :
             std::array<std::pair<registry::BlockFace, std::string_view>, 4>{{{registry::BlockFace::North, "north"},
                                                                              {registry::BlockFace::South, "south"},
                                                                              {registry::BlockFace::West, "west"},
                                                                              {registry::BlockFace::East, "east"}}}) {
            properties[std::string{name}] = tripwires::connectsTo(*world, x, y, z, face) ? "true" : "false";
        }
    } else if (path == "hopper") {
        // parity: `HopperBlock.getStateForPlacement`, which points the spout **into** the square that
        // was clicked, and straight down whenever that would be up or down. A hopper has five
        // facings, so neither the four-way nor the six-way rule above reaches it and every one used
        // to land pointing down.
        const Direction back = opposite(placement.face);
        properties["facing"] =
            back == Direction::Up || back == Direction::Down ? std::string{"down"} : std::string{nameOf(back)};
    } else if (path == "repeater" && world != nullptr) {
        // parity: `RepeaterBlock.getStateForPlacement` asks `isLocked` before the block is written,
        // so one dropped between two powered repeaters is held from its very first tick.
        properties["locked"] = redstoneComponents::wouldBeLocked(*world, x, y, z, properties) ? "true" : "false";
    }
    // parity: `SnowyBlock.getStateForPlacement`, grass and mycelium put down under snow.
    if (!definition.valuesOf("snowy").empty() && world != nullptr) {
        properties["snowy"] = isSnow(stateAt(*world, x, y + 1, z)) ? "true" : "false";
    }
    // parity: `CampfireBlock.getStateForPlacement`: hay under it makes the tall smoke, and a campfire
    // placed in water goes down unlit.
    if (path.ends_with("campfire") && world != nullptr) {
        properties["signal_fire"] = pathOf(stateAt(*world, x, y - 1, z)) == "hay_block" ? "true" : "false";
        properties["lit"] = properties["waterlogged"] != "true" ? "true" : "false";
    }
    // parity: `ChestBlock.getStateForPlacement` into `getChestType`: a chest put down beside a single
    // chest facing the same way becomes half of a double one there and then. Nothing else pairs them:
    // `updateShape` only ever *completes* a pair somebody already started.
    if (std::ranges::find(reshapes::chestBlocks(), path) != reshapes::chestBlocks().end() && world != nullptr) {
        const auto facing = properties.find("facing");
        const std::string chosen =
            facing == properties.end() ? std::string{nameOf(opposite(playerDirection(placement.yaw)))} : facing->second;
        properties["facing"] = chosen;
        properties["type"] = chestType(*world, x, y, z, block, fromName(chosen).value_or(Direction::North));
    }
    // parity: `NoteBlock.getStateForPlacement`, which takes its instrument from what it lands on.
    if (path == "note_block" && world != nullptr) {
        properties["instrument"] = std::string{noteBlocks::instrumentAt(*world, x, y, z).serializedName()};
    }
    // parity: `ScaffoldingBlock.getStateForPlacement`, which counts its distance as it lands.
    if (path == "scaffolding" && world != nullptr) {
        const std::int32_t distance = scaffoldDistance(*world, x, y, z);
        properties["distance"] = std::to_string(distance);
        properties["bottom"] = distance > 0 && pathOf(stateAt(*world, x, y - 1, z)) != "scaffolding" ? "true" : "false";
    }
    // parity: `BellBlock.getStateForPlacement`: hung, stood or bolted to one wall or two.
    if (path == "bell") {
        const bool onWall = placement.face != Direction::Up && placement.face != Direction::Down;
        if (placement.face == Direction::Down) {
            properties["attachment"] = "ceiling";
        } else if (!onWall) {
            properties["attachment"] = "floor";
        } else if (world != nullptr && bothWalls(*world, x, y, z, placement.face)) {
            properties["attachment"] = "double_wall";
        } else {
            properties["attachment"] = "single_wall";
        }
        if (onWall) {
            properties["facing"] = nameOf(opposite(placement.face));
        }
    }
    // parity: `ShelfMushroomBlock.getStateForPlacement`: the first horizontal looking direction with a
    // sturdy face to hang from, the mushroom facing back out of it.
    if (path == "shelf_mushroom" && world != nullptr) {
        for (const Direction looking : nearestLookingDirections(placement.yaw, placement.pitch, placement.face)) {
            if (looking == Direction::Up || looking == Direction::Down) {
                continue;
            }
            const Direction facing = opposite(looking);
            const std::int32_t support = stateAt(*world, x + stepX(looking), y, z + stepZ(looking));
            if (registry::blockShapes::isFaceSturdy(support, static_cast<registry::BlockFace>(facing))) {
                properties["facing"] = nameOf(facing);
                break;
            }
        }
    }
    // parity: `MangrovePropaguleBlock`, which a player always plants grown.
    if (path == "mangrove_propagule") {
        properties["age"] = "4";
    }
    // parity: `GrowingPlantHeadBlock.getStateForPlacement`, a random age out of twenty-five.
    if (std::ranges::find(kGrowingHeads, path) != kGrowingHeads.end()) {
        properties["age"] = std::to_string(random.nextInt(kGrowingAges));
    }
    // parity: `SpeleothemBlock.getStateForPlacement`: it points away from the face it was hung on,
    // and the reshape that follows works out how thick it is.
    if (!definition.valuesOf("vertical_direction").empty()) {
        properties["vertical_direction"] = placement.face == Direction::Down ? "down" : "up";
        properties["thickness"] = "tip";
    }
    // parity: `CrafterBlock.getStateForPlacement`, whose front is where the player is not.
    if (!definition.valuesOf("orientation").empty()) {
        const Direction front = opposite(nearestLooking(placement.yaw, placement.pitch));
        Direction top = Direction::Up;
        if (front == Direction::Down) {
            top = opposite(playerDirection(placement.yaw));
        } else if (front == Direction::Up) {
            top = playerDirection(placement.yaw);
        }
        properties["orientation"] = std::string{nameOf(front)} + "_" + std::string{nameOf(top)};
    }
    // parity: `BambooStalkBlock.getStateForPlacement`, the one that answers a different block: bamboo
    // planted on the ground is a sapling, and on bamboo it copies how grown that is.
    if (path == "bamboo" && world != nullptr) {
        const std::int32_t below = stateAt(*world, x, y - 1, z);
        const std::string belowPath = pathOf(below);
        std::int32_t age = 0;
        if (belowPath == "bamboo_sapling") {
            age = 0;
        } else if (belowPath == "bamboo") {
            age = numberOf(propertyOf(below, "age")).value_or(0) > 0 ? 1 : 0;
        } else {
            const std::int32_t above = stateAt(*world, x, y + 1, z);
            if (pathOf(above) != "bamboo") {
                return registry::blockStateDefinitions::stateIdOf(wire::common::Identifier::vanilla("bamboo_sapling"),
                                                                  {}, std::nullopt);
            }
            age = numberOf(propertyOf(above, "age")).value_or(0);
        }
        properties["age"] = std::to_string(age);
    }
    return std::nullopt;
}

} // namespace

Direction playerDirection(float yaw) {
    const auto quarter = static_cast<std::int32_t>(std::floor((yaw / kQuarterTurn) + kHalf));
    return kHorizontal[static_cast<std::size_t>(quarter & 3)];
}

Direction nearestLooking(float yaw, float pitch) {
    const double yawRadians = static_cast<double>(yaw) * std::numbers::pi / kHalfTurn;
    const double pitchRadians = static_cast<double>(pitch) * std::numbers::pi / kHalfTurn;
    const double x = -std::sin(yawRadians) * std::cos(pitchRadians);
    const double y = -std::sin(pitchRadians);
    const double z = std::cos(yawRadians) * std::cos(pitchRadians);
    if (std::abs(y) >= std::abs(x) && std::abs(y) >= std::abs(z)) {
        return y > 0 ? Direction::Up : Direction::Down;
    }
    if (std::abs(x) >= std::abs(z)) {
        return x > 0 ? Direction::East : Direction::West;
    }
    return z > 0 ? Direction::South : Direction::North;
}

std::array<Direction, 6> nearestLookingDirections(float yaw, float pitch, Direction clickedFace) {
    // parity: `Direction.orderedByNearest`, over `Mth.sin`'s table as vanilla works it out.
    constexpr float kDegreesToRadians = std::numbers::pi_v<float> / static_cast<float>(kHalfTurn);
    const float pitchRadians = pitch * kDegreesToRadians;
    const float yawRadians = -yaw * kDegreesToRadians;
    const float pitchSin = tableSin(pitchRadians, 0);
    const float pitchCos = tableSin(pitchRadians, kQuarterTable);
    const float yawSin = tableSin(yawRadians, 0);
    const float yawCos = tableSin(yawRadians, kQuarterTable);
    const bool xPositive = yawSin > 0.0F;
    const bool yPositive = pitchSin < 0.0F;
    const bool zPositive = yawCos > 0.0F;
    const float xYaw = xPositive ? yawSin : -yawSin;
    const float yMagnitude = yPositive ? -pitchSin : pitchSin;
    const float zYaw = zPositive ? yawCos : -yawCos;
    const float xMagnitude = xYaw * pitchCos;
    const float zMagnitude = zYaw * pitchCos;
    const Direction axisX = xPositive ? Direction::East : Direction::West;
    const Direction axisY = yPositive ? Direction::Up : Direction::Down;
    const Direction axisZ = zPositive ? Direction::South : Direction::North;
    const auto ordered = [](Direction first, Direction second, Direction third) {
        return std::array<Direction, 6>{first, second, third, opposite(third), opposite(second), opposite(first)};
    };
    std::array<Direction, 6> directions{};
    if (xYaw > zYaw) {
        if (yMagnitude > xMagnitude) {
            directions = ordered(axisY, axisX, axisZ);
        } else {
            directions = zMagnitude > yMagnitude ? ordered(axisX, axisZ, axisY) : ordered(axisX, axisY, axisZ);
        }
    } else if (yMagnitude > zMagnitude) {
        directions = ordered(axisY, axisZ, axisX);
    } else {
        directions = xMagnitude > yMagnitude ? ordered(axisZ, axisX, axisY) : ordered(axisZ, axisY, axisX);
    }
    // parity: `BlockPlaceContext.getNearestLookingDirections` moves the clicked block's direction to
    // the front, which is the placement case every item use here is.
    const auto into = std::ranges::find(directions, opposite(clickedFace));
    std::rotate(directions.begin(), into, std::next(into));
    return directions;
}

std::int32_t stateFor(const wire::common::Identifier& block, std::int32_t givenDefault, const Placement& placement,
                      random::RandomSource& random) {
    // A block has two numbers that look alike: its registry id and its default state id. Handed the
    // wrong one this used to answer a state belonging to some other block entirely, and the caller
    // had no way to tell. The block itself is the authority here, so the number is checked against
    // it and replaced when it does not fit.
    const wire::common::Identifier* owner = registry::blockStateDefinitions::blockOf(givenDefault);
    const std::int32_t defaultStateId = owner != nullptr && *owner == block
                                            ? givenDefault
                                            : registry::blocks::defaultStateIdOf(block).value_or(givenDefault);
    // parity: `ConcretePowderBlock.getStateForPlacement`, the third site that asks the same question
    // as the shape update and the landing: the square still holds the water being replaced when it
    // is asked, which is how anybody actually makes concrete. This goes before everything else
    // because concrete powder has no properties at all, so the state table has no entry to orient
    // and the whole rest of this function is skipped for it.
    if (block.path().ends_with("_concrete_powder") && placement.world != nullptr &&
        (registry::blockFluids::hasWaterFluid(placement.replacing) ||
         reshapes::touchesWater(*placement.world, placement.x, placement.y, placement.z))) {
        if (const std::optional<std::int32_t> set = reshapes::concreteFor(block); set.has_value()) {
            return *set;
        }
    }
    const registry::blockStateDefinitions::Definition* definition = registry::blockStateDefinitions::of(block);
    if (definition == nullptr) {
        return defaultStateId;
    }
    const std::string path{block.path()};
    const Direction face = placement.face;
    std::map<std::string, std::string> properties;

    if (definition->valuesOf("axis").size() == 3) {
        properties["axis"] = axisOf(face);
    }

    // parity: `FaceAttachedHorizontalDirectionalBlock`, the buttons, levers and grindstones: what
    // they hang on comes from the face that was clicked, and only then which way they look.
    const bool attached = !definition->valuesOf("face").empty();
    if (attached) {
        if (face == Direction::Up) {
            properties["face"] = "floor";
        } else if (face == Direction::Down) {
            properties["face"] = "ceiling";
        } else {
            properties["face"] = "wall";
        }
    }

    const std::size_t facings = definition->valuesOf("facing").size();
    if (facings == 6) {
        // parity: `ObserverBlock.getStateForPlacement` is
        // `getNearestLookingDirection().getOpposite().getOpposite()`, two opposites that cancel, so
        // alone among the six-way blocks an observer faces **away** from whoever put it down and
        // watches the square beyond. A piston, a dispenser, a barrel and a command block all take the
        // single opposite and face back. Every observer placed by hand used to look the wrong way.
        const Direction looking = nearestLooking(placement.yaw, placement.pitch);
        properties["facing"] = named(path, kClickedFaceBlocks) ? nameOf(face)
                               : path == "observer"            ? nameOf(looking)
                                                               : nameOf(opposite(looking));
    } else if (facings == 4 && path.contains("wall") && face != Direction::Up && face != Direction::Down) {
        properties["facing"] = nameOf(face);
    } else if (facings == 4 && attached) {
        properties["facing"] = properties["face"] == "wall" ? nameOf(face) : nameOf(playerDirection(placement.yaw));
    } else if (facings == 4 && path.ends_with("anvil")) {
        // parity: `AnvilBlock`, which lands sideways to whoever put it down.
        properties["facing"] = nameOf(clockwise(playerDirection(placement.yaw)));
    } else if (facings == 4) {
        const bool towardsPlayer = endsWithAny(path, kPlayerFacingSuffixes) || named(path, kPlayerFacingBlocks);
        properties["facing"] =
            towardsPlayer ? nameOf(playerDirection(placement.yaw)) : nameOf(opposite(playerDirection(placement.yaw)));
    }

    // parity: the same test in `StairBlock`, `SlabBlock` and `TrapDoorBlock`: the top half only when
    // the click was on a ceiling, or above the middle of a side.
    const bool upsideDown = face == Direction::Down || (face != Direction::Up && placement.clickY > kHalf);
    const std::vector<std::string>& halves = definition->valuesOf("half");
    if (std::ranges::find(halves, "top") != halves.end()) {
        properties["half"] = upsideDown ? "top" : "bottom";
    }
    const std::vector<std::string>& types = definition->valuesOf("type");
    if (std::ranges::find(types, "double") != types.end()) {
        const wire::common::Identifier* replacingBlock = registry::blockStateDefinitions::blockOf(placement.replacing);
        if (replacingBlock != nullptr && *replacingBlock == block) {
            properties["type"] = "double";
        } else {
            properties["type"] = upsideDown ? "top" : "bottom";
        }
    }

    // parity: every `SimpleWaterloggedBlock`'s `getStateForPlacement`, which is the same line in all
    // twenty-odd of them: the block keeps the water it was put into instead of eating it.
    if (!definition->valuesOf("waterlogged").empty()) {
        properties["waterlogged"] = isWater(placement.replacing) ? "true" : "false";
    }

    // parity: `BaseRailBlock.getStateForPlacement`: a rail lies along the axis the player is looking
    // down, and the connection pass afterwards is what bends it towards its neighbours.
    const std::vector<std::string>& shapes = definition->valuesOf("shape");
    if (std::ranges::find(shapes, "north_south") != shapes.end()) {
        const Direction looking = playerDirection(placement.yaw);
        properties["shape"] = looking == Direction::East || looking == Direction::West ? "east_west" : "north_south";
    }

    // parity: `LeavesBlock.getStateForPlacement`. Leaves a player puts down never decay, which is the
    // whole reason the property exists.
    if (!definition->valuesOf("persistent").empty()) {
        properties["persistent"] = "true";
    }

    // parity: sixteen segments of the yaw. `StandingSignBlock` and `BannerBlock` turn it around so the
    // face looks back at whoever placed it; `SkullBlock` does not, because a head looks the same way
    // the player does.
    if (definition->valuesOf("rotation").size() == kSegments) {
        const bool head = path.ends_with("_head") || path.ends_with("_skull");
        properties["rotation"] =
            std::to_string(segmentOf(head ? placement.yaw : placement.yaw + static_cast<float>(kHalfTurn)));
    }

    // parity: `LanternBlock.getStateForPlacement`, which hangs one put against a ceiling.
    if (!definition->valuesOf("hanging").empty()) {
        properties["hanging"] = face == Direction::Down ? "true" : "false";
    }

    // parity: `CandleBlock`, `SeaPickleBlock` and `TurtleEggBlock`, which stack another one into the
    // block that is already there rather than refusing the placement.
    const wire::common::Identifier* replacingBlock = registry::blockStateDefinitions::blockOf(placement.replacing);
    if (replacingBlock != nullptr && *replacingBlock == block) {
        for (const std::string_view name : kStacking) {
            const std::vector<std::string>& values = definition->valuesOf(name);
            if (values.empty()) {
                continue;
            }
            const std::optional<std::int32_t> had = numberOf(propertyOf(placement.replacing, std::string{name}));
            if (!had.has_value()) {
                continue;
            }
            properties[std::string{name}] =
                std::to_string(std::min(*had + 1, static_cast<std::int32_t>(values.size())));
        }
    }

    // parity: `DoorBlock.getHinge`, which counts the solid blocks either side and falls back to which
    // half of the block was clicked.
    if (placement.world != nullptr && !definition->valuesOf("hinge").empty()) {
        const auto facing = properties.find("facing");
        const Direction towards = facing == properties.end()
                                      ? playerDirection(placement.yaw)
                                      : fromName(facing->second).value_or(playerDirection(placement.yaw));
        properties["hinge"] = hingeOf(*placement.world, placement.x, placement.y, placement.z, towards,
                                      placement.clickX, placement.clickZ);
    }

    // The blocks whose own `getStateForPlacement` does something the rules above cannot say.
    if (const std::optional<std::int32_t> answered = special(block, *definition, properties, placement, random);
        answered.has_value()) {
        return *answered;
    }

    if (properties.empty()) {
        return defaultStateId;
    }
    return registry::blockStateDefinitions::stateIdOf(block, properties, defaultStateId).value_or(defaultStateId);
}

bool isObstructed(std::int32_t stateId, std::int32_t blockX, std::int32_t blockY, std::int32_t blockZ,
                  std::span<const Box> boxes) {
    if (boxes.empty() || registry::blockShapes::isEmptyShape(stateId)) {
        return false;
    }
    const std::span<const std::int32_t> shape = registry::blockShapes::collisionBoxes(stateId);
    for (std::size_t at = 0; at + 5 < shape.size(); at += 6) {
        const double units = registry::blockShapes::kUnits;
        const double minX = blockX + (shape[at] / units);
        const double minY = blockY + (shape[at + 1] / units);
        const double minZ = blockZ + (shape[at + 2] / units);
        const double maxX = blockX + (shape[at + 3] / units);
        const double maxY = blockY + (shape[at + 4] / units);
        const double maxZ = blockZ + (shape[at + 5] / units);
        for (const Box& other : boxes) {
            if (minX < other[3] && maxX > other[0] && minY < other[4] && maxY > other[1] && minZ < other[5] &&
                maxZ > other[2]) {
                return true;
            }
        }
    }
    return false;
}

Box playerBox(double x, double y, double z, bool sneaking) {
    const double half = kPlayerWidth / 2;
    return Box{x - half, y, z - half, x + half, y + (sneaking ? kSneakingHeight : kPlayerHeight), z + half};
}

} // namespace basalt::server::runtime::world::blockPlacement
