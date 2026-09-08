#include <basalt/server/registry/BlockConnections.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/BlockSurvival.hpp>
#include <basalt/server/runtime/world/Crafters.hpp>
#include <basalt/server/runtime/world/Dispensers.hpp>
#include <basalt/server/runtime/world/FallingBlocks.hpp>
#include <basalt/server/runtime/world/NeighbourUpdates.hpp>
#include <basalt/server/runtime/world/NoteBlocks.hpp>
#include <basalt/server/runtime/world/Pistons.hpp>
#include <basalt/server/runtime/world/Rails.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/RedstoneWire.hpp>
#include <basalt/server/runtime/world/ScheduledBlocks.hpp>
#include <basalt/server/runtime/world/SnowyBlocks.hpp>
#include <basalt/server/runtime/world/Sponges.hpp>
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

/// The four a connecting block keeps a property for, named as those properties are.
constexpr std::array<std::pair<BlockFace, std::string_view>, 4> kHorizontal{
    {{BlockFace::North, "north"}, {BlockFace::South, "south"}, {BlockFace::West, "west"}, {BlockFace::East, "east"}}};

using Position = std::array<std::int32_t, 3>;

void noop(std::int32_t, std::int32_t, std::int32_t, std::int32_t) {}

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
                                                       std::int32_t z, std::int32_t stateId) {
    return rewrite(world, x, y, z, stateId, {{"distance", std::to_string(leafDistance(world, x, y, z))}});
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
                                      const blockSurvival::ScaffoldingHasFallen& scaffolding,
                                      const OnBroken& onBroken) {
    if (y < context.minY || y > context.maxY) {
        return false;
    }
    WorldState& world = context.world;
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || *stateId == registry::blocks::air().stateId) {
        return false;
    }
    if (blockSurvival::isSupported(world, x, y, z, *stateId, scaffolding, context.hasSkylight)) {
        return false;
    }
    world.setBlockState(x, y, z, registry::blocks::air().stateId);
    onBroken(*stateId, x, y, z);
    return true;
}

/// What one step of the cascade carries with it, so `visit` keeps a signature worth reading.
struct Walk {
    const Context& context;
    const blockSurvival::ScaffoldingHasFallen& scaffolding;
    const OnReshaped& onReshaped;
    const OnBroken& onBroken;
};

/// Reshapes the block there, breaks it if nothing holds it, and follows on from what it held.
void visit(const Walk& walk, std::deque<Position>& pending, std::int32_t x, std::int32_t y, std::int32_t z) {
    fallingBlocks::scheduleIfFalling(walk.context.world, x, y, z);
    bool changed = false;
    std::vector<BlockChange> bubbles;
    if (const std::optional<std::int32_t> reshaped = reshape(walk.context, x, y, z, bubbles); reshaped.has_value()) {
        walk.onReshaped(*reshaped, x, y, z);
        changed = true;
    }
    for (const BlockChange& change : bubbles) {
        walk.onReshaped(change.stateId, change.x, change.y, change.z);
        changed = true;
    }
    if (breakIfUnsupported(walk.context, x, y, z, walk.scaffolding, walk.onBroken) || changed) {
        pending.push_back({x, y, z});
    }
}

} // namespace

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
                                    std::int32_t stateId) {
    WorldState& world = context.world;
    if (registry::blockConnections::isLeaves(stateId)) {
        return retreeLeaves(world, x, y, z, stateId);
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
                                    std::vector<BlockChange>& bubbles) {
    WorldState& world = context.world;
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    // parity: the scheduled ticks that change a block into something else rather than break it.
    // Vanilla defers these by a tick; the outcome is the same and this is where it is known.
    if (const std::optional<std::int32_t> soil = scheduledBlocks::coveredSoil(world, x, y, z); soil.has_value()) {
        return soil;
    }
    if (const std::optional<std::int32_t> dead = scheduledBlocks::coral(world, x, y, z); dead.has_value()) {
        return dead;
    }
    if (const std::optional<std::int32_t> counted = scheduledBlocks::scaffolding(world, x, y, z); counted.has_value()) {
        return counted;
    }
    // A column is more than one block, so it reports through the caller rather than as one state.
    scheduledBlocks::bubbleColumn(world, x, y, z, bubbles);
    if (const std::optional<std::int32_t> tuned = noteBlocks::retune(world, x, y, z, *stateId); tuned.has_value()) {
        return tuned;
    }
    // parity: `SnowyBlock.updateShape`, the white top grass wears while snow sits on it.
    if (const std::optional<std::int32_t> snowy = snowyBlocks::updated(world, x, y, z); snowy.has_value()) {
        if (world.setBlockState(x, y, z, *snowy)) {
            return snowy;
        }
    }
    // parity: `RedStoneWireBlock.updateShape`, which is what draws a line of dust as a line.
    if (const std::optional<std::int32_t> drawn = context.wireReshape ? context.wireReshape(world, x, y, z, *stateId)
                                                                      : redstoneWire::reshape(world, x, y, z, *stateId);
        drawn.has_value()) {
        return drawn;
    }
    if (const std::optional<std::int32_t> other = reshapes::of(world, x, y, z, *stateId, context.geyser);
        other.has_value()) {
        return other;
    }
    return connect(context, x, y, z, *stateId);
}

void afterChange(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                 std::vector<BlockChange>& effects, const OnBroken& onBroken, const OnReshaped& onReshaped,
                 bool reshapeSelf) {
    WorldState& world = context.world;
    const OnReshaped reshaped = onReshaped ? onReshaped : OnReshaped{noop};
    const OnBroken broken = onBroken ? onBroken : OnBroken{noop};
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
        static_cast<void>(breakIfUnsupported(context, x, y, z, scaffolding, broken));
    }

    // The block that just arrived reaches out to what was already there, which is the other half of
    // `updateShape`: a fence put down beside a fence joins it, not only the other way.
    std::vector<BlockChange> bubbles;
    if (reshapeSelf) {
        if (const std::optional<std::int32_t> moved = reshape(context, x, y, z, bubbles); moved.has_value()) {
            reshaped(*moved, x, y, z);
        }
        for (const BlockChange& change : bubbles) {
            reshaped(change.stateId, change.x, change.y, change.z);
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
        visit(walk, pending, at[0], at[1] - 1, at[2]);
        visit(walk, pending, at[0], at[1] + 1, at[2]);
        visit(walk, pending, at[0], at[1], at[2] - 1);
        visit(walk, pending, at[0], at[1], at[2] + 1);
        visit(walk, pending, at[0] - 1, at[1], at[2]);
        visit(walk, pending, at[0] + 1, at[1], at[2]);
    }
    const std::vector<BlockChange> settled = redstone::settle(world, context.worldName, x, y, z);
    for (const BlockChange& change : settled) {
        reshaped(change.stateId, change.x, change.y, change.z);
    }
    redstoneComponents::scheduleAround(world, x, y, z);
    // parity: `RedStoneWireBlock.updatePowerStrength` ends with `updateNeighborsAt` on **every**
    // piece of dust whose strength moved, not only on the block that started it. Without this a
    // lamp at the end of a line of dust never heard that the dust had lit up, because it is two
    // blocks away from whatever was placed.
    for (const BlockChange& change : settled) {
        redstoneComponents::scheduleAround(world, change.x, change.y, change.z);
    }
    // parity: `ObserverBlock.neighborChanged`, which fires only for the block it is watching.
    redstoneComponents::watchFor(world, x, y, z);
    // parity: every `neighborChanged` that reads the power: doors, lamps, bulbs and the rest.
    const auto reactAround = [&](std::int32_t atX, std::int32_t atY, std::int32_t atZ) {
        for (const BlockChange& change :
             redstoneReactors::update(world, context.worldName, atX, atY, atZ, context.random, context.reactorHooks)) {
            reshaped(change.stateId, change.x, change.y, change.z);
            if (!change.effects.empty()) {
                effects.push_back(change);
            }
        }
    };
    reactAround(x, y, z);
    for (const BlockChange& change : settled) {
        reactAround(change.x, change.y, change.z);
    }
    // parity: `RailBlock.updateState`, the one case a rail re-bends on a neighbour change: a
    // three-way junction with a signal arriving at it.
    if (const std::optional<std::int32_t> changed = world.blockState(x, y, z); changed.has_value()) {
        for (const BlockFace face : kFaces) {
            const std::int32_t nx = x + registry::stepX(face);
            const std::int32_t ny = y + registry::stepY(face);
            const std::int32_t nz = z + registry::stepZ(face);
            if (const std::optional<std::int32_t> bent =
                    rails::onNeighbourChange(world, context.worldName, nx, ny, nz, *changed);
                bent.has_value()) {
                reshaped(*bent, nx, ny, nz);
            }
        }
    }
    // parity: `DispenserBlock.neighborChanged`, which remembers it was triggered and books a turn.
    const auto arm = [&context, &world](std::int32_t atX, std::int32_t atY, std::int32_t atZ) {
        return context.dispenser ? context.dispenser(world, atX, atY, atZ)
                                 : dispensers::onNeighbourChange(world, context.worldName, atX, atY, atZ);
    };
    for (const BlockFace face : kFaces) {
        if (const std::optional<BlockChange> armed =
                arm(x + registry::stepX(face), y + registry::stepY(face), z + registry::stepZ(face));
            armed.has_value()) {
            reshaped(armed->stateId, armed->x, armed->y, armed->z);
        }
    }
    if (const std::optional<BlockChange> armed = arm(x, y, z); armed.has_value()) {
        reshaped(armed->stateId, armed->x, armed->y, armed->z);
    }
    // parity: `CrafterBlock.neighborChanged`, the same rising edge with its own four ticks.
    if (const std::optional<BlockChange> armed = crafters::onNeighbourChange(world, context.worldName, x, y, z);
        armed.has_value()) {
        reshaped(armed->stateId, armed->x, armed->y, armed->z);
    }
    for (const BlockFace face : kFaces) {
        if (const std::optional<BlockChange> armed =
                crafters::onNeighbourChange(world, context.worldName, x + registry::stepX(face),
                                            y + registry::stepY(face), z + registry::stepZ(face));
            armed.has_value()) {
            reshaped(armed->stateId, armed->x, armed->y, armed->z);
        }
    }
    // parity: `PistonBaseBlock.checkIfExtend`, which books the move rather than making it now.
    const pistons::Context piston{.world = world,
                                  .worldName = context.worldName,
                                  .minY = context.minY,
                                  .maxY = context.maxY,
                                  .random = context.random};
    pistons::trigger(piston, x, y, z);
    for (const BlockFace face : kFaces) {
        pistons::trigger(piston, x + registry::stepX(face), y + registry::stepY(face), z + registry::stepZ(face));
    }
    // parity: `SpongeBlock.onPlace`, which drinks before anything else gets a turn.
    sponges::absorb(world, x, y, z, reshaped);
    sponges::dryIfEvaporating(world, x, y, z, reshaped, context.evaporates);
    // parity: `FrogspawnBlock.onPlace`, the same idea on a longer and rougher clock.
    scheduledBlocks::scheduleFrogspawn(world, x, y, z, context.random.nextLong());
}

} // namespace basalt::server::runtime::world::neighbourUpdates
