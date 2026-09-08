#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/NoteBlocks.hpp>
#include <basalt/server/runtime/world/PoweredRails.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>
#include <basalt/server/runtime/world/RedstoneReactors.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <utility>

namespace basalt::server::runtime::world::redstoneReactors {
namespace {

using registry::BlockFace;
using Properties = std::map<std::string, std::string>;

constexpr std::array<BlockFace, 6> kFaces{BlockFace::Down,  BlockFace::Up,   BlockFace::North,
                                          BlockFace::South, BlockFace::West, BlockFace::East};

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

[[nodiscard]] std::string valueOf(const Properties& properties, const std::string& key) {
    const auto found = properties.find(key);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] bool poweredIn(const Properties& properties) { return valueOf(properties, "powered") == "true"; }

void write(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
           const Properties& wanted, std::vector<BlockChange>& changes, std::vector<BlockEffect> effects = {}) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return;
    }
    const std::optional<std::int32_t> turned = registry::blockStateDefinitions::stateIdOf(*block, wanted, stateId);
    if (!turned.has_value() || *turned == stateId || !world.setBlockState(x, y, z, *turned)) {
        return;
    }
    changes.push_back(BlockChange{.x = x, .y = y, .z = z, .stateId = *turned, .effects = std::move(effects)});
}

/// parity: `DoorBlock.playSound` and its two cousins, which is the same sound a hand makes and the
/// same tenth of a tone of randomness.
[[nodiscard]] std::vector<BlockEffect> openSound(const Hooks& hooks, std::int32_t stateId, bool open,
                                                 random::RandomSource& random) {
    if (!hooks.openSound) {
        return {};
    }
    const std::optional<std::string> sound = hooks.openSound(stateId, open);
    if (!sound.has_value()) {
        return {};
    }
    return {blockEffect::Sound{.name = *sound,
                               .source = wire::protocol::play::SoundSource::Blocks,
                               .volume = 1.0F,
                               .pitch = (random.nextFloat() * 0.1F) + 0.9F}};
}

/// parity: `DoorBlock.neighborChanged`, which asks about its own half *and* the other one: a door
/// opens from a signal reaching either end of it.
void door(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
          std::int32_t stateId, const Properties& properties, std::vector<BlockChange>& changes,
          random::RandomSource& random, const Hooks& hooks) {
    const std::int32_t otherHalf = valueOf(properties, "half") == "lower" ? 1 : -1;
    const bool signal = redstone::bestNeighbourSignal(world, worldName, x, y, z) > 0 ||
                        redstone::bestNeighbourSignal(world, worldName, x, y + otherHalf, z) > 0;
    if (signal == poweredIn(properties)) {
        return;
    }
    const Properties wanted{{"powered", signal ? "true" : "false"}, {"open", signal ? "true" : "false"}};
    write(world, x, y, z, stateId, wanted, changes, openSound(hooks, stateId, signal, random));

    // parity: `DoorBlock.updateShape`, where the other half copies this one rather than deciding for
    // itself. Without it a door opens halfway, which is not a state vanilla can be in.
    const std::int32_t twinY = y + otherHalf;
    const std::optional<std::int32_t> twin = world.blockState(x, twinY, z);
    if (!twin.has_value() || pathOf(*twin) != pathOf(stateId)) {
        return;
    }
    write(world, x, twinY, z, *twin, wanted, changes);
}

/// parity: `TrapDoorBlock` and `FenceGateBlock`, which follow the signal with both properties.
void openable(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
              std::int32_t stateId, const Properties& properties, std::vector<BlockChange>& changes,
              random::RandomSource& random, const Hooks& hooks) {
    const bool signal = redstone::bestNeighbourSignal(world, worldName, x, y, z) > 0;
    if (signal == poweredIn(properties)) {
        return;
    }
    write(world, x, y, z, stateId, {{"powered", signal ? "true" : "false"}, {"open", signal ? "true" : "false"}},
          changes, openSound(hooks, stateId, signal, random));
}

/// parity: `HopperBlock.checkPoweredState`, which is the whole of it: a hopper with any signal
/// touching it is `enabled=false`, at once and with no delay. The tick already reads that property
/// to decide whether to move an item; nothing was ever writing it.
void hopperReact(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
                 std::int32_t stateId, const Properties& properties, std::vector<BlockChange>& changes) {
    const bool enabled = valueOf(properties, "enabled") == "true";
    const bool wanted = redstone::bestNeighbourSignal(world, worldName, x, y, z) == 0;
    if (enabled == wanted) {
        return;
    }
    write(world, x, y, z, stateId, {{"enabled", wanted ? "true" : "false"}}, changes);
}

/// parity: `RedstoneLampBlock.neighborChanged`: it lights at once and books four ticks to go out, so
/// a signal that blinks does not make the lamp blink with it.
void lampReact(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
               std::int32_t stateId, const Properties& properties, std::vector<BlockChange>& changes) {
    const bool lit = valueOf(properties, "lit") == "true";
    const bool signal = redstone::bestNeighbourSignal(world, worldName, x, y, z) > 0;
    if (lit == signal) {
        return;
    }
    if (lit) {
        world.scheduleBlockTick(x, y, z, lamp(), kLampOffDelay);
        return;
    }
    write(world, x, y, z, stateId, {{"lit", "true"}}, changes);
}

/// parity: `CopperBulbBlock.checkAndFlip`: the bulb keeps its light and flips it on the *rising*
/// edge only, which is what makes it a T flip-flop in one block.
void bulb(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
          std::int32_t stateId, const Properties& properties, std::vector<BlockChange>& changes) {
    const bool powered = poweredIn(properties);
    const bool signal = redstone::bestNeighbourSignal(world, worldName, x, y, z) > 0;
    if (signal == powered) {
        return;
    }
    Properties wanted{{"powered", signal ? "true" : "false"}};
    std::vector<BlockEffect> effects;
    if (!powered) {
        const bool flipped = valueOf(properties, "lit") != "true";
        wanted.emplace("lit", flipped ? "true" : "false");
        effects.push_back(
            blockEffect::Sound{.name = flipped ? "block.copper_bulb.turn_on" : "block.copper_bulb.turn_off",
                               .source = wire::protocol::play::SoundSource::Blocks,
                               .volume = 1.0F,
                               .pitch = 1.0F});
    }
    write(world, x, y, z, stateId, wanted, changes, std::move(effects));
}

/// parity: `NoteBlock.neighborChanged`, which plays the note on the way up only: a signal going away
/// is silent.
void noteBlock(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
               std::int32_t stateId, const Properties& properties, std::vector<BlockChange>& changes) {
    const bool signal = redstone::bestNeighbourSignal(world, worldName, x, y, z) > 0;
    if (signal == poweredIn(properties)) {
        return;
    }
    std::vector<BlockEffect> effects =
        signal ? noteBlocks::playNote(world, x, y, z, stateId) : std::vector<BlockEffect>{};
    write(world, x, y, z, stateId, {{"powered", signal ? "true" : "false"}}, changes, std::move(effects));
}

/// parity: `Direction.get3DDataValue`, which is what a bell's swing travels as.
[[nodiscard]] std::int32_t facingValue(const std::string& facing) {
    if (facing == "down") {
        return 0;
    }
    if (facing == "up") {
        return 1;
    }
    if (facing == "south") {
        return 3;
    }
    if (facing == "west") {
        return 4;
    }
    if (facing == "east") {
        return 5;
    }
    return 2;
}

/// parity: `BellBlock.neighborChanged` into `attemptToRing`, which swings it the way it faces.
void bell(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
          std::int32_t stateId, const Properties& properties, std::vector<BlockChange>& changes) {
    const bool signal = redstone::bestNeighbourSignal(world, worldName, x, y, z) > 0;
    if (signal == poweredIn(properties)) {
        return;
    }
    std::vector<BlockEffect> effects;
    if (signal) {
        effects.push_back(blockEffect::Event{.b0 = 1, .b1 = facingValue(valueOf(properties, "facing"))});
        effects.push_back(blockEffect::Sound{.name = "block.bell.use",
                                             .source = wire::protocol::play::SoundSource::Blocks,
                                             .volume = 2.0F,
                                             .pitch = 1.0F});
    }
    write(world, x, y, z, stateId, {{"powered", signal ? "true" : "false"}}, changes, std::move(effects));
}

/// parity: `TntBlock.neighborChanged`, which lights it and takes the block away. What is left is the
/// entity, which the caller spawns when it sees the block go.
void tnt(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
         const Properties& properties, std::vector<BlockChange>& changes) {
    if (valueOf(properties, "unstable") == "true") {
        return;
    }
    if (redstone::bestNeighbourSignal(world, worldName, x, y, z) <= 0) {
        return;
    }
    if (!world.setBlockState(x, y, z, registry::blocks::air().stateId)) {
        return;
    }
    changes.push_back(BlockChange{.x = x, .y = y, .z = z, .stateId = registry::blocks::air().stateId});
    world.primeTnt(x, y, z);
}

void react(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
           std::vector<BlockChange>& changes, random::RandomSource& random, const Hooks& hooks) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return;
    }
    const std::string path = pathOf(*stateId);
    if (path.empty()) {
        return;
    }
    const Properties properties = registry::blockStateDefinitions::propertiesOf(*stateId);

    if (path.ends_with("_door") && properties.contains("half")) {
        door(world, worldName, x, y, z, *stateId, properties, changes, random, hooks);
        return;
    }
    if (path.ends_with("_trapdoor") || path.ends_with("_fence_gate")) {
        openable(world, worldName, x, y, z, *stateId, properties, changes, random, hooks);
        return;
    }
    if (path == "redstone_lamp") {
        lampReact(world, worldName, x, y, z, *stateId, properties, changes);
        return;
    }
    if (path == "hopper") {
        hopperReact(world, worldName, x, y, z, *stateId, properties, changes);
        return;
    }
    if (path.ends_with("copper_bulb")) {
        bulb(world, worldName, x, y, z, *stateId, properties, changes);
        return;
    }
    if (path == "note_block") {
        noteBlock(world, worldName, x, y, z, *stateId, properties, changes);
        return;
    }
    if (path == "tnt") {
        tnt(world, worldName, x, y, z, properties, changes);
        return;
    }
    if (path == "powered_rail" || path == "activator_rail") {
        if (std::optional<BlockChange> moved = poweredRails::update(world, worldName, x, y, z); moved.has_value()) {
            changes.push_back(*std::move(moved));
        }
        return;
    }
    if (path == "bell") {
        bell(world, worldName, x, y, z, *stateId, properties, changes);
        return;
    }
    if (hooks.reactShelf) {
        hooks.reactShelf(world, x, y, z, changes);
    }
}

} // namespace

const wire::common::Identifier& lamp() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("redstone_lamp");
    return instance;
}

std::vector<BlockChange> update(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y,
                                std::int32_t z, random::RandomSource& random, const Hooks& hooks, const bool itself) {
    std::vector<BlockChange> changes;
    if (itself) {
        react(world, worldName, x, y, z, changes, random, hooks);
    }
    for (const BlockFace face : kFaces) {
        react(world, worldName, x + registry::stepX(face), y + registry::stepY(face), z + registry::stepZ(face),
              changes, random, hooks);
    }
    return changes;
}

std::optional<BlockChange> lampTick(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y,
                                    std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || pathOf(*stateId) != "redstone_lamp") {
        return std::nullopt;
    }
    // An unlit lamp has nothing to turn off, which the write below would work out for itself.
    const Properties properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    if (valueOf(properties, "lit") != "true") {
        return std::nullopt;
    }
    if (redstone::bestNeighbourSignal(world, worldName, x, y, z) > 0) {
        return std::nullopt;
    }
    std::vector<BlockChange> changes;
    write(world, x, y, z, *stateId, {{"lit", "false"}}, changes);
    return changes.empty() ? std::nullopt : std::optional{changes.front()};
}

} // namespace basalt::server::runtime::world::redstoneReactors
