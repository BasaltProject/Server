#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/Tripwires.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace basalt::server::runtime::world::tripwires {
namespace {

using registry::BlockFace;
using Properties = std::map<std::string, std::string>;

constexpr std::array<BlockFace, 4> kHorizontal{BlockFace::North, BlockFace::South, BlockFace::West, BlockFace::East};

/// How far below the block a thing still counts as crossing it, since a wire lies on the floor.
constexpr double kCrossingSlack = 0.0625;

[[nodiscard]] std::string_view nameOf(BlockFace face) {
    switch (face) {
    case BlockFace::North:
        return "north";
    case BlockFace::South:
        return "south";
    case BlockFace::West:
        return "west";
    default:
        return "east";
    }
}

[[nodiscard]] std::string valueOf(const Properties& properties, const std::string& key) {
    const auto found = properties.find(key);
    return found == properties.end() ? std::string{} : found->second;
}

/// parity: `Level.gameEvent(source, event, pos)`, from the middle of the square. The index of sensors
/// is asked first because a tripwire on a busy corridor raises one of these on every crossing.
void heard(WorldState& world, std::string_view event, std::int32_t x, std::int32_t y, std::int32_t z,
           std::int64_t gameTime) {
    if (event.empty() || !world.hasSculkSensorNear(x, y, z, vibrations::kCalibratedRange)) {
        return;
    }
    vibrations::emit(world, event, x + 0.5, y + 0.5, z + 0.5, gameTime);
}

void write(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
           const wire::common::Identifier& block, bool attached, std::optional<bool> powered,
           std::vector<BlockChange>& changes) {
    Properties wanted{{"attached", attached ? "true" : "false"}};
    if (powered.has_value()) {
        wanted.emplace("powered", *powered ? "true" : "false");
    }
    const std::optional<std::int32_t> turned = registry::blockStateDefinitions::stateIdOf(block, wanted, stateId);
    if (!turned.has_value() || *turned == stateId) {
        return;
    }
    if (world.setBlockState(x, y, z, *turned)) {
        changes.push_back(BlockChange{.x = x, .y = y, .z = z, .stateId = *turned});
    }
}

} // namespace

const wire::common::Identifier& wireBlock() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("tripwire");
    return instance;
}

const wire::common::Identifier& hookBlock() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("tripwire_hook");
    return instance;
}

bool crosses(const std::int32_t x, const std::int32_t y, const std::int32_t z, const std::int32_t stateId,
             const double entityX, const double entityY, const double entityZ, const double halfWidth,
             const double height) {
    // parity: the two shapes are full width and differ only in how tall they are. A strung wire is a
    // tripwire you walk through at ankle height; a loose one fills half the block.
    const bool attached = valueOf(registry::blockStateDefinitions::propertiesOf(stateId), "attached") == "true";
    const double low = attached ? 1.0 / 16.0 : 0.0;
    const double high = attached ? 2.5 / 16.0 : 8.0 / 16.0;
    const auto overlaps = [](double lowA, double highA, double lowB, double highB) {
        return lowA < highB && highA > lowB;
    };
    return overlaps(entityX - halfWidth, entityX + halfWidth, x, x + 1.0) &&
           overlaps(entityY, entityY + height, y + low, y + high) &&
           overlaps(entityZ - halfWidth, entityZ + halfWidth, z, z + 1.0);
}

bool isStandingOn(std::int32_t x, std::int32_t y, std::int32_t z, double entityX, double entityY, double entityZ) {
    return entityX >= x && entityX <= x + 1 && entityZ >= z && entityZ <= z + 1 && entityY >= y - kCrossingSlack &&
           entityY <= y + 1;
}

/// parity: `TripWireBlock.shouldConnectTo`: another string, or a hook whose face points back at us.
bool connectsTo(const WorldState& world, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                const BlockFace towards) {
    const std::optional<std::int32_t> beside =
        world.blockState(x + registry::stepX(towards), y + registry::stepY(towards), z + registry::stepZ(towards));
    if (!beside.has_value()) {
        return false;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*beside);
    if (block == nullptr) {
        return false;
    }
    if (*block == wireBlock()) {
        return true;
    }
    return *block == hookBlock() && valueOf(registry::blockStateDefinitions::propertiesOf(*beside), "facing") ==
                                        nameOf(registry::opposite(towards));
}

std::optional<std::int32_t> reshape(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                    std::int32_t stateId, std::optional<BlockFace> towards) {
    if (!isWire(stateId)) {
        return std::nullopt;
    }
    // parity: only a horizontal neighbour draws anything; from above or below the string is left as
    // it is.
    Properties wanted;
    if (towards.has_value()) {
        if (registry::axisOf(*towards) == 1) {
            return std::nullopt;
        }
        wanted.emplace(std::string{nameOf(*towards)}, connectsTo(world, x, y, z, *towards) ? "true" : "false");
    } else {
        for (const BlockFace face : kHorizontal) {
            wanted.emplace(std::string{nameOf(face)}, connectsTo(world, x, y, z, face) ? "true" : "false");
        }
    }
    const std::optional<std::int32_t> drawn = registry::blockStateDefinitions::stateIdOf(wireBlock(), wanted, stateId);
    return drawn.has_value() && *drawn != stateId ? drawn : std::nullopt;
}

bool hookFalls(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
               BlockFace towards) {
    if (!isHook(stateId)) {
        return false;
    }
    const Properties properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const std::string named = valueOf(properties, "facing");
    const auto found = std::ranges::find_if(kHorizontal, [&named](BlockFace face) { return nameOf(face) == named; });
    if (found == kHorizontal.end() || registry::opposite(towards) != *found) {
        return false;
    }
    const BlockFace facing = *found;
    // parity: `canSurvive`, which wants the block it hangs on to have a sturdy face towards it.
    const BlockFace behind = registry::opposite(facing);
    const std::optional<std::int32_t> wall =
        world.blockState(x + registry::stepX(behind), y + registry::stepY(behind), z + registry::stepZ(behind));
    return !wall.has_value() || !registry::blockShapes::isFaceSturdy(*wall, facing);
}

bool isWire(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && *block == wireBlock();
}

bool isHook(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && *block == hookBlock();
}

void press(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, bool occupied, std::int64_t gameTime,
           std::vector<BlockChange>& changes) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr || *block != wireBlock()) {
        return;
    }
    const Properties properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const bool wasPressed = valueOf(properties, "powered") == "true";
    if (occupied != wasPressed) {
        const std::optional<std::int32_t> turned =
            registry::blockStateDefinitions::stateIdOf(*block, {{"powered", occupied ? "true" : "false"}}, *stateId);
        if (!turned.has_value()) {
            return;
        }
        if (world.setBlockState(x, y, z, *turned)) {
            changes.push_back(BlockChange{.x = x, .y = y, .z = z, .stateId = *turned});
        }
        // parity: `TripWireBlock.checkPressed` ends in `updateSource` with the wire's **new** state,
        // which is how the hook learns what happened without reading the square again.
        updateSource(world, x, y, z, *turned, gameTime, changes);
    }
    // parity: `checkPressed` books ten ticks while something is on it, which is what notices the
    // thing leaving, and a **zero** when it has just been let go, which is one last look on the very
    // next turn. That second one changes nothing on its own and is booked anyway, so the list of
    // pending ticks matches the jar's.
    if (occupied) {
        world.scheduleBlockTick(x, y, z, wireBlock(), kRecheckTicks);
    } else if (wasPressed) {
        world.scheduleBlockTick(x, y, z, wireBlock(), 0);
    }
}

void calculateHook(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int64_t gameTime,
                   std::vector<BlockChange>& changes, std::int32_t wireSource,
                   std::optional<std::int32_t> wireSourceState, std::optional<std::int32_t> destroyedState) {
    // parity: `isBeingDestroyed`. With the hook already out of the world the state it had is what is
    // worked with, its square is not rewritten, and the whole line comes out loose.
    const bool beingDestroyed = destroyedState.has_value();
    const std::optional<std::int32_t> stateId = beingDestroyed ? destroyedState : world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr || *block != hookBlock()) {
        return;
    }
    const Properties properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const std::string facing = valueOf(properties, "facing");
    const auto direction =
        std::ranges::find_if(kHorizontal, [&facing](BlockFace face) { return nameOf(face) == facing; });
    if (direction == kHorizontal.end()) {
        return;
    }

    bool attached = !beingDestroyed;
    bool powered = false;
    std::int32_t receiver = 0;
    // parity: `BlockState[] wireStates`, the states the scan **read**, which is what the pass writes
    // back at the end: a square the caller handed over as a state of its own is written from that
    // state and not from whatever the world holds by then.
    std::map<std::int32_t, std::int32_t> scanned;
    for (std::int32_t step = 1; step < kMaxLength; ++step) {
        const std::int32_t tx = x + (registry::stepX(*direction) * step);
        const std::int32_t tz = z + (registry::stepZ(*direction) * step);
        const std::optional<std::int32_t> alongId = world.blockState(tx, y, tz);
        if (!alongId.has_value()) {
            break;
        }
        const wire::common::Identifier* along = registry::blockStateDefinitions::blockOf(*alongId);
        if (along != nullptr && *along == hookBlock()) {
            const Properties theirs = registry::blockStateDefinitions::propertiesOf(*alongId);
            if (valueOf(theirs, "facing") == nameOf(registry::opposite(*direction))) {
                receiver = step;
            }
            break;
        }
        // parity: the square the caller named is read from the state it handed over, whatever the
        // world says is there now. That is how a string already gone still counts on this pass.
        const bool isSource = step == wireSource;
        if (!isSource && (along == nullptr || *along != wireBlock())) {
            attached = false;
            continue;
        }
        const std::int32_t readId = isSource ? wireSourceState.value_or(*alongId) : *alongId;
        scanned.emplace(step, readId);
        const Properties wire = registry::blockStateDefinitions::propertiesOf(readId);
        // parity: a wire cut with shears is disarmed, and a disarmed wire never powers.
        const bool armed = valueOf(wire, "disarmed") != "true";
        if (armed && valueOf(wire, "powered") == "true") {
            powered = true;
        }
        if (isSource) {
            world.scheduleBlockTick(x, y, z, hookBlock(), kHookRecheckTicks);
            attached = attached && armed;
        }
    }
    // parity: `attached &= receiverPos > 1`, so two hooks side by side are not a line.
    attached = attached && receiver > 1;
    powered = powered && attached;

    // parity: `TripWireHookBlock.emitState`, an else-if chain, so a hook raises **one** event even
    // when both its properties moved: the click wins over the catch. Both ends of the line raise it,
    // and both read the near hook's own old state, which is what vanilla passes them.
    const bool wasAttached = valueOf(properties, "attached") == "true";
    const bool wasPowered = valueOf(properties, "powered") == "true";
    std::string_view event;
    if (powered && !wasPowered) {
        event = "block_activate";
    } else if (!powered && wasPowered) {
        event = "block_deactivate";
    } else if (attached && !wasAttached) {
        event = "block_attach";
    } else if (!attached && wasAttached) {
        event = "block_detach";
    }

    // parity: the jar writes the hook at the **other** end first and its own last, with the two
    // events in between. The order matters because of the guard below, which sits between them.
    if (receiver > 0) {
        const std::int32_t ox = x + (registry::stepX(*direction) * receiver);
        const std::int32_t oz = z + (registry::stepZ(*direction) * receiver);
        const std::optional<std::int32_t> otherId = world.blockState(ox, y, oz);
        if (!otherId.has_value()) {
            return;
        }
        const wire::common::Identifier* other = registry::blockStateDefinitions::blockOf(*otherId);
        if (other == nullptr) {
            return;
        }
        write(world, ox, y, oz, *otherId, *other, attached, powered, changes);
        // parity: `if (!level.getBlockState(pos).is(Blocks.TRIPWIRE_HOOK)) { onRemoved(...); return; }`.
        // With the near hook already gone this walk stops here, so the string in the middle keeps the
        // `attached` it had instead of being rewritten. Measured in `ganchoquitado3`, where the jar
        // leaves the line taut after one hook goes and we were slackening it.
        const std::optional<std::int32_t> stillHere = world.blockState(x, y, z);
        if (!stillHere.has_value() || !isHook(*stillHere)) {
            return;
        }
        heard(world, event, ox, y, oz, gameTime);
    }
    heard(world, event, x, y, z, gameTime);
    if (!beingDestroyed) {
        write(world, x, y, z, *stateId, *block, attached, powered, changes);
    }
    // parity: the wires themselves follow the pair, which is what draws them taut, and the jar does
    // this **only when `attached` moved**: a line that was already taut is left alone, however out of
    // step a single string in it may be.
    if (wasAttached == attached) {
        return;
    }
    for (std::int32_t step = 1; step < std::max(receiver, 1); ++step) {
        const auto read = scanned.find(step);
        if (read == scanned.end()) {
            continue;
        }
        const std::int32_t tx = x + (registry::stepX(*direction) * step);
        const std::int32_t tz = z + (registry::stepZ(*direction) * step);
        const std::optional<std::int32_t> alongId = world.blockState(tx, y, tz);
        if (!alongId.has_value()) {
            continue;
        }
        // parity: the square has to hold a string **or a hook** for the write to land, and what is
        // written is the state the scan read.
        const wire::common::Identifier* along = registry::blockStateDefinitions::blockOf(*alongId);
        if (along == nullptr || (*along != wireBlock() && *along != hookBlock())) {
            continue;
        }
        write(world, tx, y, tz, read->second, *along, attached, std::nullopt, changes);
        // parity: that `setBlock` redraws the string's neighbours, not the string: measured with
        // `cuerdasola`, where the jar leaves the string beside the hook exactly as the command wrote
        // it. Redrawing it here joined it to the hook, which the jar does not do.
    }
}

void updateSource(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t wireStateId,
                  std::int64_t gameTime, std::vector<BlockChange>& changes) {
    // parity: `TripWireBlock.updateSource` walks **south and west** and nothing else. It does not
    // need the other two: `calculateHook` writes both ends of the line, so finding the hook at the
    // lower end is finding both.
    for (const BlockFace face : {BlockFace::South, BlockFace::West}) {
        for (std::int32_t step = 1; step < kMaxLength; ++step) {
            const std::int32_t tx = x + (registry::stepX(face) * step);
            const std::int32_t tz = z + (registry::stepZ(face) * step);
            const std::optional<std::int32_t> alongId = world.blockState(tx, y, tz);
            if (!alongId.has_value()) {
                break;
            }
            const wire::common::Identifier* along = registry::blockStateDefinitions::blockOf(*alongId);
            if (along == nullptr) {
                break;
            }
            if (*along == hookBlock()) {
                const Properties theirs = registry::blockStateDefinitions::propertiesOf(*alongId);
                if (valueOf(theirs, "facing") == nameOf(registry::opposite(face))) {
                    calculateHook(world, tx, y, tz, gameTime, changes, step, wireStateId);
                }
                break;
            }
            if (*along != wireBlock()) {
                break;
            }
        }
    }
}

} // namespace basalt::server::runtime::world::tripwires
