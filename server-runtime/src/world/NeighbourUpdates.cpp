#include "NeighbourUpdatesShared.hpp"

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
#include <atomic>
#include <charconv>
#include <cstdint>
#include <deque>
#include <limits>
#include <string>
#include <utility>

namespace basalt::server::runtime::world::neighbourUpdates {

namespace {
std::atomic<std::int32_t> gChainedLimit{kMaxChainedNeighbourUpdates};
} // namespace

std::int32_t chainedLimit() noexcept { return gChainedLimit.load(std::memory_order_relaxed); }

void setChainedLimit(std::int32_t limit) noexcept {
    gChainedLimit.store(limit < 0 ? std::numeric_limits<std::int32_t>::max() : limit, std::memory_order_relaxed);
}
using namespace detail;

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

} // namespace basalt::server::runtime::world::neighbourUpdates
