#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/BlockInteractions.hpp>
#include <basalt/server/runtime/world/CommandBlocks.hpp>
#include <basalt/server/runtime/world/Dripleaves.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/NoteBlocks.hpp>
#include <basalt/server/runtime/world/PoweredRails.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>
#include <basalt/server/runtime/world/RedstoneReactors.hpp>
#include <basalt/server/runtime/world/Shelves.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <utility>

namespace basalt::server::runtime::world::redstoneReactors {
namespace {

using registry::BlockFace;
using Properties = std::map<std::string, std::string>;

/// parity: `NeighborUpdater.UPDATE_ORDER`.
constexpr std::array<BlockFace, 6> kUpdateOrder{BlockFace::West, BlockFace::East,  BlockFace::Down,
                                                BlockFace::Up,   BlockFace::North, BlockFace::South};

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

/// parity: `DoorBlock.playSound` and its two cousins.
[[nodiscard]] std::vector<BlockEffect> openSound(std::int32_t stateId, bool open, random::RandomSource& random) {
    // parity: `DoorBlock.playSound` and its two cousins.
    const std::optional<std::string> sound = blockInteractions::redstoneSound(stateId, open);
    if (!sound.has_value()) {
        return {};
    }
    if (!sound.has_value()) {
        return {};
    }
    return {blockEffect::Sound{.name = *sound,
                               .source = wire::protocol::play::SoundSource::Blocks,
                               .volume = 1.0F,
                               .pitch = (random.nextFloat() * 0.1F) + 0.9F}};
}

/// parity: `Level.gameEvent(source, event, pos)`.
void heard(WorldState& world, std::string_view event, std::int32_t x, std::int32_t y, std::int32_t z,
           std::int64_t gameTime) {
    if (!world.hasSculkSensorNear(x, y, z, vibrations::kCalibratedRange)) {
        return;
    }
    vibrations::emit(world, event, x + 0.5, y + 0.5, z + 0.5, gameTime);
}

/// parity: `DoorBlock.neighborChanged`.
void door(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
          std::int32_t stateId, const Properties& properties, std::int64_t gameTime, std::vector<BlockChange>& changes,
          random::RandomSource& random, std::optional<std::int32_t> told) {
    // parity: `if (!this.defaultBlockState().is(block) && ...)`.
    if (told.has_value() && pathOf(*told) == pathOf(stateId)) {
        return;
    }
    const std::int32_t otherHalf = valueOf(properties, "half") == "lower" ? 1 : -1;
    const bool signal = redstone::bestNeighbourSignal(world, worldName, x, y, z) > 0 ||
                        redstone::bestNeighbourSignal(world, worldName, x, y + otherHalf, z) > 0;
    if (signal == poweredIn(properties)) {
        return;
    }
    // parity: `DoorBlock.neighborChanged` raises the event **only when the door really swings**.
    if (signal != (valueOf(properties, "open") == "true")) {
        heard(world, signal ? "block_open" : "block_close", x, y, z, gameTime);
    }
    const Properties wanted{{"powered", signal ? "true" : "false"}, {"open", signal ? "true" : "false"}};
    write(world, x, y, z, stateId, wanted, changes, openSound(stateId, signal, random));

    // parity: `DoorBlock.updateShape`.
    const std::int32_t twinY = y + otherHalf;
    const std::optional<std::int32_t> twin = world.blockState(x, twinY, z);
    if (!twin.has_value() || pathOf(*twin) != pathOf(stateId)) {
        return;
    }
    write(world, x, twinY, z, *twin, wanted, changes);
}

/// parity: `TrapDoorBlock` and `FenceGateBlock`.
void openable(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
              std::int32_t stateId, const Properties& properties, std::int64_t gameTime,
              std::vector<BlockChange>& changes, random::RandomSource& random) {
    const bool signal = redstone::bestNeighbourSignal(world, worldName, x, y, z) > 0;
    if (signal == poweredIn(properties)) {
        return;
    }
    // parity: `TrapDoorBlock.neighborChanged` and `FenceGateBlock.neighborChanged`.
    if (signal != (valueOf(properties, "open") == "true")) {
        heard(world, signal ? "block_open" : "block_close", x, y, z, gameTime);
    }
    write(world, x, y, z, stateId, {{"powered", signal ? "true" : "false"}, {"open", signal ? "true" : "false"}},
          changes, openSound(stateId, signal, random));
    // parity: `TrapDoorBlock.neighborChanged` ends by booking the water's turn when the door holds.
    if (pathOf(stateId).ends_with("_trapdoor") && valueOf(properties, "waterlogged") == "true") {
        world.scheduleFluidTick(x, y, z, fluidId(FluidKind::Water, true), fluidFlow::kWaterTickDelay);
    }
}

/// parity: `HopperBlock.checkPoweredState`.
void hopperReact(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
                 std::int32_t stateId, const Properties& properties, std::vector<BlockChange>& changes) {
    const bool enabled = valueOf(properties, "enabled") == "true";
    const bool wanted = redstone::bestNeighbourSignal(world, worldName, x, y, z) == 0;
    if (enabled == wanted) {
        return;
    }
    write(world, x, y, z, stateId, {{"enabled", wanted ? "true" : "false"}}, changes);
}

/// parity: `RedstoneLampBlock.neighborChanged`.
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

/// parity: `AbstractSkullBlock.neighborChanged`.
void skull(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
           std::int32_t stateId, const Properties& properties, std::vector<BlockChange>& changes) {
    const bool signal = redstone::bestNeighbourSignal(world, worldName, x, y, z) > 0;
    if (signal == poweredIn(properties)) {
        return;
    }
    write(world, x, y, z, stateId, {{"powered", signal ? "true" : "false"}}, changes);
}

/// parity: `CopperBulbBlock.checkAndFlip`.
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

/// parity: `NoteBlock.neighborChanged`.
void noteBlock(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
               std::int32_t stateId, const Properties& properties, std::int64_t gameTime,
               std::vector<BlockChange>& changes) {
    const bool signal = redstone::bestNeighbourSignal(world, worldName, x, y, z) > 0;
    if (signal == poweredIn(properties)) {
        return;
    }
    std::vector<BlockEffect> effects =
        signal ? noteBlocks::playNote(world, x, y, z, stateId) : std::vector<BlockEffect>{};
    // parity: `NoteBlock.playNote` raises `NOTE_BLOCK_PLAY` beside the sound.
    if (!effects.empty()) {
        heard(world, "note_block_play", x, y, z, gameTime);
    }
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

/// parity: `BellBlock.neighborChanged` into `attemptToRing`.
void bell(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
          std::int32_t stateId, const Properties& properties, std::int64_t gameTime,
          std::vector<BlockChange>& changes) {
    const bool signal = redstone::bestNeighbourSignal(world, worldName, x, y, z) > 0;
    if (signal == poweredIn(properties)) {
        return;
    }
    std::vector<BlockEffect> effects;
    if (signal) {
        // parity: `BellBlock.attemptToRing` raises `BLOCK_CHANGE`.
        heard(world, "block_change", x, y, z, gameTime);
        effects.push_back(blockEffect::Event{.b0 = 1, .b1 = facingValue(valueOf(properties, "facing"))});
        effects.push_back(blockEffect::Sound{.name = "block.bell.use",
                                             .source = wire::protocol::play::SoundSource::Blocks,
                                             .volume = 2.0F,
                                             .pitch = 1.0F});
    }
    write(world, x, y, z, stateId, {{"powered", signal ? "true" : "false"}}, changes, std::move(effects));
}

/// parity: `TntBlock.neighborChanged`.
void tnt(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
         bool tntExplodes, std::vector<BlockChange>& changes) {
    // parity: `TntBlock.prime` answers false with the `tnt_explodes` rule off.
    if (!tntExplodes) {
        return;
    }
    // parity: `neighborChanged` does not look at `unstable`.
    if (redstone::bestNeighbourSignal(world, worldName, x, y, z) <= 0) {
        return;
    }
    if (!world.setBlockState(x, y, z, registry::blocks::air().stateId)) {
        return;
    }
    changes.push_back(BlockChange{.x = x, .y = y, .z = z, .stateId = registry::blocks::air().stateId});
    // parity: `TntBlock.prime` raises `PRIME_FUSE`, which is what a sculk sensor hears.
    world.primeTnt(x, y, z, "prime_fuse");
}

void react(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
           std::int64_t gameTime, bool tntExplodes, std::vector<BlockChange>& changes, random::RandomSource& random,
           std::optional<std::int32_t> told = std::nullopt) {
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
        door(world, worldName, x, y, z, *stateId, properties, gameTime, changes, random, told);
        return;
    }
    if (path.ends_with("_trapdoor") || path.ends_with("_fence_gate")) {
        openable(world, worldName, x, y, z, *stateId, properties, gameTime, changes, random);
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
    if ((path.ends_with("_skull") || path.ends_with("_head")) && properties.contains("powered")) {
        skull(world, worldName, x, y, z, *stateId, properties, changes);
        return;
    }
    // parity: `BigDripleafBlock.neighborChanged`.
    if (path == "big_dripleaf") {
        if (const std::optional<BlockChange> flat = dripleaves::react(world, worldName, x, y, z, gameTime);
            flat.has_value()) {
            changes.push_back(*flat);
        }
        return;
    }
    if (path == "note_block") {
        noteBlock(world, worldName, x, y, z, *stateId, properties, gameTime, changes);
        return;
    }
    // parity: `CommandBlock.neighborChanged`.
    if (commandBlocks::modeOf(path).has_value()) {
        commandBlocks::onSignal(world, x, y, z, redstone::bestNeighbourSignal(world, worldName, x, y, z) > 0);
        return;
    }
    // parity: `StructureBlock.neighborChanged` and `TestBlock.neighborChanged`.
    if (path == "structure_block" || path == "test_block") {
        // parity: `TestBlock.neighborChanged` does nothing at all for a block in `start` mode.
        if (path == "test_block" && valueOf(properties, "mode") == "start") {
            return;
        }
        server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
        server::world::ChunkBlockEntity* entity = chunk == nullptr ? nullptr : chunk->blockEntityAt(x & 15, y, z & 15);
        if (entity == nullptr) {
            return;
        }
        const bool powered = redstone::bestNeighbourSignal(world, worldName, x, y, z) > 0;
        if (entity->data().getBoolean("powered") == powered) {
            return;
        }
        entity->data().putBoolean("powered", powered);
        chunk->markDirty();
        if (powered && path == "structure_block") {
            world.queueStructureTrigger(x, y, z);
        }
        return;
    }
    if (path == "tnt") {
        tnt(world, worldName, x, y, z, tntExplodes, changes);
        return;
    }
    if (path == "powered_rail" || path == "activator_rail") {
        if (std::optional<BlockChange> moved = poweredRails::update(world, worldName, x, y, z); moved.has_value()) {
            changes.push_back(*std::move(moved));
        }
        return;
    }
    if (path == "bell") {
        bell(world, worldName, x, y, z, *stateId, properties, gameTime, changes);
        return;
    }
    if (shelves::isShelf(path)) {
        // parity: `ShelfBlock.neighborChanged`, which joins or breaks its chain with the signal.
        shelves::reactToSignal(world, worldName, x, y, z, gameTime, changes);
    }
}

} // namespace

const wire::common::Identifier& lamp() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("redstone_lamp");
    return instance;
}

/// parity: the blocks whose `onPlace` reads `hasNeighborSignal`.
[[nodiscard]] bool readsPowerOnPlace(const std::string_view path) {
    // parity: the shelf is not one of them. `ShelfBlock.onPlace` only joins or breaks its side chain.
    return path == "tnt" || path == "hopper" || path.ends_with("copper_bulb") || path == "powered_rail" ||
           path == "activator_rail" || path == "detector_rail";
}

std::vector<BlockChange> placed(WorldState& world, const std::string_view worldName, const std::int32_t x,
                                const std::int32_t y, const std::int32_t z, const std::int64_t gameTime,
                                random::RandomSource& random, const bool tntExplodes,
                                const std::optional<std::int32_t> removed) {
    std::vector<BlockChange> changes;
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || !readsPowerOnPlace(pathOf(*stateId))) {
        return changes;
    }
    // parity: the same block written again is not an arrival, so none of them answers it.
    if (removed.has_value()) {
        const wire::common::Identifier* before = registry::blockStateDefinitions::blockOf(*removed);
        const wire::common::Identifier* after = registry::blockStateDefinitions::blockOf(*stateId);
        if (before != nullptr && after != nullptr && *before == *after) {
            return changes;
        }
    }
    react(world, worldName, x, y, z, gameTime, tntExplodes, changes, random);
    return changes;
}

std::vector<BlockChange> update(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y,
                                std::int32_t z, std::int64_t gameTime, random::RandomSource& random,
                                const bool tntExplodes, const bool itself, const std::optional<std::int32_t> told) {
    std::vector<BlockChange> changes;
    if (itself) {
        react(world, worldName, x, y, z, gameTime, tntExplodes, changes, random, told);
    }
    for (const BlockFace face : kUpdateOrder) {
        react(world, worldName, x + registry::stepX(face), y + registry::stepY(face), z + registry::stepZ(face),
              gameTime, tntExplodes, changes, random, told);
    }
    return changes;
}

std::vector<BlockChange> updateInFront(WorldState& world, const std::string_view worldName, const std::int32_t x,
                                       const std::int32_t y, const std::int32_t z, const BlockFace back,
                                       const std::int64_t gameTime, random::RandomSource& random,
                                       const bool tntExplodes) {
    std::vector<BlockChange> changes;
    react(world, worldName, x, y, z, gameTime, tntExplodes, changes, random);
    for (const BlockFace face : kUpdateOrder) {
        if (face == back) {
            continue;
        }
        react(world, worldName, x + registry::stepX(face), y + registry::stepY(face), z + registry::stepZ(face),
              gameTime, tntExplodes, changes, random);
    }
    return changes;
}

std::vector<BlockChange> updateAroundWire(WorldState& world, const std::string_view worldName, const std::int32_t x,
                                          const std::int32_t y, const std::int32_t z, const std::int64_t gameTime,
                                          random::RandomSource& random, const bool tntExplodes) {
    std::vector<BlockChange> changes = update(world, worldName, x, y, z, gameTime, random, tntExplodes);
    for (const BlockFace face : kUpdateOrder) {
        const std::vector<BlockChange> further =
            update(world, worldName, x + registry::stepX(face), y + registry::stepY(face), z + registry::stepZ(face),
                   gameTime, random, tntExplodes);
        changes.insert(changes.end(), further.begin(), further.end());
    }
    return changes;
}

std::optional<BlockChange> lampTick(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y,
                                    std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || pathOf(*stateId) != "redstone_lamp") {
        return std::nullopt;
    }
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
