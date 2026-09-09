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

[[nodiscard]] const wire::common::Identifier* blockAt(const WorldState& world, std::int32_t x, std::int32_t y,
                                                      std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    return stateId.has_value() ? registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
}

/// parity: `Level.gameEvent(source, event, pos)`, from the middle of the square. The index of sensors
/// is asked first because a tripwire on a busy corridor raises one of these on every crossing.
void heard(WorldState& world, std::string_view event, std::int32_t x, std::int32_t y, std::int32_t z,
           std::int64_t gameTime) {
    if (event.empty() || !world.hasSculkSensorNear(x, y, z, vibrations::kRange)) {
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

/// Walks the line from a wire to the hook at either end and recalculates it.
void recalculateFrom(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, BlockFace face,
                     std::int64_t gameTime, std::vector<BlockChange>& changes) {
    for (std::int32_t step = 1; step < kMaxLength; ++step) {
        const std::int32_t hx = x + (registry::stepX(face) * step);
        const std::int32_t hz = z + (registry::stepZ(face) * step);
        const wire::common::Identifier* block = blockAt(world, hx, y, hz);
        if (block == nullptr) {
            return;
        }
        if (*block == wireBlock()) {
            continue;
        }
        if (*block != hookBlock()) {
            return;
        }
        calculateHook(world, hx, y, hz, gameTime, changes);
        return;
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

bool isStandingOn(std::int32_t x, std::int32_t y, std::int32_t z, double entityX, double entityY, double entityZ) {
    return entityX >= x && entityX <= x + 1 && entityZ >= z && entityZ <= z + 1 && entityY >= y - kCrossingSlack &&
           entityY <= y + 1;
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
    if (occupied != (valueOf(properties, "powered") == "true")) {
        const std::optional<std::int32_t> turned =
            registry::blockStateDefinitions::stateIdOf(*block, {{"powered", occupied ? "true" : "false"}}, *stateId);
        if (!turned.has_value()) {
            return;
        }
        if (world.setBlockState(x, y, z, *turned)) {
            changes.push_back(BlockChange{.x = x, .y = y, .z = z, .stateId = *turned});
        }
        for (const BlockFace face : kHorizontal) {
            recalculateFrom(world, x, y, z, face, gameTime, changes);
        }
    }
    if (occupied) {
        world.scheduleBlockTick(x, y, z, wireBlock(), kRecheckTicks);
    }
}

void calculateHook(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int64_t gameTime,
                   std::vector<BlockChange>& changes) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
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

    bool attached = true;
    bool powered = false;
    std::int32_t receiver = 0;
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
        if (along == nullptr || *along != wireBlock()) {
            attached = false;
            continue;
        }
        const Properties wire = registry::blockStateDefinitions::propertiesOf(*alongId);
        // parity: a wire cut with shears is disarmed, and a disarmed wire never powers.
        if (valueOf(wire, "disarmed") != "true" && valueOf(wire, "powered") == "true") {
            powered = true;
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

    write(world, x, y, z, *stateId, *block, attached, powered, changes);
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
        heard(world, event, ox, y, oz, gameTime);
    }
    heard(world, event, x, y, z, gameTime);
    // parity: the wires themselves follow the pair, which is what draws them taut.
    for (std::int32_t step = 1; step < std::max(receiver, 1); ++step) {
        const std::int32_t tx = x + (registry::stepX(*direction) * step);
        const std::int32_t tz = z + (registry::stepZ(*direction) * step);
        const std::optional<std::int32_t> alongId = world.blockState(tx, y, tz);
        if (!alongId.has_value()) {
            continue;
        }
        const wire::common::Identifier* along = registry::blockStateDefinitions::blockOf(*alongId);
        if (along == nullptr || *along != wireBlock()) {
            continue;
        }
        write(world, tx, y, tz, *alongId, *along, attached, std::nullopt, changes);
    }
}

} // namespace basalt::server::runtime::world::tripwires
