#include <basalt/server/registry/BlockConnections.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/BlockInteractions.hpp>
#include <basalt/server/runtime/world/BlockSurvival.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/Composters.hpp>
#include <basalt/server/runtime/world/CopperChests.hpp>
#include <basalt/server/runtime/world/Crafters.hpp>
#include <basalt/server/runtime/world/Dispensers.hpp>
#include <basalt/server/runtime/world/FallingBlocks.hpp>
#include <basalt/server/runtime/world/Fires.hpp>
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

/// How many neighbours one position of the cascade spends of its budget.
constexpr std::int32_t kDirections = 6;

constexpr std::array<BlockFace, 6> kFaces{BlockFace::Down,  BlockFace::Up,   BlockFace::North,
                                          BlockFace::South, BlockFace::West, BlockFace::East};

/// parity: `NeighborUpdater.UPDATE_ORDER`, the order in which the six squares around a change are
/// told. It is not `Direction.values()`, and it decides who books first when two of them answer.
constexpr std::array<BlockFace, 6> kUpdateOrder{BlockFace::West, BlockFace::East,  BlockFace::Down,
                                                BlockFace::Up,   BlockFace::North, BlockFace::South};

/// parity: `BlockBehaviour.UPDATE_SHAPE_ORDER`, which is neither of the other two.
constexpr std::array<BlockFace, 6> kUpdateShapeOrder{BlockFace::West,  BlockFace::East, BlockFace::North,
                                                     BlockFace::South, BlockFace::Down, BlockFace::Up};

/// The four a connecting block keeps a property for, named as those properties are.
constexpr std::array<std::pair<BlockFace, std::string_view>, 4> kHorizontal{
    {{BlockFace::North, "north"}, {BlockFace::South, "south"}, {BlockFace::West, "west"}, {BlockFace::East, "east"}}};

using Position = std::array<std::int32_t, 3>;

void noop(std::int32_t, std::int32_t, std::int32_t, std::int32_t) {}

[[nodiscard]] bool sameBlock(const std::int32_t one, const std::int32_t other) {
    const wire::common::Identifier* first = registry::blockStateDefinitions::blockOf(one);
    const wire::common::Identifier* second = registry::blockStateDefinitions::blockOf(other);
    return first != nullptr && second != nullptr && *first == *second;
}

/// parity: `ObserverBlock.onPlace` only reaches out for one that arrives **powered**, and one that
/// arrives powered clears itself first, so an observer that has just landed tells nobody from here.
[[nodiscard]] bool isObserverState(const std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && block->path() == "observer";
}

[[nodiscard]] std::int32_t stateAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    return world.blockState(x, y, z).value_or(registry::blocks::air().stateId);
}

/// parity: `LeavesBlock.getOptionalDistanceAt`, where a trunk is zero and anything unknown is seven.
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

/// Writes `wanted` over the block there, answering the new state only when it really moved.
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

/// How far these leaves are from a trunk, recounted from their neighbours.
///
/// parity: `LeavesBlock.updateDistance`: a log counts as zero, any other leaves as their own
/// distance, and anything else as seven. At seven the leaves are decaying, and a random tick is what
/// actually drops them.
[[nodiscard]] std::optional<std::int32_t> retreeLeaves(WorldState& world, std::int32_t x, std::int32_t y,
                                                       std::int32_t z, std::int32_t stateId,
                                                       std::optional<BlockFace> towards) {
    // parity: `LeavesBlock.updateShape` does **not** rewrite the leaves: it books a turn and answers
    // the state it was given. The distance moves one tick later, in `LeavesBlock.tick`.
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    // parity: the jar reads the **one** neighbour that moved, not the six, and books unless that
    // neighbour is a trunk these leaves already sit against. So a plank laid beside a leaf already
    // one block from its log still books a turn that changes nothing, and that turn is measurable.
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
    if (leafDistance(world, x, y, z) != distanceOf(stateId)) {
        world.scheduleBlockTick(x, y, z, *block, 1);
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

/// Breaks the block there when nothing holds it up any more.
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
    // parity: `updateShape` asks which side the news came from before it asks whether the block can
    // stay, so a hook or a wall torch only falls when its own wall is what moved.
    if (!blockSurvival::loosenedBy(*stateId, towards)) {
        return false;
    }
    if (blockSurvival::isSupported(world, x, y, z, *stateId, scaffolding, context.hasSkylight)) {
        return false;
    }
    // parity: `SugarCaneBlock.updateShape` and `CactusBlock.updateShape` book a turn instead of
    // breaking, so a stalk left without its footing stands for one more tick and the tower over it
    // comes apart a square at a time.
    const wire::common::Identifier* breaking = registry::blockStateDefinitions::blockOf(*stateId);
    const std::string_view breakingPath = breaking == nullptr ? std::string_view{} : breaking->path();
    if (scheduledBlocks::booksItsOwnBreak(breakingPath)) {
        scheduledBlocks::scheduleBreakIfLoose(world, x, y, z, false);
        return false;
    }
    // parity: `SpeleothemBlock.updateShape` books a turn too, and its own delay: a stalactite waits
    // two ticks and then falls as an entity rather than breaking where it hangs.
    if (breakingPath == "pointed_dripstone" || breakingPath == "sulfur_spike") {
        scheduledBlocks::scheduleSpeleothem(world, x, y, z, false);
        return false;
    }
    // parity: `ScaffoldingBlock.updateShape` never breaks anything either: the turn it booked is
    // what decides, and that turn has already been booked by the shape walk.
    if (breakingPath == "scaffolding") {
        return false;
    }
    world.setBlockState(x, y, z, registry::blocks::air().stateId);
    onBroken(*stateId, x, y, z);
    return true;
}

/// parity: `SideChainPartBlock.isBeingUpdatedByNeighbor`'s second half, the part the shelf carried
/// before the write.
[[nodiscard]] bool wasConnectedShelf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr || !shelves::isShelf(block->path())) {
        return false;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto part = properties.find("side_chain");
    return part != properties.end() && part->second != "unconnected";
}

/// parity: `BlockState.updateNeighbourShapes`, the six squares a write redraws, each told the side
/// the news arrives from and walked in vanilla's own order.
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

/// What one step of the cascade carries with it, so `visit` keeps a signature worth reading.
struct Walk {
    const Context& context;
    const blockSurvival::ScaffoldingHasFallen& scaffolding;
    const OnReshaped& onReshaped;
    const OnBroken& onBroken;
};

/// Reshapes the block there, breaks it if nothing holds it, and follows on from what it held.
void visit(const Walk& walk, std::deque<Position>& pending, std::int32_t x, std::int32_t y, std::int32_t z,
           BlockFace towards) {
    fallingBlocks::scheduleIfFalling(walk.context.world, x, y, z);
    bool changed = false;
    const std::optional<std::int32_t> before = walk.context.world.blockState(x, y, z);
    if (const std::optional<std::int32_t> reshaped = reshape(walk.context, x, y, z, towards); reshaped.has_value()) {
        // parity: `Block.updateOrDestroy`, which is what runs a shape update's answer: a state is
        // written, but **air** is a `destroyBlock` and that one drops what was standing there. Half a
        // door left alone comes apart and leaves its door on the floor.
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
        const std::int32_t neighbour =
            stateAt(world, x + registry::stepX(face), y + registry::stepY(face), z + registry::stepZ(face));
        wanted[std::string{name}] = registry::blockConnections::connectsTo(stateId, neighbour, face) ? "true" : "false";
    }
    return rewrite(world, x, y, z, stateId, wanted);
}

std::optional<std::int32_t> reshape(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                                    std::optional<BlockFace> towards) {
    WorldState& world = context.world;
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    // parity: the blocks whose `updateShape` books a turn and changes nothing now. Doing it here
    // instead made tilled soil give way a tick early and coral go white on the spot, where the jar
    // leaves it its three seconds of colour.
    if (towards == BlockFace::Up) {
        scheduledBlocks::scheduleCoveredSoil(world, x, y, z);
    }
    scheduledBlocks::scheduleCoralDeath(world, x, y, z, context.random.nextLong());
    scheduledBlocks::scheduleCreakingHeart(world, x, y, z);
    // parity: `ScaffoldingBlock.updateShape` answers the state it was given and books a turn for the
    // next tick. Counting the distance here made a whole run collapse in one tick instead of walking
    // out from the footing a square at a time. Measured in `andamio`.
    scheduledBlocks::scheduleScaffolding(world, x, y, z);
    // A column is more than one block, so it reports through the caller rather than as one state.
    scheduledBlocks::scheduleBubbleColumn(world, x, y, z);
    if (const std::optional<std::int32_t> tuned = noteBlocks::retune(world, x, y, z, *stateId); tuned.has_value()) {
        return tuned;
    }
    // parity: `SnowyBlock.updateShape`, the white top grass wears while snow sits on it.
    if (const std::optional<std::int32_t> snowy = snowyBlocks::updated(world, x, y, z); snowy.has_value()) {
        if (world.setBlockState(x, y, z, *snowy)) {
            return snowy;
        }
    }
    // parity: `RepeaterBlock.updateShape`, which rewrites `locked` from what the sides say.
    if (const std::optional<std::int32_t> held = redstoneComponents::relock(world, x, y, z); held.has_value()) {
        return held;
    }
    // parity: `TripWireHookBlock.updateShape`, which drops the hook **only** when the news comes from
    // the side it hangs on. From anywhere else it stays, which is why one put down in mid air by
    // command lives until something touches its wall.
    if (towards.has_value() && tripwires::hookFalls(world, x, y, z, *stateId, *towards)) {
        return registry::blocks::air().stateId;
    }
    // parity: `BellBlock.updateShape`, the one that rewrites the state instead of only deciding
    // whether the block stays: a bell between two walls that loses one hangs on the other.
    if (towards.has_value()) {
        if (const std::optional<std::int32_t> rung = blockInteractions::bellShape(world, x, y, z, *stateId, *towards);
            rung.has_value()) {
            return rung;
        }
    }
    // parity: `TripWireBlock.updateShape`, which draws a line of string as a line the same way the
    // dust does: every horizontal side follows `shouldConnectTo`.
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

void settleArrivedSignal(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                         const OnReshaped& onReshaped) {
    // parity: `SculkSensorBlock.onPlace` and `TargetBlock.onPlace`, which are the same four lines:
    // a block that arrives already carrying a signal and with no turn booked to spend it puts that
    // signal back to zero. A `/setblock` of a target with `power=15` reads fifteen here and nothing
    // in the jar, because a signal with no clock behind it is a signal that would never go out.
    const std::optional<std::int32_t> arrived = world.blockState(x, y, z);
    if (!arrived.has_value()) {
        return;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*arrived);
    if (block == nullptr) {
        return;
    }
    const std::string_view path = block->path();
    // parity: `LightningRodBlock.onPlace`, the same idea with the other answer: a rod that arrives
    // powered keeps its signal and books the eight turns that spend it, instead of dropping it.
    if (path.ends_with("lightning_rod")) {
        const std::map<std::string, std::string> rod = registry::blockStateDefinitions::propertiesOf(*arrived);
        const auto powered = rod.find("powered");
        if (powered != rod.end() && powered->second == "true" && !world.hasScheduledBlockTick(x, y, z, *block)) {
            world.scheduleBlockTick(x, y, z, *block, lightning::kRodTicks);
        }
        return;
    }
    // parity: `ComposterBlock.onPlace`, which books the twenty turns that finish a composter that
    // arrives one layer short of ready. This one books without asking whether a turn is already
    // there: two of the same never fit anyway.
    if (path == "composter") {
        const std::map<std::string, std::string> heap = registry::blockStateDefinitions::propertiesOf(*arrived);
        const auto level = heap.find("level");
        if (level != heap.end() && level->second == std::to_string(composters::kFull)) {
            world.scheduleBlockTick(x, y, z, *block, composters::kReadyDelay);
        }
        return;
    }
    // parity: `ObserverBlock.onPlace`, which drops the signal like the sensor does and then tells
    // what it was feeding, because an observer's pulse does reach two squares.
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
    while (!pending.empty() && examined < kMaxCascade) {
        const Position at = pending.front();
        pending.pop_front();
        examined += kDirections;
        for (const BlockFace face : kUpdateShapeOrder) {
            visit(walk, pending, at[0] + registry::stepX(face), at[1] + registry::stepY(face),
                  at[2] + registry::stepZ(face), registry::opposite(face));
        }
    }
}

namespace {

/// parity: `LevelEvent.PARTICLES_DESTROY_BLOCK`, the puff `clearPatternBlocks` leaves behind.
constexpr std::int32_t kBlockDestroyedEffect = 2001;

/// parity: `spawnGolemInWorld`'s `snapTo`, which stands the golem a twentieth of a block up.
constexpr double kGolemFootroom = 0.05;

/// Whether the block that just arrived finished a golem, in which case it is already gone. parity:
/// `CarvedPumpkinBlock.onPlace` runs inside `setBlockState`, so everything `Level.setBlock` does
/// afterwards is behind `newState == blockState` and never happens. What tells the neighbours is
/// `updatePatternBlocks`, one `updateNeighborsAt` per square, once the golem is standing.
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
    // parity: `clearPatternBlocks` writes air with flag 2, which reaches the clients and tells
    // nothing around it, and raises the break particles of whatever it took away.
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
    // parity: `replaceCopperBlockWithChest` is the last thing `trySpawnGolem` does, after the squares
    // have already told their neighbours, and it writes with flag 2: the clients hear about it and
    // nobody is asked again. Flag 2 still redraws the squares **around** it, which is what turns the
    // chest beside it into the other half of a double one.
    if (match->kind == golemPatterns::Golem::Copper) {
        const wire::common::Identifier* body = registry::blockStateDefinitions::blockOf(match->bodyState);
        const std::map<std::string, std::string> pumpkin =
            registry::blockStateDefinitions::propertiesOf(match->pumpkinState);
        const auto facing = pumpkin.find("facing");
        if (body != nullptr && facing != pumpkin.end()) {
            const std::optional<std::int32_t> chest =
                copperChests::placedState(world, match->body[0], match->body[1], match->body[2],
                                          copperChests::chestForBlock(body->path()), facing->second);
            if (chest.has_value() && world.setBlockState(match->body[0], match->body[1], match->body[2], *chest)) {
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
                 const OnReshaped& onReshaped, bool reshapeSelf, const std::function<void()>& afterShapes) {
    WorldState& world = context.world;
    const OnReshaped reshaped = onReshaped ? onReshaped : OnReshaped{noop};
    const OnBroken broken = onBroken ? onBroken : OnBroken{noop};
    if (buildGolem(context, x, y, z, effects, broken, reshaped)) {
        return;
    }
    // Scaffolding counts out from its footing, which is a walk over the whole row rather than
    // anything one neighbour can answer, so the survival check is handed the walk.
    const blockSurvival::ScaffoldingHasFallen scaffolding = [&world](const WorldState&, std::int32_t sx,
                                                                     std::int32_t sy, std::int32_t sz) {
        return scheduledBlocks::scaffoldingHasFallen(world, sx, sy, sz);
    };

    // parity: `RedStoneWireBlock.onPlace` tells the blocks over and under it to update their own
    // neighbours, and this dust is one of those, so a dust with nothing to sit on goes at once,
    // before anything around it has been reshaped. That is what leaves the dust beside it a dot
    // instead of a cross: it never sees this one arrive.
    if (const std::optional<std::int32_t> arrived = world.blockState(x, y, z);
        arrived.has_value() && redstoneWire::isWire(*arrived)) {
        // parity: `onPlace` settles this dust's own strength before it tells anybody, and that write
        // redraws the squares around it while this dust is still there. It is the whole difference
        // between leaving the dust beside it a dot and leaving it a cross: with no signal the
        // strength does not move, nothing is written, and the neighbour never sees this dust arrive.
        for (const BlockChange& change : redstone::settle(world, context.worldName, x, y, z)) {
            reshaped(change.stateId, change.x, change.y, change.z);
            shapeAround(context, change.x, change.y, change.z, reshaped);
        }
        static_cast<void>(breakIfUnsupported(context, x, y, z, scaffolding, broken));
    }
    if (const std::optional<std::int32_t> arrived = world.blockState(x, y, z); arrived.has_value()) {
        const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*arrived);
        const std::string_view path = block == nullptr ? std::string_view{} : block->path();
        // parity: `PotentSulfurBlock.onPlace` raises `BLOCK_ACTIVATE` for a geyser that arrives
        // already going, which is the noise of it starting.
        if (path == "potent_sulfur") {
            const std::map<std::string, std::string> properties =
                registry::blockStateDefinitions::propertiesOf(*arrived);
            const auto state = properties.find("potent_sulfur_state");
            if (state != properties.end() && (state->second == "erupting" || state->second == "continuous") &&
                world.hasSculkSensorNear(x, y, z, vibrations::kCalibratedRange)) {
                vibrations::emit(world, "block_activate", x + 0.5, y + 0.5, z + 0.5, context.gameTime);
            }
        }
        // parity: `ShelfBlock.affectNeighborsAfterRemoval`: the square that held a shelf and holds
        // one no longer lets its neighbours go. Measured in `estantequita`.
        if (removed.has_value()) {
            std::vector<BlockChange> parted;
            shelves::leftBehind(world, x, y, z, *removed, parted);
            for (const BlockChange& change : parted) {
                reshaped(change.stateId, change.x, change.y, change.z);
            }
            // parity: `PistonHeadBlock.affectNeighborsAfterRemoval`, which is `destroyBlock` on the
            // body behind it, drops and all: taking the head off takes the piston with it. It never
            // fires when the piston pulls its own head back, because that write carries neither
            // `UPDATE_NEIGHBORS` nor the piston flag. Measured in `pistoncabezarota`.
            if (const std::optional<std::array<std::int32_t, 3>> body = pistons::fittingBase(world, x, y, z, *removed);
                body.has_value()) {
                const std::optional<std::int32_t> was = world.blockState((*body)[0], (*body)[1], (*body)[2]);
                if (was.has_value() &&
                    world.setBlockState((*body)[0], (*body)[1], (*body)[2], registry::blocks::air().stateId)) {
                    broken(*was, (*body)[0], (*body)[1], (*body)[2]);
                }
            }
        }
        // parity: `ShelfBlock.onPlace`, which joins the chain for a shelf that arrives already
        // powered. Measured in `colocadoonplace`, where three of them link up in the jar as the
        // third one lands and stayed apart here.
        if (shelves::isShelf(path)) {
            std::vector<BlockChange> joined;
            shelves::arrived(world, x, y, z, removed.has_value() && wasConnectedShelf(*removed), joined);
            for (const BlockChange& change : joined) {
                reshaped(change.stateId, change.x, change.y, change.z);
            }
        }
    }
    settleArrivedSignal(world, x, y, z, reshaped);
    // parity: `SugarCaneBlock.updateShape` runs on the cane itself as it lands, and that is where a
    // cane with nothing to drink books the turn that breaks it.
    scheduledBlocks::scheduleBreakIfLoose(
        world, x, y, z, blockSurvival::isSupported(world, x, y, z, std::nullopt, scaffolding, context.hasSkylight));
    // parity: and `ScaffoldingBlock.updateShape` runs on the piece as it lands too, which is why a
    // piece put down by hand keeps the `bottom` it was given until its first turn.
    scheduledBlocks::scheduleScaffolding(world, x, y, z);
    // parity: `CoralBlock.onPlace` and the three `BaseCoralPlantTypeBlock` ones book the die turn for
    // the coral **as it lands**, not only when a neighbour moves: one put down out of water arrives
    // with its count already running. The shape walk above only ever reaches the neighbours.
    scheduledBlocks::scheduleCoralDeath(world, x, y, z, context.random.nextLong());
    // parity: `CreakingHeartBlock.onPlace` is `updateShape`'s own turn, booked the same way.
    scheduledBlocks::scheduleCreakingHeart(world, x, y, z);

    // The block that just arrived reaches out to what was already there, which is the other half of
    // `updateShape`: a fence put down beside a fence joins it, not only the other way.
    if (reshapeSelf) {
        if (const std::optional<std::int32_t> moved = reshape(context, x, y, z); moved.has_value()) {
            reshaped(*moved, x, y, z);
        }
    }
    // parity: `RedStoneWireBlock.updateIndirectNeighbourShapes`, the one place a dust reaches past
    // its own six sides: the dust one below and one above the square it connects to are told to look
    // again. It runs on the state the block ends up with, not the one it arrived as, so it goes
    // after the reshape above; without it a line up a staircase is drawn as if the steps were flat.
    for (const redstoneWire::Step& at : redstoneWire::acrossSteps(world, x, y, z)) {
        if (const std::optional<std::int32_t> drawn = redstoneWire::reshape(
                world, at.x, at.y, at.z, world.blockState(at.x, at.y, at.z).value_or(0), at.towards);
            drawn.has_value()) {
            reshaped(drawn.value(), at.x, at.y, at.z);
        }
    }
    // parity: `FallingBlock.onPlace` and its `updateShape`, which both do nothing but ask to be
    // looked at again in two ticks. The block that just arrived counts: sand put down in mid air
    // used to sit there for good, because only its neighbours were ever asked.
    fallingBlocks::scheduleIfFalling(world, x, y, z);
    fallingBlocks::scheduleIfFalling(world, x, y + 1, z);
    // parity: `SnifferEggBlock.onPlace`, which asks for the first of its three turns as soon as the
    // egg is down. Re-scheduling keeps a delay already counting, so a neighbour moving nearby does
    // not put the egg back to the start.
    scheduledBlocks::scheduleHatch(world, x, y, z, context.random.nextLong());
    // parity: `FrostedIceBlock.onPlace`, which books the melt clock of every piece that lands.
    scheduledBlocks::scheduleFrostedIce(world, x, y, z, context.random);
    // parity: `FireBlock.onPlace`, which books a turn for every fire that lands and does it without
    // asking whether the block changed. Soul fire is a `BaseFireBlock` and books none, which is why
    // it never burns out. Without this a fire put down by command sat there for ever.
    if (const std::optional<std::int32_t> lit = world.blockState(x, y, z); lit.has_value() && fires::isFire(*lit)) {
        fires::schedule(world, x, y, z, context.random);
    }
    // parity: the neighbour update every redstone component listens to: the dust settles at once and
    // the components book their own turn, which is what their delay is.
    // parity: `Level.setBlock` updates the shapes around what changed before anything reads the
    // power off them, and a dust with its connection still unset points nowhere: a lamp at the end
    // of a line asked the dust next to it, saw `east=none` with power already at fifteen, and stayed
    // dark. The shapes go first, then the signal.
    const Walk walk{.context = context, .scaffolding = scaffolding, .onReshaped = reshaped, .onBroken = broken};
    std::deque<Position> pending;
    pending.push_back({x, y, z});
    std::int32_t examined = 0;
    while (!pending.empty() && examined < kMaxCascade) {
        const Position at = pending.front();
        pending.pop_front();
        examined += kDirections;
        // parity: `BlockBehaviour.updateNeighbourShapes` walks `UPDATE_SHAPE_ORDER`, which is
        // neither `Direction.values()` nor `NeighborUpdater.UPDATE_ORDER`, and hands each neighbour
        // the side the news arrives from. A dust redraws differently depending on both.
        for (const BlockFace face : kUpdateShapeOrder) {
            visit(walk, pending, at[0] + registry::stepX(face), at[1] + registry::stepY(face),
                  at[2] + registry::stepZ(face), registry::opposite(face));
        }
    }
    const std::vector<BlockChange> settled = redstone::settle(world, context.worldName, x, y, z);
    for (const BlockChange& change : settled) {
        reshaped(change.stateId, change.x, change.y, change.z);
    }
    const redstoneComponents::ContainerSignalAt reading = comparatorReading::of(world);
    redstoneComponents::scheduleAround(world, context.worldName, x, y, z, true, reading);
    // parity: `DefaultRedstoneWireEvaluator.updatePowerStrength` ends with `updateNeighborsAt` on
    // **every** piece of dust whose strength moved and on each of that dust's six squares. Without
    // the first half a lamp at the end of a line never heard that the dust had lit up; without the
    // second, a repeater one square past the dust never heard either, and a chain of them stopped
    // at the first.
    for (const BlockChange& change : settled) {
        redstoneComponents::scheduleAroundWire(world, context.worldName, change.x, change.y, change.z, reading);
    }
    // parity: `ObserverBlock.updateShape`, which fires for the block it is watching. Every write
    // updates the shapes around it, so a piece of dust that lit up wakes an observer looking at it
    // just as much as a block somebody placed.
    redstoneComponents::watchFor(world, x, y, z);
    redstoneComponents::watchOnPlace(world, x, y, z);
    for (const BlockChange& change : settled) {
        redstoneComponents::watchFor(world, change.x, change.y, change.z);
    }
    // parity: every `neighborChanged` that reads the power: doors, lamps, bulbs and the rest.
    //
    // parity: a reactor writes with `UPDATE_ALL`, so whatever it changed goes on to tell its own
    // neighbours. That is what carries a signal down a line of powered rails past the first one:
    // `PoweredRailBlock.updateState` only ever looks at the rail it is asked about.
    if (afterShapes) {
        afterShapes();
    }
    std::deque<Position> reacting;
    reacting.push_back({x, y, z});
    // parity: `DiodeBlock.updateNeighborsInFront` from `onPlace` and from
    // `affectNeighborsAfterRemoval`, plus `RedstoneTorchBlock.notifyNeighbors`, which tell the square
    // they feed **and its own neighbours**. The state that has just gone is what says where a
    // component that somebody took out was feeding, so both states are asked.
    const std::optional<std::int32_t> standing = world.blockState(x, y, z);
    std::vector<redstoneComponents::Reach> reach;
    if (standing.has_value() && !isObserverState(*standing)) {
        for (const redstoneComponents::Reach& at : redstoneComponents::outputReach(*standing, x, y, z)) {
            reach.push_back(at);
        }
    }
    if (removed.has_value() && (!standing.has_value() || !sameBlock(*removed, *standing))) {
        for (const redstoneComponents::Reach& at : redstoneComponents::outputReach(*removed, x, y, z)) {
            reach.push_back(at);
        }
    }
    for (const redstoneComponents::Reach& at : reach) {
        std::vector<BlockChange> ahead;
        if (at.skip.has_value()) {
            redstoneComponents::scheduleInFront(world, context.worldName, at.x, at.y, at.z, *at.skip, reading);
            ahead = redstoneReactors::updateInFront(world, context.worldName, at.x, at.y, at.z, *at.skip,
                                                    context.gameTime, context.random, context.tntExplodes);
        } else {
            // parity: `RedstoneTorchBlock.notifyNeighbors` is a plain `updateNeighborsAt` on each of
            // the six, so nothing is left out and the square itself is not asked.
            redstoneComponents::scheduleAround(world, context.worldName, at.x, at.y, at.z, false, reading);
            ahead = redstoneReactors::update(world, context.worldName, at.x, at.y, at.z, context.gameTime,
                                             context.random, context.tntExplodes, false);
        }
        for (const BlockChange& change : ahead) {
            reshaped(change.stateId, change.x, change.y, change.z);
            if (!change.effects.empty()) {
                effects.push_back(change);
            }
            redstoneComponents::watchFor(world, change.x, change.y, change.z);
            reacting.push_back({change.x, change.y, change.z});
        }
    }
    // parity: `onPlace`, the only half of a write the square that changed can answer for itself.
    // Everything else reacts from `neighborChanged`, and `Level.updateNeighborsAt` never tells the
    // square it is called for: a lamp put down beside a lit torch stays dark until something pokes it.
    for (const BlockChange& change : redstoneReactors::placed(world, context.worldName, x, y, z, context.gameTime,
                                                              context.random, context.tntExplodes)) {
        reshaped(change.stateId, change.x, change.y, change.z);
        if (!change.effects.empty()) {
            effects.push_back(change);
        }
        redstoneComponents::watchFor(world, change.x, change.y, change.z);
        reacting.push_back({change.x, change.y, change.z});
    }
    // parity: `DefaultRedstoneWireEvaluator.updatePowerStrength` runs `updateNeighborsAt` on the dust
    // that moved **and on each of its six squares**, so a lamp stuck to a block the dust powers from
    // above is reached even though nothing touches it. The seven go into a `HashSet` first, so the
    // order is the map's and `wireReach` is what works it out.
    for (const BlockChange& change : settled) {
        for (const std::array<std::int32_t, 3>& at : redstoneComponents::wireReach(change.x, change.y, change.z)) {
            reacting.push_back({at[0], at[1], at[2]});
        }
    }
    std::int32_t reacted = 0;
    while (!reacting.empty() && reacted < kMaxCascade) {
        const Position at = reacting.front();
        reacting.pop_front();
        ++reacted;
        for (const BlockChange& change :
             redstoneReactors::update(world, context.worldName, at[0], at[1], at[2], context.gameTime, context.random,
                                      context.tntExplodes, false)) {
            reshaped(change.stateId, change.x, change.y, change.z);
            if (!change.effects.empty()) {
                effects.push_back(change);
            }
            redstoneComponents::watchFor(world, change.x, change.y, change.z);
            reacting.push_back({change.x, change.y, change.z});
        }
    }
    // parity: `DispenserBlock.neighborChanged`, which remembers it was triggered and books a turn.
    const auto arm = [&context, &world](std::int32_t atX, std::int32_t atY, std::int32_t atZ) {
        return context.dispenser ? context.dispenser(world, atX, atY, atZ)
                                 : dispensers::onNeighbourChange(world, context.worldName, atX, atY, atZ);
    };
    const pistons::Context piston{.world = world,
                                  .worldName = context.worldName,
                                  .minY = context.minY,
                                  .maxY = context.maxY,
                                  .random = context.random};
    // parity: `Level.updateNeighborsAt`, the six squares around one that changed, each hearing the
    // whole of its own `neighborChanged`: the rail re-bends (`RailBlock.updateState`), the dispenser
    // remembers it was triggered, the crafter does the same with its own four ticks, and the piston
    // books its move (`PistonBaseBlock.checkIfExtend`).
    const bool wasIce = removed.has_value() && registry::blockStateDefinitions::blockOf(*removed) != nullptr &&
                        registry::blockStateDefinitions::blockOf(*removed)->path() == "frosted_ice";
    const auto tellAround = [&](std::int32_t cx, std::int32_t cy, std::int32_t cz) {
        const std::optional<std::int32_t> source = world.blockState(cx, cy, cz);
        for (const BlockFace face : kUpdateOrder) {
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
            // parity: `SpongeBlock.neighborChanged` drinks when the **neighbour** changes too, not
            // only when the sponge itself arrives. A sponge put down before the water stayed dry.
            sponges::absorb(world, nx, ny, nz, reshaped);
            // parity: `FrostedIceBlock.neighborChanged`, which only listens to other frosted ice.
            // Which block the news carries is not the same on both roads: `Level.setBlock` hands the
            // **old** one (`updateNeighborsAt(pos, oldState.getBlock())`) and a command hands the new
            // one (`ServerLevel.updateNeighboursOnBlockSet`). Frosted ice is the only block that asks,
            // and it melts either way, so both are enough here.
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
    tellAround(x, y, z);
    if (const std::optional<BlockChange> armed = arm(x, y, z); armed.has_value()) {
        reshaped(armed->stateId, armed->x, armed->y, armed->z);
    }
    if (const std::optional<BlockChange> armed = crafters::onNeighbourChange(world, context.worldName, x, y, z);
        armed.has_value()) {
        reshaped(armed->stateId, armed->x, armed->y, armed->z);
    }
    pistons::trigger(piston, x, y, z);
    // parity: and the same for every square a dust that moved reaches, which is the dust and its six
    // squares (`DefaultRedstoneWireEvaluator.updatePowerStrength`). Without this a piston stuck to a
    // line of dust never heard it light up: only the components did.
    for (const BlockChange& change : settled) {
        for (const std::array<std::int32_t, 3>& at : redstoneComponents::wireReach(change.x, change.y, change.z)) {
            tellAround(at[0], at[1], at[2]);
        }
    }
    // parity: `TripWireBlock.onPlace`, which tells the hooks at the ends of its line. Without it a
    // tripwire built by hand or by command hung there unattached until something stepped on it.
    if (const std::optional<std::int32_t> laid = world.blockState(x, y, z);
        laid.has_value() && tripwires::isWire(*laid)) {
        tripwires::updateSource(world, x, y, z, *laid, context.gameTime, effects);
    }
    // parity: `TripWireBlock.affectNeighborsAfterRemoval`, which hands the line the string that has
    // just gone **as if it were still there and powered**, so the hooks fire and each books itself a
    // look ten ticks later to find the gap. Every other block's version of this hook is a plain "tell
    // the neighbours", which the cascade above already does; this is the only one that substitutes a
    // state, and without it a cut wire left both hooks hanging loose at once.
    if (removed.has_value() && tripwires::isWire(*removed)) {
        if (const std::optional<std::int32_t> live =
                registry::blockStateDefinitions::stateIdOf(tripwires::wireBlock(), {{"powered", "true"}}, *removed);
            live.has_value()) {
            tripwires::updateSource(world, x, y, z, *live, context.gameTime, effects);
        }
    }
    // parity: `LiquidBlock`'s three hooks, which all end in the same `scheduleTick`: water only
    // notices there is somewhere to go when something around it changes. Measured in `aguapolvo`,
    // where a `/setblock` of water simply stood there for ever because nobody woke it.
    if (context.wakeFluids) {
        context.wakeFluids(x, y, z);
    }
    // parity: `SpongeBlock.onPlace`, which drinks before anything else gets a turn.
    sponges::absorb(world, x, y, z, reshaped);
    sponges::dryIfEvaporating(world, x, y, z, reshaped, context.evaporates);
    // parity: `FrogspawnBlock.onPlace`, the same idea on a longer and rougher clock.
    scheduledBlocks::scheduleFrogspawn(world, x, y, z, context.random.nextLong());
}

} // namespace basalt::server::runtime::world::neighbourUpdates
