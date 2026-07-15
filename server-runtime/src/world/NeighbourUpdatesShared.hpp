#pragma once

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

namespace basalt::server::runtime::world::neighbourUpdates::detail {

using registry::BlockFace;

/// parity: `LeavesBlock.DECAY_DISTANCE`.
constexpr std::int32_t kDecayDistance = 7;

constexpr std::array<BlockFace, 6> kFaces{BlockFace::Down,  BlockFace::Up,   BlockFace::North,
                                          BlockFace::South, BlockFace::West, BlockFace::East};

[[nodiscard]] inline std::int32_t axisOfName(std::string_view name) {
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

inline void noop(std::int32_t, std::int32_t, std::int32_t, std::int32_t) {}

[[nodiscard]] inline bool sameBlock(const std::int32_t one, const std::int32_t other) {
    const wire::common::Identifier* first = registry::blockStateDefinitions::blockOf(one);
    const wire::common::Identifier* second = registry::blockStateDefinitions::blockOf(other);
    return first != nullptr && second != nullptr && *first == *second;
}

/// parity: `ObserverBlock.onPlace` only reaches out for one that arrives **powered**.
[[nodiscard]] inline bool isObserverState(const std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && block->path() == "observer";
}

[[nodiscard]] inline std::int32_t stateAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    return world.blockState(x, y, z).value_or(registry::blocks::air().stateId);
}

/// parity: `LeavesBlock.getOptionalDistanceAt`.
[[nodiscard]] inline std::int32_t distanceOf(std::int32_t stateId) {
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

[[nodiscard]] inline std::optional<std::int32_t> rewrite(WorldState& world, std::int32_t x, std::int32_t y,
                                                         std::int32_t z, std::int32_t stateId,
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
[[nodiscard]] inline std::optional<std::int32_t> retreeLeaves(WorldState& world, std::int32_t x, std::int32_t y,
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
[[nodiscard]] inline std::optional<std::int32_t> rebuildWall(const Context& context, std::int32_t x, std::int32_t y,
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
[[nodiscard]] inline std::optional<std::int32_t> turnStairs(WorldState& world, std::int32_t x, std::int32_t y,
                                                            std::int32_t z, std::int32_t stateId) {
    const std::optional<std::string_view> shape =
        registry::blockConnections::stairShape(stateId, stateAt(world, x, y, z - 1), stateAt(world, x, y, z + 1),
                                               stateAt(world, x - 1, y, z), stateAt(world, x + 1, y, z));
    if (!shape.has_value()) {
        return std::nullopt;
    }
    return rewrite(world, x, y, z, stateId, {{"shape", std::string{*shape}}});
}

[[nodiscard]] inline bool breakIfUnsupported(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                                             const blockSurvival::ScaffoldingHasFallen& scaffolding,
                                             const OnBroken& onBroken,
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
[[nodiscard]] inline std::optional<bool> dropUnsupportedComponent(const Context& context, const std::int32_t x,
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
[[nodiscard]] inline bool wasConnectedShelf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr || !shelves::isShelf(block->path())) {
        return false;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto part = properties.find("side_chain");
    return part != properties.end() && part->second != "unconnected";
}

/// parity: `BlockBehaviour.BlockStateBase.updateNeighbourShapes`.
inline void shapeAround(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                        const OnReshaped& reshaped) {
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
[[nodiscard]] inline bool plantBooksWater(const Context& context, const std::int32_t x, const std::int32_t y,
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
inline void bookLiquidTurn(const Context& context, const std::int32_t x, const std::int32_t y, const std::int32_t z,
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

inline void visit(const Walk& walk, std::deque<Position>& pending, std::int32_t x, std::int32_t y, std::int32_t z,
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

} // namespace basalt::server::runtime::world::neighbourUpdates::detail
