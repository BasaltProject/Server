#include <basalt/server/registry/BlockConnections.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/BlockEntityDefaults.hpp>
#include <basalt/server/runtime/world/BlockInteractions.hpp>
#include <basalt/server/runtime/world/BlockSurvival.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/Composters.hpp>
#include <basalt/server/runtime/world/CopperChests.hpp>
#include <basalt/server/runtime/world/Crafters.hpp>
#include <basalt/server/runtime/world/Dispensers.hpp>
#include <basalt/server/runtime/world/FallingBlocks.hpp>
#include <basalt/server/runtime/world/Fires.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/Lightning.hpp>
#include <basalt/server/runtime/world/NeighbourUpdates.hpp>
#include <basalt/server/runtime/world/NoteBlocks.hpp>
#include <basalt/server/runtime/world/Pistons.hpp>
#include <basalt/server/runtime/world/Rails.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/RedstoneWire.hpp>
#include <basalt/server/runtime/world/ScheduledBlocks.hpp>
#include <basalt/server/runtime/world/Shelves.hpp>
#include <basalt/server/runtime/world/SnowyBlocks.hpp>
#include <basalt/server/runtime/world/Sponges.hpp>
#include <basalt/server/runtime/world/Tripwires.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/runtime/world/Walls.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <deque>
#include <string>
#include <utility>

namespace basalt::server::runtime::world::neighbourUpdates {
namespace {

using registry::BlockFace;

/// parity: `LeavesBlock.DECAY_DISTANCE`.
constexpr std::int32_t kDecayDistance = 7;

constexpr std::array<BlockFace, 6> kFaces{BlockFace::Down,  BlockFace::Up,   BlockFace::North,
                                          BlockFace::South, BlockFace::West, BlockFace::East};

[[nodiscard]] std::int32_t axisOfName(std::string_view name) {
    if (name == "up" || name == "down") {
        return 1;
    }
    return name == "north" || name == "south" ? 2 : 0;
}

/// parity: `NeighborUpdater.UPDATE_ORDER`.
constexpr std::array<BlockFace, 6> kUpdateOrder{BlockFace::West, BlockFace::East,  BlockFace::Down,
                                                BlockFace::Up,   BlockFace::North, BlockFace::South};

/// parity: `BlockBehaviour.UPDATE_SHAPE_ORDER`, which is neither of the other two.
constexpr std::array<BlockFace, 6> kUpdateShapeOrder{BlockFace::West,  BlockFace::East, BlockFace::North,
                                                     BlockFace::South, BlockFace::Down, BlockFace::Up};

constexpr std::array<std::pair<BlockFace, std::string_view>, 4> kHorizontal{
    {{BlockFace::North, "north"}, {BlockFace::South, "south"}, {BlockFace::West, "west"}, {BlockFace::East, "east"}}};

/// parity: `Direction.Plane.HORIZONTAL`.
constexpr std::array<BlockFace, 4> kFlat{BlockFace::North, BlockFace::East, BlockFace::South, BlockFace::West};

using Position = std::array<std::int32_t, 3>;

void noop(std::int32_t, std::int32_t, std::int32_t, std::int32_t) {}

[[nodiscard]] bool sameBlock(const std::int32_t one, const std::int32_t other) {
    const wire::common::Identifier* first = registry::blockStateDefinitions::blockOf(one);
    const wire::common::Identifier* second = registry::blockStateDefinitions::blockOf(other);
    return first != nullptr && second != nullptr && *first == *second;
}

/// parity: `ObserverBlock.onPlace` only reaches out for one that arrives **powered**.
[[nodiscard]] bool isObserverState(const std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && block->path() == "observer";
}

[[nodiscard]] std::int32_t stateAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    return world.blockState(x, y, z).value_or(registry::blocks::air().stateId);
}

/// parity: `LeavesBlock.getOptionalDistanceAt`.
[[nodiscard]] std::int32_t distanceOf(std::int32_t stateId) {
    if (registry::blockConnections::preventsLeafDecay(stateId)) {
        return 0;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find("distance");
    if (found == properties.end()) {
        return kDecayDistance;
    }
    std::int32_t distance = 0;
    const char* first = found->second.data();
    const char* last = first + found->second.size();
    if (std::from_chars(first, last, distance).ec != std::errc{}) {
        return kDecayDistance;
    }
    return distance;
}

[[nodiscard]] std::optional<std::int32_t> rewrite(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                  std::int32_t stateId,
                                                  const std::map<std::string, std::string>& wanted) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> reshaped = registry::blockStateDefinitions::stateIdOf(*block, wanted, stateId);
    if (!reshaped.has_value() || *reshaped == stateId || !world.setBlockState(x, y, z, *reshaped)) {
        return std::nullopt;
    }
    return reshaped;
}

/// parity: `LeavesBlock.updateDistance`.
[[nodiscard]] std::optional<std::int32_t> retreeLeaves(WorldState& world, std::int32_t x, std::int32_t y,
                                                       std::int32_t z, std::int32_t stateId,
                                                       std::optional<BlockFace> towards) {
    // parity: `LeavesBlock.updateShape` does **not** rewrite the leaves.
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    // parity: the jar reads the **one** neighbour that moved, not the six, and books unless that.
    if (towards.has_value()) {
        const std::int32_t fromNeighbour =
            distanceOf(stateAt(world, x + registry::stepX(*towards), y + registry::stepY(*towards),
                               z + registry::stepZ(*towards))) +
            1;
        if (fromNeighbour != 1 || distanceOf(stateId) != fromNeighbour) {
            world.scheduleBlockTick(x, y, z, *block, 1);
        }
        return std::nullopt;
    }
    // parity: no direction is the `updateFromNeighbourShapes` case.
    for (const BlockFace face : kUpdateShapeOrder) {
        const std::int32_t fromNeighbour = distanceOf(stateAt(world, x + registry::stepX(face),
                                                              y + registry::stepY(face), z + registry::stepZ(face))) +
                                           1;
        if (fromNeighbour != 1 || distanceOf(stateId) != fromNeighbour) {
            world.scheduleBlockTick(x, y, z, *block, 1);
            break;
        }
    }
    return std::nullopt;
}

/// parity: `WallBlock.updateShape`, sides and post together.
[[nodiscard]] std::optional<std::int32_t> rebuildWall(const Context& context, std::int32_t x, std::int32_t y,
                                                      std::int32_t z, std::int32_t stateId) {
    WorldState& world = context.world;
    const std::int32_t north = stateAt(world, x, y, z - 1);
    const std::int32_t south = stateAt(world, x, y, z + 1);
    const std::int32_t west = stateAt(world, x - 1, y, z);
    const std::int32_t east = stateAt(world, x + 1, y, z);
    const std::int32_t above = stateAt(world, x, y + 1, z);
    const std::optional<std::map<std::string, std::string>> wanted =
        context.wallShape ? context.wallShape(stateId, north, south, west, east, above)
                          : walls::shapeOf(stateId, north, south, west, east, above);
    if (!wanted.has_value()) {
        return std::nullopt;
    }
    return rewrite(world, x, y, z, stateId, *wanted);
}

/// parity: `StairBlock.updateShape`, which is what turns a staircase's corners.
[[nodiscard]] std::optional<std::int32_t> turnStairs(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                     std::int32_t stateId) {
    const std::optional<std::string_view> shape =
        registry::blockConnections::stairShape(stateId, stateAt(world, x, y, z - 1), stateAt(world, x, y, z + 1),
                                               stateAt(world, x - 1, y, z), stateAt(world, x + 1, y, z));
    if (!shape.has_value()) {
        return std::nullopt;
    }
    return rewrite(world, x, y, z, stateId, {{"shape", std::string{*shape}}});
}

[[nodiscard]] bool breakIfUnsupported(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                                      const blockSurvival::ScaffoldingHasFallen& scaffolding, const OnBroken& onBroken,
                                      std::optional<BlockFace> towards = std::nullopt) {
    if (y < context.minY || y > context.maxY) {
        return false;
    }
    WorldState& world = context.world;
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || *stateId == registry::blocks::air().stateId) {
        return false;
    }
    // parity: `updateShape` asks.
    if (!blockSurvival::loosenedBy(*stateId, towards)) {
        return false;
    }
    if (blockSurvival::isSupported(world, x, y, z, *stateId, scaffolding, context.hasSkylight)) {
        return false;
    }
    // parity: `SugarCaneBlock.updateShape` and `CactusBlock.updateShape` book a turn instead of.
    const wire::common::Identifier* breaking = registry::blockStateDefinitions::blockOf(*stateId);
    const std::string_view breakingPath = breaking == nullptr ? std::string_view{} : breaking->path();
    if (scheduledBlocks::booksItsOwnBreak(breakingPath)) {
        scheduledBlocks::scheduleBreakIfLoose(world, x, y, z, false);
        return false;
    }
    // parity: `SpeleothemBlock.updateShape` books a turn too.
    if (breakingPath == "pointed_dripstone" || breakingPath == "sulfur_spike") {
        scheduledBlocks::scheduleSpeleothem(world, x, y, z, false);
        return false;
    }
    // parity: `ScaffoldingBlock.updateShape` never breaks anything either.
    if (breakingPath == "scaffolding") {
        return false;
    }
    world.setBlockState(x, y, z, registry::blocks::air().stateId);
    onBroken(*stateId, x, y, z);
    return true;
}

/// parity: `DiodeBlock.neighborChanged` and `RedStoneWireBlock.neighborChanged` open with `canSurvive`.
[[nodiscard]] std::optional<bool> dropUnsupportedComponent(const Context& context, const std::int32_t x,
                                                           const std::int32_t y, const std::int32_t z,
                                                           const OnBroken& onBroken) {
    WorldState& world = context.world;
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    const bool diode = block->path() == "repeater" || block->path() == "comparator";
    if (!diode && block->path() != "redstone_wire") {
        return std::nullopt;
    }
    if (blockSurvival::isSupported(world, x, y, z, *stateId, {}, context.hasSkylight)) {
        return std::nullopt;
    }
    world.setBlockState(x, y, z, registry::blocks::air().stateId);
    onBroken(*stateId, x, y, z);
    return diode;
}

/// parity: `SideChainPartBlock.isBeingUpdatedByNeighbor`'s second half.
[[nodiscard]] bool wasConnectedShelf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr || !shelves::isShelf(block->path())) {
        return false;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto part = properties.find("side_chain");
    return part != properties.end() && part->second != "unconnected";
}

/// parity: `BlockBehaviour.BlockStateBase.updateNeighbourShapes`.
void shapeAround(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z, const OnReshaped& reshaped) {
    for (const BlockFace face : kUpdateShapeOrder) {
        const std::int32_t nx = x + registry::stepX(face);
        const std::int32_t ny = y + registry::stepY(face);
        const std::int32_t nz = z + registry::stepZ(face);
        if (const std::optional<std::int32_t> drawn = reshape(context, nx, ny, nz, registry::opposite(face));
            drawn.has_value()) {
            reshaped(drawn.value(), nx, ny, nz);
        }
    }
}

struct Walk {
    const Context& context;
    const blockSurvival::ScaffoldingHasFallen& scaffolding;
    const OnReshaped& onReshaped;
    const OnBroken& onBroken;
};

/// parity: `SeagrassBlock`.
[[nodiscard]] bool plantBooksWater(const Context& context, const std::int32_t x, const std::int32_t y,
                                   const std::int32_t z, const std::string_view path, const BlockFace towards) {
    // parity: `SeagrassBlock.updateShape` books unless the shape update left it air.
    if (path == "seagrass") {
        return blockSurvival::isSupported(context.world, x, y, z, std::nullopt, {}, context.hasSkylight);
    }
    const bool head = path == "kelp";
    if (!head && path != "kelp_plant") {
        return false;
    }
    const std::optional<std::int32_t> above = context.world.blockState(x, y + 1, z);
    const wire::common::Identifier* grown =
        above.has_value() ? registry::blockStateDefinitions::blockOf(*above) : nullptr;
    const bool kelpAbove = grown != nullptr && (grown->path() == "kelp" || grown->path() == "kelp_plant");
    if (head) {
        // parity: news from below turns a head into a body when the column carries on above, and that.
        if (towards == BlockFace::Down && kelpAbove &&
            blockSurvival::isSupported(context.world, x, y, z, std::nullopt, {}, context.hasSkylight)) {
            return false;
        }
        // The head converts when the square above **is** kelp, so it books when it is not.
        return towards != BlockFace::Up || !kelpAbove;
    }
    // parity: and the body converts back into a head when the square above stops being kelp, which is.
    return towards != BlockFace::Up || kelpAbove;
}

/// parity: `LiquidBlock.updateShape` books the fluid's turn when either square is a source.
void bookLiquidTurn(const Context& context, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                    const BlockFace towards) {
    const std::optional<std::int32_t> here = context.world.blockState(x, y, z);
    if (!here.has_value()) {
        return;
    }
    const std::optional<FluidKind> kind = fluidFlow::liquidBlockKindOf(*here);
    if (!kind.has_value()) {
        // parity: two families book water on a shape update with no condition at all.
        const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*here);
        const auto logged = properties.find("waterlogged");
        const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*here);
        const bool column = block != nullptr && block->path() == "bubble_column";
        if (column || (logged != properties.end() && logged->second == "true") ||
            (block != nullptr && plantBooksWater(context, x, y, z, block->path(), towards))) {
            context.world.scheduleFluidTick(x, y, z, fluidId(FluidKind::Water, true), fluidFlow::kWaterTickDelay);
        }
        return;
    }
    const std::optional<std::int32_t> from = context.world.blockState(
        x + registry::stepX(towards), y + registry::stepY(towards), z + registry::stepZ(towards));
    const bool sourceHere = isSourceFluid(*here);
    if (!sourceHere && !(from.has_value() && isSourceFluid(*from))) {
        return;
    }
    const std::int32_t delay = *kind == FluidKind::Water ? fluidFlow::kWaterTickDelay
                               : context.ultrawarm       ? fluidFlow::kFastLavaTickDelay
                                                         : fluidFlow::kLavaTickDelay;
    context.world.scheduleFluidTick(x, y, z, fluidId(*kind, sourceHere), delay);
}

void visit(const Walk& walk, std::deque<Position>& pending, std::int32_t x, std::int32_t y, std::int32_t z,
           BlockFace towards) {
    fallingBlocks::scheduleIfFalling(walk.context.world, x, y, z, fallingBlocks::Asked::updateShape);
    bookLiquidTurn(walk.context, x, y, z, towards);
    bool changed = false;
    const std::optional<std::int32_t> before = walk.context.world.blockState(x, y, z);
    if (const std::optional<std::int32_t> reshaped = reshape(walk.context, x, y, z, towards); reshaped.has_value()) {
        // parity: `Block.updateOrDestroy`, which is what runs a shape update's answer: a state is.
        if (*reshaped == registry::blocks::air().stateId && before.has_value() &&
            *before != registry::blocks::air().stateId) {
            walk.onBroken(*before, x, y, z);
        } else {
            walk.onReshaped(*reshaped, x, y, z);
        }
        changed = true;
    }
    if (breakIfUnsupported(walk.context, x, y, z, walk.scaffolding, walk.onBroken, towards) || changed) {
        pending.push_back({x, y, z});
    }
}

} // namespace

std::optional<BlockChange> leafTick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || !registry::blockConnections::isLeaves(*stateId)) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> written =
        rewrite(world, x, y, z, *stateId, {{"distance", std::to_string(leafDistance(world, x, y, z))}});
    if (!written.has_value()) {
        return std::nullopt;
    }
    return BlockChange{.x = x, .y = y, .z = z, .stateId = *written};
}

std::int32_t leafDistance(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    std::int32_t nearest = kDecayDistance;
    for (const BlockFace face : kFaces) {
        const std::int32_t neighbour =
            stateAt(world, x + registry::stepX(face), y + registry::stepY(face), z + registry::stepZ(face));
        nearest = std::min(nearest, distanceOf(neighbour) + 1);
        if (nearest == 1) {
            return 1;
        }
    }
    return nearest;
}

std::optional<std::int32_t> connect(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                                    std::int32_t stateId, std::optional<BlockFace> towards) {
    WorldState& world = context.world;
    if (registry::blockConnections::isLeaves(stateId)) {
        return retreeLeaves(world, x, y, z, stateId, towards);
    }
    if (registry::blockConnections::isStairs(stateId)) {
        return turnStairs(world, x, y, z, stateId);
    }
    if (registry::blockConnections::isWall(stateId)) {
        return rebuildWall(context, x, y, z, stateId);
    }
    if (!registry::blockConnections::connects(stateId)) {
        return std::nullopt;
    }
    std::map<std::string, std::string> wanted;
    for (const auto& [face, name] : kHorizontal) {
        // parity: `FenceBlock` and `IronBarsBlock` redraw only the side the news came from; no.
        if (towards.has_value() && *towards != face) {
            continue;
        }
        const std::int32_t neighbour =
            stateAt(world, x + registry::stepX(face), y + registry::stepY(face), z + registry::stepZ(face));
        wanted[std::string{name}] = registry::blockConnections::connectsTo(stateId, neighbour, face) ? "true" : "false";
    }
    if (wanted.empty()) {
        return std::nullopt;
    }
    return rewrite(world, x, y, z, stateId, wanted);
}

std::int32_t validState(const Context& context, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                        const std::int32_t stateId) {
    WorldState& world = context.world;
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return stateId;
    }
    std::map<std::string, std::string> wanted;
    if (registry::blockConnections::isStairs(stateId)) {
        // parity: `StairBlock.updateShape`.
        const std::optional<std::string_view> shape =
            registry::blockConnections::stairShape(stateId, stateAt(world, x, y, z - 1), stateAt(world, x, y, z + 1),
                                                   stateAt(world, x - 1, y, z), stateAt(world, x + 1, y, z));
        if (shape.has_value()) {
            wanted["shape"] = std::string{*shape};
        }
    } else if (registry::blockConnections::isWall(stateId)) {
        // parity: `WallBlock.updateShape`.
        const std::optional<std::map<std::string, std::string>> sides =
            context.wallShape
                ? context.wallShape(stateId, stateAt(world, x, y, z - 1), stateAt(world, x, y, z + 1),
                                    stateAt(world, x - 1, y, z), stateAt(world, x + 1, y, z),
                                    stateAt(world, x, y + 1, z))
                : walls::shapeOf(stateId, stateAt(world, x, y, z - 1), stateAt(world, x, y, z + 1),
                                 stateAt(world, x - 1, y, z), stateAt(world, x + 1, y, z), stateAt(world, x, y + 1, z));
        if (sides.has_value()) {
            wanted = *sides;
        }
    } else if (registry::blockConnections::connects(stateId)) {
        // parity: `CrossCollisionBlock.updateShape`.
        for (const auto& [face, name] : kHorizontal) {
            const std::int32_t neighbour =
                stateAt(world, x + registry::stepX(face), y + registry::stepY(face), z + registry::stepZ(face));
            wanted[std::string{name}] =
                registry::blockConnections::connectsTo(stateId, neighbour, face) ? "true" : "false";
        }
    }
    if (wanted.empty()) {
        return stateId;
    }
    return registry::blockStateDefinitions::stateIdOf(*block, wanted, stateId).value_or(stateId);
}

std::optional<std::int32_t> reshape(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                                    std::optional<BlockFace> towards) {
    WorldState& world = context.world;
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    // parity: the blocks whose `updateShape` books a turn and changes nothing now.
    if (towards == BlockFace::Up) {
        scheduledBlocks::scheduleCoveredSoil(world, x, y, z);
    }
    scheduledBlocks::scheduleCoralDeath(world, x, y, z, context.random.nextLong());
    scheduledBlocks::scheduleCreakingHeart(world, x, y, z);
    // parity: `ScaffoldingBlock.updateShape` answers the state it was given and books a turn for the.
    scheduledBlocks::scheduleScaffolding(world, x, y, z);
    // A column is more than one block, so it reports through the caller rather than as one state.
    scheduledBlocks::scheduleBubbleColumn(world, x, y, z, towards);
    // parity: `NoteBlock.updateShape` rewrites the instrument **only** when the news comes from above.
    if (!towards.has_value() || registry::axisOf(*towards) == 1) {
        if (const std::optional<std::int32_t> tuned = noteBlocks::retune(world, x, y, z, *stateId); tuned.has_value()) {
            return tuned;
        }
    }
    // parity: `SnowyBlock.updateShape`, the white top grass wears while snow sits on it.
    if (const std::optional<std::int32_t> snowy = snowyBlocks::updated(world, x, y, z); snowy.has_value()) {
        if (world.setBlockState(x, y, z, *snowy)) {
            return snowy;
        }
    }
    // parity: `RepeaterBlock.updateShape`.
    const std::map<std::string, std::string> diodeProperties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto diodeFacing = diodeProperties.find("facing");
    if (!towards.has_value() || diodeFacing == diodeProperties.end() ||
        registry::axisOf(*towards) != axisOfName(diodeFacing->second)) {
        if (const std::optional<std::int32_t> held = redstoneComponents::relock(world, x, y, z); held.has_value()) {
            return held;
        }
    }
    // parity: `TripWireHookBlock.updateShape`.
    if (towards.has_value() && tripwires::hookFalls(world, x, y, z, *stateId, *towards)) {
        return registry::blocks::air().stateId;
    }
    // parity: `BellBlock.updateShape`.
    if (towards.has_value()) {
        if (const std::optional<std::int32_t> rung = blockInteractions::bellShape(world, x, y, z, *stateId, *towards);
            rung.has_value()) {
            return rung;
        }
    }
    // parity: `TripWireBlock.updateShape`.
    if (const std::optional<std::int32_t> strung = tripwires::reshape(world, x, y, z, *stateId, towards);
        strung.has_value()) {
        return strung;
    }
    // parity: `RedStoneWireBlock.updateShape`, which is what draws a line of dust as a line.
    if (const std::optional<std::int32_t> drawn = context.wireReshape
                                                      ? context.wireReshape(world, x, y, z, *stateId, towards)
                                                      : redstoneWire::reshape(world, x, y, z, *stateId, towards);
        drawn.has_value()) {
        return drawn;
    }
    if (const std::optional<std::int32_t> other = reshapes::of(world, x, y, z, *stateId, context.geyser, towards);
        other.has_value()) {
        return other;
    }
    return connect(context, x, y, z, *stateId, towards);
}

namespace {

/// parity: `ObserverBlock.updateNeighborsInFront`.
void tellInFront(const Context& context, const std::int32_t stateId, const std::int32_t x, const std::int32_t y,
                 const std::int32_t z, const OnReshaped& onReshaped) {
    WorldState& world = context.world;
    for (const redstoneComponents::Reach& at : redstoneComponents::outputReach(stateId, x, y, z)) {
        if (!at.skip.has_value()) {
            continue;
        }
        redstoneComponents::scheduleInFront(world, context.worldName, at.x, at.y, at.z, *at.skip);
        for (const BlockChange& change :
             redstoneReactors::updateInFront(world, context.worldName, at.x, at.y, at.z, *at.skip, context.gameTime,
                                             context.random, context.tntExplodes)) {
            if (world.setBlockState(change.x, change.y, change.z, change.stateId)) {
                (onReshaped ? onReshaped : OnReshaped{noop})(change.stateId, change.x, change.y, change.z);
                // parity: the reactor wrote with flag two, which still walks the shapes around it.
                reshapeOnly(context, change.x, change.y, change.z, {}, onReshaped);
            }
        }
    }
}

} // namespace

void settleArrivedSignal(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                         const std::optional<std::int32_t> removed, const OnReshaped& onReshaped) {
    WorldState& world = context.world;
    // parity: `SculkSensorBlock.onPlace` and `TargetBlock.onPlace`.
    const std::optional<std::int32_t> arrived = world.blockState(x, y, z);
    if (!arrived.has_value()) {
        return;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*arrived);
    if (block == nullptr) {
        return;
    }
    const std::string_view path = block->path();
    // parity: four of the five below open with `!state.is(oldState.getBlock())`.
    const bool blockStayed = removed.has_value() && sameBlock(*removed, *arrived);
    // parity: `ComposterBlock.onPlace`.
    if (path == "composter") {
        const std::map<std::string, std::string> heap = registry::blockStateDefinitions::propertiesOf(*arrived);
        const auto level = heap.find("level");
        if (level != heap.end() && level->second == std::to_string(composters::kFull)) {
            world.scheduleBlockTick(x, y, z, *block, composters::kReadyDelay);
        }
        return;
    }
    if (blockStayed) {
        return;
    }
    // parity: `LightningRodBlock.onPlace`.
    if (path.ends_with("lightning_rod")) {
        const std::map<std::string, std::string> rod = registry::blockStateDefinitions::propertiesOf(*arrived);
        const auto powered = rod.find("powered");
        if (powered != rod.end() && powered->second == "true" && !world.hasScheduledBlockTick(x, y, z, *block)) {
            world.scheduleBlockTick(x, y, z, *block, lightning::kRodTicks);
        }
        return;
    }
    // parity: `ObserverBlock.onPlace`.
    if (path == "observer") {
        const std::map<std::string, std::string> watcher = registry::blockStateDefinitions::propertiesOf(*arrived);
        const auto powered = watcher.find("powered");
        if (powered == watcher.end() || powered->second != "true" || world.hasScheduledBlockTick(x, y, z, *block)) {
            return;
        }
        if (const std::optional<std::int32_t> quiet =
                registry::blockStateDefinitions::stateIdOf(*block, {{"powered", "false"}}, *arrived);
            quiet.has_value() && world.setBlockState(x, y, z, *quiet)) {
            (onReshaped ? onReshaped : OnReshaped{noop})(*quiet, x, y, z);
            // parity: that `onPlace` ends in `updateNeighborsInFront`, and the write it makes first.
            tellInFront(context, *quiet, x, y, z, onReshaped);
        }
        return;
    }
    if (path != "target" && path != "sculk_sensor" && path != "calibrated_sculk_sensor") {
        return;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*arrived);
    const auto power = properties.find("power");
    if (power == properties.end() || power->second == "0" || world.hasScheduledBlockTick(x, y, z, *block)) {
        return;
    }
    if (const std::optional<std::int32_t> quiet =
            registry::blockStateDefinitions::stateIdOf(*block, {{"power", "0"}}, *arrived);
        quiet.has_value() && world.setBlockState(x, y, z, *quiet)) {
        (onReshaped ? onReshaped : OnReshaped{noop})(*quiet, x, y, z);
    }
}

void reshapeIndirect(const Context& context, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                     const std::int32_t stateId, const OnReshaped& onReshaped) {
    WorldState& world = context.world;
    const OnReshaped reshaped = onReshaped ? onReshaped : OnReshaped{noop};
    const std::optional<std::int32_t> standing = world.blockState(x, y, z);
    // parity: `RedStoneWireBlock.updateIndirectNeighbourShapes`.
    const std::vector<redstoneWire::Step> steps = standing == stateId
                                                      ? redstoneWire::acrossSteps(world, x, y, z)
                                                      : redstoneWire::acrossSteps(world, x, y, z, stateId);
    for (const redstoneWire::Step& at : steps) {
        if (const std::optional<std::int32_t> drawn = redstoneWire::reshape(
                world, at.x, at.y, at.z, world.blockState(at.x, at.y, at.z).value_or(0), at.towards);
            drawn.has_value()) {
            reshaped(drawn.value(), at.x, at.y, at.z);
        }
    }
}

void neighbourChanged(const Context& context, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                      const std::optional<std::int32_t> source, std::vector<BlockChange>& effects,
                      const OnBroken& onBroken, const OnReshaped& onReshaped) {
    WorldState& world = context.world;
    const OnReshaped reshaped = onReshaped ? onReshaped : OnReshaped{noop};
    const OnBroken broken = onBroken ? onBroken : OnBroken{noop};
    const std::optional<std::int32_t> standing = world.blockState(x, y, z);
    if (!standing.has_value()) {
        return;
    }
    if (context.physicsAllowed && !context.physicsAllowed(x, y, z, source)) {
        return;
    }
    // parity: `Level.removeBlock` writes with flag three.
    if (dropUnsupportedComponent(context, x, y, z, broken).has_value()) {
        Context gone = context;
        gone.phases = phase::kDeparture | phase::kShapes | phase::kNeighbours;
        afterChange(gone, x, y, z, standing, effects, broken, reshaped, ArrivedShape::NotPlaced);
        return;
    }
    for (const BlockChange& change : redstoneReactors::reactAt(world, context.worldName, x, y, z, context.gameTime,
                                                               context.random, context.tntExplodes, source)) {
        reshaped(change.stateId, change.x, change.y, change.z);
        // parity: a reactor answers a neighbour with `level.setBlock(pos, state, 2)`.
        reshapeOnly(context, change.x, change.y, change.z, broken, reshaped);
        if (!change.effects.empty()) {
            effects.push_back(change);
        }
        redstoneComponents::watchFor(world, change.x, change.y, change.z);
        Context tell = context;
        tell.phases = phase::kNeighbours;
        afterChange(tell, change.x, change.y, change.z, std::nullopt, effects, broken, reshaped,
                    ArrivedShape::NotPlaced);
    }
    const pistons::Context piston{.world = world,
                                  .worldName = context.worldName,
                                  .minY = context.minY,
                                  .maxY = context.maxY,
                                  .random = context.random};
    const auto armed = [&reshaped](const std::optional<BlockChange>& change) {
        if (change.has_value()) {
            reshaped(change->stateId, change->x, change->y, change->z);
        }
    };
    if (source.has_value()) {
        if (const std::optional<std::int32_t> bent =
                rails::onNeighbourChange(world, context.worldName, x, y, z, *source);
            bent.has_value()) {
            reshaped(*bent, x, y, z);
        }
    }
    armed(context.dispenser ? context.dispenser(world, x, y, z)
                            : dispensers::onNeighbourChange(world, context.worldName, x, y, z));
    armed(crafters::onNeighbourChange(world, context.worldName, x, y, z));
    pistons::trigger(piston, x, y, z);
    sponges::absorb(world, x, y, z, reshaped);
    // parity: `FrostedIceBlock.neighborChanged`, which only listens to other frosted ice.
    const wire::common::Identifier* told =
        source.has_value() ? registry::blockStateDefinitions::blockOf(*source) : nullptr;
    if (told != nullptr && told->path() == "frosted_ice") {
        if (const std::optional<BlockChange> melted = scheduledBlocks::frostedIceHeard(world, x, y, z);
            melted.has_value()) {
            reshaped(melted->stateId, melted->x, melted->y, melted->z);
            std::vector<BlockChange> chained;
            scheduledBlocks::frostedIceChain(world, x, y, z, chained);
            for (const BlockChange& gone : chained) {
                reshaped(gone.stateId, gone.x, gone.y, gone.z);
            }
        }
    }
}

void reshapeOnly(const Context& context, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                 const OnBroken& onBroken, const OnReshaped& onReshaped) {
    WorldState& world = context.world;
    const OnReshaped reshaped = onReshaped ? onReshaped : OnReshaped{noop};
    const OnBroken broken = onBroken ? onBroken : OnBroken{noop};
    const blockSurvival::ScaffoldingHasFallen scaffolding = [&world](const WorldState&, std::int32_t sx,
                                                                     std::int32_t sy, std::int32_t sz) {
        return scheduledBlocks::scaffoldingHasFallen(world, sx, sy, sz);
    };
    const Walk walk{.context = context, .scaffolding = scaffolding, .onReshaped = reshaped, .onBroken = broken};
    std::deque<Position> pending;
    pending.push_back({x, y, z});
    std::int32_t examined = 0;
    while (!pending.empty() && examined < context.maxChainedUpdates) {
        const Position at = pending.front();
        pending.pop_front();
        ++examined;
        for (const BlockFace face : kUpdateShapeOrder) {
            visit(walk, pending, at[0] + registry::stepX(face), at[1] + registry::stepY(face),
                  at[2] + registry::stepZ(face), registry::opposite(face));
        }
    }
}

namespace {

/// parity: `LevelEvent.PARTICLES_DESTROY_BLOCK`.
constexpr std::int32_t kBlockDestroyedEffect = 2001;

/// parity: `spawnGolemInWorld`'s `snapTo`.
constexpr double kGolemFootroom = 0.05;

[[nodiscard]] bool buildGolem(const Context& context, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                              std::vector<BlockChange>& effects, const OnBroken& broken, const OnReshaped& reshaped) {
    WorldState& world = context.world;
    const std::optional<std::int32_t> standing = world.blockState(x, y, z);
    if (!standing.has_value()) {
        return false;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*standing);
    if (block == nullptr) {
        return false;
    }
    if (const std::string_view path = block->path(); path != "carved_pumpkin" && path != "jack_o_lantern") {
        return false;
    }
    const std::optional<golemPatterns::Match> match = golemPatterns::find(world, x, y, z, context.isCopper);
    if (!match.has_value()) {
        return false;
    }
    // parity: `clearPatternBlocks` writes air with flag 2.
    std::vector<std::int32_t> cleared;
    cleared.reserve(match->cells.size());
    for (const std::array<std::int32_t, 3>& cell : match->cells) {
        const std::int32_t was = world.blockState(cell[0], cell[1], cell[2]).value_or(0);
        cleared.push_back(was);
        if (!world.setBlockState(cell[0], cell[1], cell[2], registry::blocks::air().stateId)) {
            continue;
        }
        reshaped(registry::blocks::air().stateId, cell[0], cell[1], cell[2]);
        effects.push_back(BlockChange{.x = cell[0],
                                      .y = cell[1],
                                      .z = cell[2],
                                      .stateId = was,
                                      .effects = {blockEffect::Effect{.type = kBlockDestroyedEffect, .data = was}}});
    }
    if (context.summonGolem) {
        context.summonGolem(match->kind, match->spawn[0] + 0.5, match->spawn[1] + kGolemFootroom,
                            match->spawn[2] + 0.5);
    }
    for (std::size_t index = 0; index < match->cells.size(); ++index) {
        const std::array<std::int32_t, 3>& cell = match->cells[index];
        afterChange(context, cell[0], cell[1], cell[2], cleared[index], effects, broken, reshaped);
    }
    // parity: `replaceCopperBlockWithChest` is the last thing `trySpawnGolem` does.
    if (match->kind == golemPatterns::Golem::Copper) {
        const wire::common::Identifier* body = registry::blockStateDefinitions::blockOf(match->bodyState);
        const std::map<std::string, std::string> pumpkin =
            registry::blockStateDefinitions::propertiesOf(match->pumpkinState);
        const auto facing = pumpkin.find("facing");
        if (body != nullptr && facing != pumpkin.end()) {
            const wire::common::Identifier chestBlock =
                wire::common::Identifier::vanilla(copperChests::chestForBlock(body->path()));
            const std::optional<std::int32_t> chest = copperChests::placedState(
                world, match->body[0], match->body[1], match->body[2], chestBlock.path(), facing->second);
            if (chest.has_value() && world.setBlockState(match->body[0], match->body[1], match->body[2], *chest)) {
                // parity: `Level.setBlock` gives the new chest its block entity, empty, from the start.
                if (server::world::Chunk* chunk = world.chunkIfLoaded(match->body[0] >> 4, match->body[2] >> 4);
                    chunk != nullptr) {
                    chunk->setBlockEntity(server::world::ChunkBlockEntity{match->body[0] & 15, match->body[1],
                                                                          match->body[2] & 15, chestBlock,
                                                                          blockEntityDefaults::of(chestBlock, *chest)});
                    chunk->markDirty();
                }
                reshaped(*chest, match->body[0], match->body[1], match->body[2]);
                reshapeOnly(context, match->body[0], match->body[1], match->body[2], broken, reshaped);
            }
        }
    }
    return true;
}

} // namespace

void afterChange(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                 std::optional<std::int32_t> removed, std::vector<BlockChange>& effects, const OnBroken& onBroken,
                 const OnReshaped& onReshaped, ArrivedShape arrivedShape, const std::function<void()>& afterShapes) {
    WorldState& world = context.world;
    const OnReshaped reshaped = onReshaped ? onReshaped : OnReshaped{noop};
    const OnBroken broken = onBroken ? onBroken : OnBroken{noop};
    const auto runs = [&context](const std::uint32_t phase) { return (context.phases & phase) != 0; };
    if (runs(phase::kPlace) && buildGolem(context, x, y, z, effects, broken, reshaped)) {
        return;
    }
    const blockSurvival::ScaffoldingHasFallen scaffolding = [&world](const WorldState&, std::int32_t sx,
                                                                     std::int32_t sy, std::int32_t sz) {
        return scheduledBlocks::scaffoldingHasFallen(world, sx, sy, sz);
    };

    // parity: `RedStoneWireBlock.onPlace` tells the blocks over and under it.
    bool wireArrived = false;
    if (const std::optional<std::int32_t> arrived = world.blockState(x, y, z);
        runs(phase::kPlace) && arrived.has_value() && redstoneWire::isWire(*arrived) &&
        !(removed.has_value() && redstoneWire::isWire(*removed))) {
        wireArrived = true;
        // parity: `onPlace` settles this dust's own strength before it tells anybody.
        for (const BlockChange& change : redstone::settle(world, context.worldName, x, y, z)) {
            reshaped(change.stateId, change.x, change.y, change.z);
            shapeAround(context, change.x, change.y, change.z, reshaped);
        }
        static_cast<void>(breakIfUnsupported(context, x, y, z, scaffolding, broken));
    }
    if (const std::optional<std::int32_t> arrived = world.blockState(x, y, z); arrived.has_value()) {
        const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*arrived);
        const std::string_view path = block == nullptr ? std::string_view{} : block->path();
        // parity: `PotentSulfurBlock.onPlace` raises `BLOCK_ACTIVATE` for a geyser that arrives.
        if (runs(phase::kPlace) && path == "potent_sulfur") {
            const std::map<std::string, std::string> properties =
                registry::blockStateDefinitions::propertiesOf(*arrived);
            const auto state = properties.find("potent_sulfur_state");
            if (state != properties.end() && (state->second == "erupting" || state->second == "continuous") &&
                world.hasSculkSensorNear(x, y, z, vibrations::kCalibratedRange)) {
                vibrations::emit(world, "block_activate", x + 0.5, y + 0.5, z + 0.5, context.gameTime);
            }
        }
        // parity: `ShelfBlock.affectNeighborsAfterRemoval`.
        if (runs(phase::kDeparture) && removed.has_value()) {
            std::vector<BlockChange> parted;
            shelves::leftBehind(world, x, y, z, *removed, parted);
            for (const BlockChange& change : parted) {
                reshaped(change.stateId, change.x, change.y, change.z);
            }
            // parity: `PistonHeadBlock.affectNeighborsAfterRemoval`, which is `destroyBlock` on the.
            if (const std::optional<std::array<std::int32_t, 3>> body = pistons::fittingBase(world, x, y, z, *removed);
                body.has_value()) {
                const std::optional<std::int32_t> was = world.blockState((*body)[0], (*body)[1], (*body)[2]);
                if (was.has_value() &&
                    world.setBlockState((*body)[0], (*body)[1], (*body)[2], registry::blocks::air().stateId)) {
                    broken(*was, (*body)[0], (*body)[1], (*body)[2]);
                }
            }
        }
        // parity: `ShelfBlock.onPlace`, which joins the chain for a shelf that arrives already.
        if (runs(phase::kPlace) && shelves::isShelf(path)) {
            std::vector<BlockChange> joined;
            shelves::arrived(world, x, y, z, removed.has_value() && wasConnectedShelf(*removed), joined);
            for (const BlockChange& change : joined) {
                reshaped(change.stateId, change.x, change.y, change.z);
            }
        }
    }
    // parity: `LiquidBlock.onPlace` books inside the write.
    if (runs(phase::kTicks) && context.arrivedFluid) {
        context.arrivedFluid(x, y, z);
    }
    if (runs(phase::kPlace)) {
        settleArrivedSignal(context, x, y, z, removed, reshaped);
    }
    // parity: `ScaffoldingBlock.onPlace` really is an `onPlace`.
    if (runs(phase::kTicks)) {
        scheduledBlocks::scheduleScaffolding(world, x, y, z);
    }

    // parity: `Level.setBlock` runs `updateIndirectNeighbourShapes` on the state that **left** as.
    if (runs(phase::kShapes) && removed.has_value()) {
        for (const redstoneWire::Step& at : redstoneWire::acrossSteps(world, x, y, z, *removed)) {
            if (const std::optional<std::int32_t> drawn = redstoneWire::reshape(
                    world, at.x, at.y, at.z, world.blockState(at.x, at.y, at.z).value_or(0), at.towards);
                drawn.has_value()) {
                reshaped(drawn.value(), at.x, at.y, at.z);
            }
        }
    }
    // `updateShape`. parity: `CreakingHeartBlock` has no `onPlace`, only `updateShape`, so its turn
    if (runs(phase::kShapes) && arrivedShape != ArrivedShape::NotPlaced) {
        // parity: `BlockInput.place` settles the pre-shape **before** the write.
        if (arrivedShape == ArrivedShape::Reshape) {
            if (const std::optional<std::int32_t> moved = reshape(context, x, y, z); moved.has_value()) {
                reshaped(*moved, x, y, z);
            }
        }
        // parity: the fifteen blocks of `booksItsOwnBreak` and `CoralBlock` have no `onPlace`.
        scheduledBlocks::scheduleBreakIfLoose(
            world, x, y, z, blockSurvival::isSupported(world, x, y, z, std::nullopt, scaffolding, context.hasSkylight));
        // parity: `ObserverBlock.updateShape` on the observer itself.
        redstoneComponents::watchOnPlace(world, x, y, z);
    }
    if (runs(phase::kShapes)) {
        for (const redstoneWire::Step& at : redstoneWire::acrossSteps(world, x, y, z)) {
            if (const std::optional<std::int32_t> drawn = redstoneWire::reshape(
                    world, at.x, at.y, at.z, world.blockState(at.x, at.y, at.z).value_or(0), at.towards);
                drawn.has_value()) {
                reshaped(drawn.value(), at.x, at.y, at.z);
            }
        }
    }
    // parity: `FallingBlock.onPlace` and its `updateShape`.
    if (runs(phase::kTicks)) {
        fallingBlocks::scheduleIfFalling(world, x, y, z, fallingBlocks::Asked::onPlace);
    }
    if (runs(phase::kShapes)) {
        fallingBlocks::scheduleIfFalling(world, x, y + 1, z, fallingBlocks::Asked::updateShape);
    }
    if (runs(phase::kTicks)) {
        // parity: `SnifferEggBlock.onPlace`.
        scheduledBlocks::scheduleHatch(world, x, y, z, context.random.nextLong());
        // parity: `FrostedIceBlock.onPlace`.
        scheduledBlocks::scheduleFrostedIce(world, x, y, z, context.random);
        // parity: `FireBlock.onPlace`.
        if (const std::optional<std::int32_t> lit = world.blockState(x, y, z); lit.has_value() && fires::isFire(*lit)) {
            fires::schedule(world, x, y, z, context.random);
        }
    }
    // parity: the neighbour update every redstone component listens to: the dust settles at once and.
    const Walk walk{.context = context, .scaffolding = scaffolding, .onReshaped = reshaped, .onBroken = broken};
    std::deque<Position> pending;
    pending.push_back({x, y, z});
    std::int32_t examined = 0;
    while (runs(phase::kShapes) && !pending.empty() && examined < context.maxChainedUpdates) {
        const Position at = pending.front();
        pending.pop_front();
        ++examined;
        // parity: `BlockBehaviour.updateNeighbourShapes` walks `UPDATE_SHAPE_ORDER`, which is.
        for (const BlockFace face : kUpdateShapeOrder) {
            visit(walk, pending, at[0] + registry::stepX(face), at[1] + registry::stepY(face),
                  at[2] + registry::stepZ(face), registry::opposite(face));
        }
    }
    // parity: this is the tail of an ordinary write, `updateNeighboursOnBlockSet`.
    const std::vector<BlockChange> settled =
        runs(phase::kNeighbours)
            ? redstone::settle(world, context.worldName, x, y, z, {}, redstone::Asked::NeighboursOnly)
            : std::vector<BlockChange>{};
    for (const BlockChange& change : settled) {
        reshaped(change.stateId, change.x, change.y, change.z);
    }
    const redstoneComponents::ContainerSignalAt reading = comparatorReading::of(world);
    if (runs(phase::kNeighbours)) {
        redstoneComponents::scheduleAround(world, context.worldName, x, y, z, true, reading);
    }
    // parity: `Level.setBlock` follows the neighbour updates with `updateNeighbourForOutputSignal`.
    if (runs(phase::kNeighbours) && comparatorReading::exists(world, x, y, z)) {
        redstoneComponents::scheduleComparatorsAround(world, context.worldName, x, y, z, reading);
    }
    // parity: `Containers.updateNeighboursAfterDestroy`.
    if (runs(phase::kDeparture) && removed.has_value() && redstoneComponents::tellsComparatorsOnRemoval(*removed)) {
        const std::optional<std::int32_t> nowHere = world.blockState(x, y, z);
        if (!nowHere.has_value() || !sameBlock(*removed, *nowHere)) {
            redstoneComponents::scheduleComparatorsAround(world, context.worldName, x, y, z, reading);
        }
    }
    // parity: `DefaultRedstoneWireEvaluator.updatePowerStrength` ends with `updateNeighborsAt` on.
    for (const BlockChange& change : settled) {
        redstoneComponents::scheduleAroundWire(world, context.worldName, change.x, change.y, change.z, reading);
    }
    // parity: `ObserverBlock.updateShape`.
    if (runs(phase::kNeighbours) || runs(phase::kShapes)) {
        redstoneComponents::watchFor(world, x, y, z);
    }
    for (const BlockChange& change : settled) {
        redstoneComponents::watchFor(world, change.x, change.y, change.z);
    }
    // parity: every `neighborChanged` that reads the power: doors, lamps, bulbs and the rest.
    if (afterShapes) {
        afterShapes();
    }

    // parity: `CollectingNeighborUpdater` answers from a stack.
    struct Reacting {
        Position at;
        std::optional<std::int32_t> told;
        std::int32_t face = 0;
        bool started = false;
    };

    std::vector<Reacting> reacting;
    // parity: a block that changed is asked again, and so are the squares its own update reaches past.
    const auto reactsAt = [&reacting](const BlockChange& change) {
        reacting.push_back({{change.x, change.y, change.z}, change.stateId, 0, false});
        for (const registry::BlockFace also : redstone::alsoTells(change.stateId)) {
            reacting.push_back(
                {{change.x + registry::stepX(also), change.y + registry::stepY(also), change.z + registry::stepZ(also)},
                 change.stateId,
                 0,
                 false});
        }
    };
    // parity: the square that changed tells with the block that **left** on the world's road and with.
    if (runs(phase::kNeighbours)) {
        reacting.push_back({{x, y, z}, context.railArrivalTells ? removed : world.blockState(x, y, z), 0, false});
    }
    // parity: `DiodeBlock.updateNeighborsInFront` from `onPlace` and from.
    const std::optional<std::int32_t> standing = world.blockState(x, y, z);
    std::vector<std::pair<redstoneComponents::Reach, std::int32_t>> reach;
    if (runs(phase::kPlace) && standing.has_value() && !isObserverState(*standing)) {
        for (const redstoneComponents::Reach& at : redstoneComponents::outputReach(*standing, x, y, z)) {
            reach.emplace_back(at, *standing);
        }
    }
    // parity: `LevelChunk.setBlockState` runs `affectNeighborsAfterRemoval` on the state that left.
    const bool changedBlock = !standing.has_value() || !removed.has_value() || !sameBlock(*removed, *standing);
    const bool railArrived = standing.has_value() && rails::isRail(*standing);
    // parity: `affectNeighborsAfterRemoval` for a dust.
    const bool wireLeft =
        runs(phase::kDeparture) && changedBlock && removed.has_value() && redstoneWire::isWire(*removed);
    // parity: and this is the one line the two roads do not share: `LevelChunk.setBlockState` lets a.
    if (runs(phase::kDeparture) && removed.has_value() && (changedBlock || (railArrived && context.railArrivalTells))) {
        for (const redstoneComponents::Reach& at : redstoneComponents::outputReach(*removed, x, y, z)) {
            reach.emplace_back(at, *removed);
        }
        // parity: the two `affectNeighborsAfterRemoval` that reach past their own square.
        for (const redstoneComponents::Reach& at : redstoneComponents::removalReach(*removed, x, y, z)) {
            reach.emplace_back(at, *removed);
        }
    }
    // parity: the rest of `RedStoneWireBlock.onPlace`.
    const auto corner = [&reach, &world](const std::int32_t cx, const std::int32_t cy, const std::int32_t cz,
                                         const std::int32_t wireState) {
        const std::optional<std::int32_t> here = world.blockState(cx, cy, cz);
        if (!here.has_value() || !redstoneWire::isWire(*here)) {
            return;
        }
        reach.emplace_back(redstoneComponents::Reach{.x = cx, .y = cy, .z = cz}, wireState);
        for (const BlockFace face : kFaces) {
            reach.emplace_back(redstoneComponents::Reach{.x = cx + registry::stepX(face),
                                                         .y = cy + registry::stepY(face),
                                                         .z = cz + registry::stepZ(face)},
                               wireState);
        }
    };
    const auto wireSpread = [&reach, &world, &corner, x, y, z](const std::span<const BlockFace> sides,
                                                               const std::int32_t wireState) {
        for (const BlockFace face : sides) {
            reach.emplace_back(redstoneComponents::Reach{.x = x + registry::stepX(face),
                                                         .y = y + registry::stepY(face),
                                                         .z = z + registry::stepZ(face)},
                               wireState);
        }
        // parity: `updateNeighborsOfNeighboringWires` walks the four horizontals **twice**, in.
        for (const BlockFace side : kFlat) {
            corner(x + registry::stepX(side), y, z + registry::stepZ(side), wireState);
        }
        for (const BlockFace side : kFlat) {
            const std::int32_t sx = x + registry::stepX(side);
            const std::int32_t sz = z + registry::stepZ(side);
            // parity: a side that conducts sends the news **up**, one that does not sends it down.
            const std::optional<std::int32_t> beside = world.blockState(sx, y, sz);
            const bool conducts = beside.has_value() && registry::blockSupport::conductsRedstone(*beside);
            corner(sx, conducts ? y + 1 : y - 1, sz, wireState);
        }
    };
    if (wireArrived) {
        // parity: `Direction.Plane.VERTICAL` is up and then down.
        static constexpr std::array<BlockFace, 2> kVertical{BlockFace::Up, BlockFace::Down};
        wireSpread(kVertical, standing.value_or(0));
    }
    if (wireLeft) {
        wireSpread(kFaces, removed.value_or(0));
    }
    for (const auto& [at, told] : reach) {
        static_cast<void>(told);
        std::vector<BlockChange> ahead;
        if (at.skip.has_value()) {
            redstoneComponents::scheduleInFront(world, context.worldName, at.x, at.y, at.z, *at.skip, reading);
            ahead = redstoneReactors::updateInFront(world, context.worldName, at.x, at.y, at.z, *at.skip,
                                                    context.gameTime, context.random, context.tntExplodes);
        } else {
            // parity: `RedstoneTorchBlock.notifyNeighbors` is a plain `updateNeighborsAt` on each of.
            redstoneComponents::scheduleAround(world, context.worldName, at.x, at.y, at.z, false, reading);
            ahead = redstoneReactors::update(world, context.worldName, at.x, at.y, at.z, context.gameTime,
                                             context.random, context.tntExplodes, false);
        }
        for (const BlockChange& change : ahead) {
            reshaped(change.stateId, change.x, change.y, change.z);
            // parity: a reactor answers a neighbour with `level.setBlock(pos, state, 2)`.
            reshapeOnly(context, change.x, change.y, change.z, broken, reshaped);
            if (!change.effects.empty()) {
                effects.push_back(change);
            }
            redstoneComponents::watchFor(world, change.x, change.y, change.z);
            reactsAt(change);
        }
    }
    // parity: `onPlace`.
    for (const BlockChange& change : runs(phase::kPlace)
                                         ? redstoneReactors::placed(world, context.worldName, x, y, z, context.gameTime,
                                                                    context.random, context.tntExplodes, removed)
                                         : std::vector<BlockChange>{}) {
        reshaped(change.stateId, change.x, change.y, change.z);
        if (!change.effects.empty()) {
            effects.push_back(change);
        }
        redstoneComponents::watchFor(world, change.x, change.y, change.z);
        reactsAt(change);
    }
    // parity: `DefaultRedstoneWireEvaluator.updatePowerStrength` runs `updateNeighborsAt` on the dust.
    for (const BlockChange& change : settled) {
        for (const std::array<std::int32_t, 3>& at : redstoneComponents::wireReach(change.x, change.y, change.z)) {
            reacting.push_back({{at[0], at[1], at[2]}, std::nullopt, 0, false});
        }
    }
    std::ranges::reverse(reacting);
    std::int32_t reacted = 0;
    while (!reacting.empty()) {
        Reacting& current = reacting.back();
        if (!current.started) {
            if (reacted >= context.maxChainedUpdates) {
                // parity: `CollectingNeighborUpdater.addAndRun` stops answering past.
                break;
            }
            current.started = true;
            ++reacted;
        }
        if (current.face < 6) {
            const BlockFace face = kUpdateOrder[current.face];
            ++current.face;
            // parity: the six squares this notice reaches each run their `neighborChanged`, and a.
            const std::int32_t nx = current.at[0] + registry::stepX(face);
            const std::int32_t ny = current.at[1] + registry::stepY(face);
            const std::int32_t nz = current.at[2] + registry::stepZ(face);
            const std::optional<bool> diode = dropUnsupportedComponent(context, nx, ny, nz, broken);
            if (!diode.has_value()) {
                continue;
            }
            // parity: `Level.removeBlock` writes with flag three.
            reshapeOnly(context, nx, ny, nz, broken, reshaped);
            if (*diode) {
                for (std::int32_t index = 5; index >= 0; --index) {
                    const BlockFace also = kUpdateOrder[index];
                    reacting.push_back(
                        {{nx + registry::stepX(also), ny + registry::stepY(also), nz + registry::stepZ(also)},
                         std::nullopt,
                         0,
                         false});
                }
            }
            reacting.push_back({{nx, ny, nz}, std::nullopt, 0, false});
            continue;
        }
        const Position at = current.at;
        const std::optional<std::int32_t> told = current.told;
        reacting.pop_back();
        if (context.physicsAllowed && !context.physicsAllowed(at[0], at[1], at[2], told)) {
            continue;
        }
        for (const BlockChange& change :
             redstoneReactors::update(world, context.worldName, at[0], at[1], at[2], context.gameTime, context.random,
                                      context.tntExplodes, false, told)) {
            reshaped(change.stateId, change.x, change.y, change.z);
            // parity: the same flag-two shape walk as in the reach loop above. This is the loop an.
            reshapeOnly(context, change.x, change.y, change.z, broken, reshaped);
            if (!change.effects.empty()) {
                effects.push_back(change);
            }
            redstoneComponents::watchFor(world, change.x, change.y, change.z);
            // parity: what the answer reaches past its own square is told inside this `neighborChanged`.
            std::vector<Reacting> reached;
            for (const registry::BlockFace also : redstone::alsoTells(change.stateId)) {
                reached.push_back({{change.x + registry::stepX(also), change.y + registry::stepY(also),
                                    change.z + registry::stepZ(also)},
                                   change.stateId,
                                   0,
                                   false});
            }
            for (auto it = reached.rbegin(); it != reached.rend(); ++it) {
                reacting.push_back(*it);
            }
            reacting.push_back({{change.x, change.y, change.z}, change.stateId, 0, false});
        }
    }
    // parity: `DispenserBlock.neighborChanged`.
    const auto arm = [&context, &world](std::int32_t atX, std::int32_t atY, std::int32_t atZ) {
        return context.dispenser ? context.dispenser(world, atX, atY, atZ)
                                 : dispensers::onNeighbourChange(world, context.worldName, atX, atY, atZ);
    };
    const pistons::Context piston{.world = world,
                                  .worldName = context.worldName,
                                  .minY = context.minY,
                                  .maxY = context.maxY,
                                  .random = context.random};
    // parity: `Level.updateNeighborsAt`.
    const bool wasIce = removed.has_value() && registry::blockStateDefinitions::blockOf(*removed) != nullptr &&
                        registry::blockStateDefinitions::blockOf(*removed)->path() == "frosted_ice";
    const auto tellAround =
        [&](std::int32_t cx, std::int32_t cy, std::int32_t cz, std::optional<BlockFace> skip = std::nullopt,
            std::optional<std::int32_t> from = std::nullopt, std::optional<std::int32_t> told = std::nullopt) {
            // parity: `updateNeighborsAt(pos, block)` carries the block that **told**.
            const std::optional<std::int32_t> source =
                from.has_value() ? from : (told.has_value() ? told : world.blockState(cx, cy, cz));
            // parity: `updateNeighborsInFront` is two calls, and the first one is.
            if (from.has_value()) {
                pistons::trigger(piston, cx, cy, cz);
                if (const std::optional<BlockChange> armed = arm(cx, cy, cz); armed.has_value()) {
                    reshaped(armed->stateId, armed->x, armed->y, armed->z);
                }
                if (const std::optional<BlockChange> armed =
                        crafters::onNeighbourChange(world, context.worldName, cx, cy, cz);
                    armed.has_value()) {
                    reshaped(armed->stateId, armed->x, armed->y, armed->z);
                }
                if (const std::optional<std::int32_t> bent =
                        rails::onNeighbourChange(world, context.worldName, cx, cy, cz, *from);
                    bent.has_value()) {
                    reshaped(*bent, cx, cy, cz);
                }
            }
            for (const BlockFace face : kUpdateOrder) {
                if (skip.has_value() && face == *skip) {
                    continue;
                }
                const std::int32_t nx = cx + registry::stepX(face);
                const std::int32_t ny = cy + registry::stepY(face);
                const std::int32_t nz = cz + registry::stepZ(face);
                if (source.has_value()) {
                    if (const std::optional<std::int32_t> bent =
                            rails::onNeighbourChange(world, context.worldName, nx, ny, nz, *source);
                        bent.has_value()) {
                        reshaped(*bent, nx, ny, nz);
                    }
                }
                if (const std::optional<BlockChange> armed = arm(nx, ny, nz); armed.has_value()) {
                    reshaped(armed->stateId, armed->x, armed->y, armed->z);
                }
                if (const std::optional<BlockChange> armed =
                        crafters::onNeighbourChange(world, context.worldName, nx, ny, nz);
                    armed.has_value()) {
                    reshaped(armed->stateId, armed->x, armed->y, armed->z);
                }
                pistons::trigger(piston, nx, ny, nz);
                // parity: `SpongeBlock.neighborChanged` drinks when the **neighbour** changes too, not.
                sponges::absorb(world, nx, ny, nz, reshaped);
                // parity: `FrostedIceBlock.neighborChanged`, which only listens to other frosted ice.
                const bool icy =
                    wasIce || (source.has_value() && registry::blockStateDefinitions::blockOf(*source) != nullptr &&
                               registry::blockStateDefinitions::blockOf(*source)->path() == "frosted_ice");
                if (icy) {
                    if (const std::optional<BlockChange> melted = scheduledBlocks::frostedIceHeard(world, nx, ny, nz);
                        melted.has_value()) {
                        reshaped(melted->stateId, melted->x, melted->y, melted->z);
                        std::vector<BlockChange> chained;
                        scheduledBlocks::frostedIceChain(world, nx, ny, nz, chained);
                        for (const BlockChange& gone : chained) {
                            reshaped(gone.stateId, gone.x, gone.y, gone.z);
                        }
                    }
                }
            }
        };
    // parity: the two roads hand along different blocks. `Level.setBlock` tells with.
    if (runs(phase::kNeighbours)) {
        tellAround(x, y, z, std::nullopt, std::nullopt, context.railArrivalTells ? removed : std::nullopt);
    }
    // parity: the reach above is an `updateNeighborsAt` like any other.
    for (const auto& [at, from] : reach) {
        tellAround(at.x, at.y, at.z, at.skip, from);
    }
    if (runs(phase::kPlace)) {
        if (const std::optional<BlockChange> armed = arm(x, y, z); armed.has_value()) {
            reshaped(armed->stateId, armed->x, armed->y, armed->z);
        }
        if (const std::optional<BlockChange> armed = crafters::onNeighbourChange(world, context.worldName, x, y, z);
            armed.has_value()) {
            reshaped(armed->stateId, armed->x, armed->y, armed->z);
        }
        pistons::trigger(piston, x, y, z);
    }
    // parity: and the same for every square a dust that moved reaches, which is the dust and its six.
    for (const BlockChange& change : settled) {
        for (const std::array<std::int32_t, 3>& at : redstoneComponents::wireReach(change.x, change.y, change.z)) {
            tellAround(at[0], at[1], at[2]);
        }
    }
    const std::optional<std::int32_t> laid = world.blockState(x, y, z);
    // parity: `LevelChunk.setBlockState` only runs `affectNeighborsAfterRemoval` when the block.
    const bool tripwireBlockChanged = !laid.has_value() || !removed.has_value() || !sameBlock(*removed, *laid);
    // parity: `TripWireBlock.onPlace`.
    if (runs(phase::kPlace) && tripwireBlockChanged && laid.has_value() && tripwires::isWire(*laid)) {
        tripwires::updateSource(world, x, y, z, *laid, context.gameTime, effects);
    }
    // parity: `TripWireBlock.affectNeighborsAfterRemoval`.
    if (runs(phase::kDeparture) && tripwireBlockChanged && removed.has_value() && tripwires::isHook(*removed)) {
        const std::map<std::string, std::string> gone = registry::blockStateDefinitions::propertiesOf(*removed);
        const auto attachedAt = gone.find("attached");
        const auto poweredAt = gone.find("powered");
        const bool wasAttached = attachedAt != gone.end() && attachedAt->second == "true";
        const bool wasPowered = poweredAt != gone.end() && poweredAt->second == "true";
        if (wasAttached || wasPowered) {
            tripwires::calculateHook(world, x, y, z, context.gameTime, effects, -1, std::nullopt, *removed);
        }
    }
    if (runs(phase::kDeparture) && tripwireBlockChanged && removed.has_value() && tripwires::isWire(*removed)) {
        if (const std::optional<std::int32_t> live =
                registry::blockStateDefinitions::stateIdOf(tripwires::wireBlock(), {{"powered", "true"}}, *removed);
            live.has_value()) {
            tripwires::updateSource(world, x, y, z, *live, context.gameTime, effects);
        }
    }
    // parity: `LiquidBlock`'s three hooks.
    if (runs(phase::kTicks) && context.wakeFluids) {
        context.wakeFluids(x, y, z);
    }
    // parity: `SpongeBlock.onPlace`, which drinks before anything else gets a turn.
    if (runs(phase::kPlace)) {
        sponges::absorb(world, x, y, z, reshaped);
        sponges::dryIfEvaporating(world, x, y, z, reshaped, context.evaporates);
    }
    // parity: `FrogspawnBlock.onPlace`, the same idea on a longer and rougher clock.
    if (runs(phase::kTicks)) {
        scheduledBlocks::scheduleFrogspawn(world, x, y, z, context.random.nextLong());
    }
}

} // namespace basalt::server::runtime::world::neighbourUpdates
