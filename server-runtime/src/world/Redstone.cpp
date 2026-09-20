#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/runtime/world/ContainerOpeners.hpp>
#include <basalt/server/runtime/world/Jukeboxes.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>
#include <basalt/server/runtime/world/RedstoneWire.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace basalt::server::runtime::world::redstone {
namespace {

using registry::BlockFace;

constexpr std::array<BlockFace, 6> kFaces{BlockFace::Down,  BlockFace::Up,   BlockFace::North,
                                          BlockFace::South, BlockFace::West, BlockFace::East};

constexpr std::array<BlockFace, 4> kHorizontal{BlockFace::North, BlockFace::South, BlockFace::West, BlockFace::East};

/// Sixteen strengths cannot take more rounds than that to settle, plus one to notice they stopped.
constexpr std::int32_t kMaxRounds = 17;

[[nodiscard]] std::string_view nameOf(BlockFace face) {
    switch (face) {
    case BlockFace::Down:
        return "down";
    case BlockFace::Up:
        return "up";
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

using Properties = std::map<std::string, std::string>;

[[nodiscard]] std::string_view valueOf(const Properties& properties, std::string_view key) {
    const auto found = properties.find(std::string{key});
    return found == properties.end() ? std::string_view{} : std::string_view{found->second};
}

[[nodiscard]] std::optional<std::int32_t> numberOf(const Properties& properties, std::string_view key) {
    const std::string_view text = valueOf(properties, key);
    if (text.empty()) {
        return std::nullopt;
    }
    std::int32_t value = 0;
    const std::from_chars_result result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

[[nodiscard]] std::int32_t plateSignal(const Properties& properties) {
    if (const std::optional<std::int32_t> power = numberOf(properties, "power"); power.has_value()) {
        return *power;
    }
    return valueOf(properties, "powered") == "true" ? kMaxSignal : 0;
}

/// parity: `RedStoneWireBlock.getSignal`, whose three cases are all here: nothing downwards,
/// everything upwards (the block the dust sits on), and sideways only where it is connected.
[[nodiscard]] std::int32_t wireSignal(const Properties& drawn, std::int32_t stateId, BlockFace from) {
    // Dust has no `up` property, so asking from below would answer nothing anyway; the case is
    // written out because it is a rule and not an accident of the state names.
    if (from == BlockFace::Down) {
        return 0;
    }
    const std::int32_t power = numberOf(registry::blockStateDefinitions::propertiesOf(stateId), "power").value_or(0);
    if (power == 0) {
        return 0;
    }
    if (from == BlockFace::Up) {
        return power;
    }
    // The side facing whoever is asking, which is the opposite of the way they asked from.
    const std::string_view side = valueOf(drawn, nameOf(registry::opposite(from)));
    return side.empty() || side == "none" ? 0 : power;
}

/// parity: `FaceAttachedHorizontalDirectionalBlock.getConnectedDirection`, the side it hangs on.
[[nodiscard]] std::optional<BlockFace> attachedFace(const Properties& properties) {
    const std::string_view face = valueOf(properties, "face");
    if (face == "ceiling") {
        return BlockFace::Down;
    }
    if (face == "floor") {
        return BlockFace::Up;
    }
    const std::string_view facing = valueOf(properties, "facing");
    for (const BlockFace candidate : kFaces) {
        if (nameOf(candidate) == facing) {
            return candidate;
        }
    }
    return std::nullopt;
}

/// parity: the tag `lightning_rods` is eight blocks and not one, because the rod oxidises like any
/// other copper and every stage is its own block, all of them a `LightningRodBlock`. Matching the
/// plain name left an exposed rod struck by lightning feeding nothing at all.
[[nodiscard]] bool isLightningRod(std::string_view path) { return path.ends_with("lightning_rod"); }

/// parity: the blocks whose `isSignalSource` is true, whatever they are giving off right now. The
/// rods are not here: they are matched by suffix above.
constexpr std::array<std::string_view, 19> kSignalSources{
    "redstone_block", "redstone_torch", "redstone_wall_torch", "repeater", "comparator", "observer", "lever",
    "daylight_detector", "detector_rail", "lectern", "tripwire_hook", "sculk_sensor", "calibrated_sculk_sensor",
    "trapped_chest",
    // parity: `JukeboxBlock.isSignalSource` is
    // always true, playing or not, and that is
    // what makes a dust reach out to it.
    "jukebox", "light_weighted_pressure_plate", "heavy_weighted_pressure_plate", "target",
    // parity: `RedStoneWireBlock.isSignalSource` is true too, which is easy to miss because every
    // redstone path checks for the dust before it asks. The two that do ask are the plain rail
    // deciding whether to turn a corner and, above all, `NaturalSpawner.isValidEmptySpawnBlock`:
    // a dust on the floor is what stops mobs spawning on it.
    "redstone_wire"};

[[nodiscard]] bool conducts(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    return stateId.has_value() && registry::blockSupport::conductsRedstone(*stateId);
}

[[nodiscard]] std::int32_t wirePowerAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || !isWire(*stateId)) {
        return 0;
    }
    return numberOf(registry::blockStateDefinitions::propertiesOf(*stateId), "power").value_or(0);
}

/// parity: `RedStoneWireBlock.getBlockSignal`, which asks the neighbours while pretending the dust
/// itself gives off nothing: without that, two pieces of dust would hold each other up.
[[nodiscard]] std::int32_t blockSignalFor(const WorldState& world, std::string_view worldName, std::int32_t x,
                                          std::int32_t y, std::int32_t z) {
    std::int32_t best = 0;
    for (const BlockFace face : kFaces) {
        const std::int32_t nx = x + registry::stepX(face);
        const std::int32_t ny = y + registry::stepY(face);
        const std::int32_t nz = z + registry::stepZ(face);
        // Dust beside dust is skipped outright, and would answer nothing anyway with the wire
        // muted: what this rules out is two pieces holding each other up.
        const std::optional<std::int32_t> neighbour = world.blockState(nx, ny, nz);
        if (!neighbour.has_value() || isWire(*neighbour)) {
            continue;
        }
        best = std::max(best, signalFrom(world, worldName, nx, ny, nz, face, true));
        if (best >= kMaxSignal) {
            return kMaxSignal;
        }
    }
    return best;
}

/// parity: `RedstoneWireEvaluator.getIncomingWireSignal`, steps up and down included.
[[nodiscard]] std::int32_t incomingWireSignal(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    std::int32_t best = 0;
    const bool aboveIsConductor = conducts(world, x, y + 1, z);
    for (const BlockFace face : kHorizontal) {
        const std::int32_t nx = x + registry::stepX(face);
        const std::int32_t nz = z + registry::stepZ(face);
        best = std::max(best, wirePowerAt(world, nx, y, nz));
        if (conducts(world, nx, y, nz)) {
            if (!aboveIsConductor) {
                best = std::max(best, wirePowerAt(world, nx, y + 1, nz));
            }
        } else {
            best = std::max(best, wirePowerAt(world, nx, y - 1, nz));
        }
    }
    return std::max(0, best - 1);
}

/// Writes the strength a single piece of dust settles on, or nothing when it did not move.
[[nodiscard]] std::optional<BlockChange> updateWire(WorldState& world, std::string_view worldName, std::int32_t x,
                                                    std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || !isWire(*stateId)) {
        return std::nullopt;
    }
    const Properties properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const std::int32_t was = numberOf(properties, "power").value_or(0);
    const std::int32_t wanted = wireStrength(world, worldName, x, y, z);
    if (was == wanted) {
        return std::nullopt;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> turned =
        registry::blockStateDefinitions::stateIdOf(*block, {{"power", std::to_string(wanted)}}, *stateId);
    if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
        return std::nullopt;
    }
    return BlockChange{.x = x, .y = y, .z = z, .stateId = *turned};
}

} // namespace

std::int32_t signalOf(std::int32_t stateId, BlockFace from, std::int32_t openers, std::int32_t comparatorOutput,
                      bool mutedWire, bool playing, bool testPowered) {
    const std::string path = pathOf(stateId);
    if (path.empty()) {
        return 0;
    }
    const Properties properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const bool powered = valueOf(properties, "powered") == "true";

    if (path == "redstone_block") {
        return kMaxSignal;
    }
    // parity: `RedstoneTorchBlock.getSignal`, which feeds every side but the block it stands on.
    if (path == "redstone_torch") {
        return valueOf(properties, "lit") == "true" && from != BlockFace::Up ? kMaxSignal : 0;
    }
    // parity: `RedstoneWallTorchBlock.getSignal`, which does not feed the wall it hangs on.
    if (path == "redstone_wall_torch") {
        return valueOf(properties, "lit") == "true" && valueOf(properties, "facing") != nameOf(from) ? kMaxSignal : 0;
    }
    // parity: `DiodeBlock.getSignal` and `ObserverBlock.getSignal`, both of which feed only the side
    // `FACING` names.
    if (path == "repeater" || path == "comparator" || path == "observer") {
        if (!powered || valueOf(properties, "facing") != nameOf(from)) {
            return 0;
        }
        return path == "comparator" ? comparatorOutput : kMaxSignal;
    }
    if (path == "lever" || path.ends_with("_button")) {
        return powered ? kMaxSignal : 0;
    }
    if (path == "light_weighted_pressure_plate" || path == "heavy_weighted_pressure_plate") {
        return numberOf(properties, "power").value_or(0);
    }
    if (path.ends_with("_pressure_plate")) {
        return plateSignal(properties);
    }
    // parity: `RedStoneWireBlock.getSignal`: dust only feeds the sides it is *drawn towards*, plus
    // the block underneath it. Feeding every side lit whatever happened to be next to a line of dust
    // running past it.
    if (path == "redstone_wire") {
        return mutedWire ? 0 : wireSignal(properties, stateId, from);
    }
    // parity: `DaylightDetectorBlock`, which feeds every side with what it reads off the sky.
    if (path == "daylight_detector") {
        return numberOf(properties, "power").value_or(0);
    }
    // parity: `DetectorRailBlock`, `LecternBlock`, `TripWireHookBlock` and `LightningRodBlock`, all
    // fifteen or nothing.
    if (path == "detector_rail" || path == "lectern" || path == "tripwire_hook" || isLightningRod(path)) {
        return powered ? kMaxSignal : 0;
    }
    // parity: `TargetBlock` and `SculkSensorBlock`, both of which carry their strength in `power`
    // and count it down on their own scheduled tick.
    if (path == "target" || path == "sculk_sensor") {
        return numberOf(properties, "power").value_or(0);
    }
    // parity: `CalibratedSculkSensorBlock.getSignal` gives nothing on the face it looks at, which is
    // the one its frequency filter reads: a sensor that fed its own filter would tune itself.
    if (path == "calibrated_sculk_sensor") {
        return valueOf(properties, "facing") == nameOf(from) ? 0 : numberOf(properties, "power").value_or(0);
    }
    // parity: `TrappedChestBlock.getSignal`, one per person looking inside, capped at fifteen.
    if (path == "trapped_chest") {
        return std::clamp(openers, 0, kMaxSignal);
    }
    // parity: `JukeboxBlock.ownSignal`, fifteen while a disc is *playing*, not while one is in: a
    // song that has run out leaves its disc in the block and the signal goes.
    if (path == "jukebox") {
        return valueOf(properties, "has_record") == "true" && playing ? kMaxSignal : 0;
    }
    // parity: `TestBlock.ownSignal`. Only the `start` mode gives anything off, and only while its
    // block entity says the test is running: the other modes are what **read** a signal, and the
    // `powered` they keep is theirs and never reaches a wire.
    if (path == "test_block") {
        return valueOf(properties, "mode") == "start" && testPowered ? kMaxSignal : 0;
    }
    return 0;
}

std::int32_t directSignalOf(std::int32_t stateId, BlockFace from, std::int32_t openers, std::int32_t comparatorOutput,
                            bool mutedWire) {
    const std::string path = pathOf(stateId);
    if (path.empty()) {
        return 0;
    }
    const Properties properties = registry::blockStateDefinitions::propertiesOf(stateId);

    // parity: `RedstoneTorchBlock.getDirectSignal`, only downwards, which is the block above it.
    if (path == "redstone_torch" || path == "redstone_wall_torch") {
        return from == BlockFace::Down ? signalOf(stateId, from) : 0;
    }
    if (path == "lever" || path.ends_with("_button")) {
        const std::optional<BlockFace> attached = attachedFace(properties);
        return valueOf(properties, "powered") == "true" && attached.has_value() && *attached == from ? kMaxSignal : 0;
    }
    if (path == "light_weighted_pressure_plate" || path == "heavy_weighted_pressure_plate") {
        return from == BlockFace::Up ? numberOf(properties, "power").value_or(0) : 0;
    }
    if (path.ends_with("_pressure_plate")) {
        return from == BlockFace::Up ? plateSignal(properties) : 0;
    }
    // parity: `RedStoneWireBlock.getDirectSignal`, which is its `getSignal` again: dust powers what
    // it points at strongly, and that is what a repeater behind it reads.
    if (path == "redstone_wire") {
        return mutedWire ? 0 : wireSignal(properties, stateId, from);
    }
    // parity: an observer and a diode power the block they feed strongly.
    if (path == "repeater" || path == "comparator" || path == "observer") {
        return signalOf(stateId, from, 0, comparatorOutput);
    }
    // parity: a detector rail, a lectern, a sculk sensor and a trapped chest all feed the block
    // underneath them and nothing else.
    if (path == "detector_rail" || path == "lectern" || path == "sculk_sensor" || path == "calibrated_sculk_sensor" ||
        path == "trapped_chest") {
        return from == BlockFace::Up ? signalOf(stateId, from, openers) : 0;
    }
    // parity: `TripWireHookBlock` and `LightningRodBlock`, which feed the side they face.
    if (path == "tripwire_hook" || isLightningRod(path)) {
        return valueOf(properties, "facing") == nameOf(from) ? signalOf(stateId, from) : 0;
    }
    return 0;
}

std::int32_t comparatorOutputAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    if (chunk == nullptr) {
        return kMaxSignal;
    }
    const server::world::ChunkBlockEntity* entity = chunk->blockEntityAt(x & 15, y, z & 15);
    if (entity == nullptr) {
        return kMaxSignal;
    }
    const wire::nbt::NbtTag* stored = entity->data().find(kComparatorOutputKey);
    if (stored == nullptr) {
        return kMaxSignal;
    }
    const std::int32_t* value = stored->get<std::int32_t>();
    return value == nullptr ? kMaxSignal : *value;
}

namespace {

/// parity: `JukeboxSongPlayer.isPlaying`, which lives in the block entity and not in the state.
/// parity: `TestBlock.ownSignal` reads `TestBlockEntity.isPowered`, which is not a state property:
/// the mode is, and whether the test is running is not.
[[nodiscard]] bool testBlockPoweredAt(const WorldState& world, const std::int32_t x, const std::int32_t y,
                                      const std::int32_t z) {
    const server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    if (chunk == nullptr) {
        return false;
    }
    const server::world::ChunkBlockEntity* entity = chunk->blockEntityAt(x & 15, y, z & 15);
    return entity != nullptr && entity->data().getBoolean("powered");
}

[[nodiscard]] bool jukeboxPlayingAt(const WorldState& world, const std::int32_t x, const std::int32_t y,
                                    const std::int32_t z) {
    const server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    if (chunk == nullptr) {
        return false;
    }
    const server::world::ChunkBlockEntity* entity = chunk->blockEntityAt(x & 15, y, z & 15);
    return entity != nullptr && jukeboxes::isPlaying(entity->data());
}

} // namespace

std::int32_t signalFrom(const WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y,
                        std::int32_t z, BlockFace from, bool mutedWire) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return 0;
    }
    // parity: `RedStoneWireBlock.getSignal` does **not** read the drawn shape it has stored. It runs
    // `getConnectionState(level, state, pos)`, which works the connection out from the world every
    // time it is asked and takes only the power and the was-a-dot answer off the state. That is why
    // vanilla can leave a dust drawn loose that still carries: the drawing goes stale and the signal
    // does not. Reading the property instead was right only for as long as our own drawing was kept
    // fresher than the jar keeps its own.
    const std::int32_t own =
        isWire(*stateId)
            ? (mutedWire ? 0 : wireSignal(redstoneWire::connectionsFor(world, x, y, z, *stateId), *stateId, from))
            : signalOf(*stateId, from, containerOpeners::count(worldName, x, y, z), comparatorOutputAt(world, x, y, z),
                       mutedWire, jukeboxPlayingAt(world, x, y, z), testBlockPoweredAt(world, x, y, z));
    if (!registry::blockSupport::conductsRedstone(*stateId)) {
        return own;
    }
    return std::max(own, directSignalTo(world, worldName, x, y, z, mutedWire));
}

std::int32_t directSignalTo(const WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y,
                            std::int32_t z, bool mutedWire) {
    std::int32_t best = 0;
    for (const BlockFace face : kFaces) {
        const std::int32_t nx = x + registry::stepX(face);
        const std::int32_t ny = y + registry::stepY(face);
        const std::int32_t nz = z + registry::stepZ(face);
        const std::optional<std::int32_t> neighbour = world.blockState(nx, ny, nz);
        if (!neighbour.has_value()) {
            continue;
        }
        best = std::max(best, directSignalOf(*neighbour, face, containerOpeners::count(worldName, nx, ny, nz),
                                             comparatorOutputAt(world, nx, ny, nz), mutedWire));
        if (best >= kMaxSignal) {
            return kMaxSignal;
        }
    }
    return best;
}

std::int32_t containerSignal(std::span<const std::pair<std::int32_t, std::int32_t>> slots, std::int32_t size) {
    if (size <= 0) {
        return 0;
    }
    float total = 0.0F;
    for (const auto& [count, maxStack] : slots) {
        if (count <= 0) {
            continue;
        }
        total += static_cast<float>(count) / static_cast<float>(std::max(maxStack, 1));
    }
    total /= static_cast<float>(size);
    // parity: `Mth.lerpDiscrete(fraction, 0, 15)`, which is `floor(fraction * 14)` plus one for
    // anything at all: a single item already reads one, however big the container is.
    if (total <= 0.0F) {
        return 0;
    }
    return std::clamp(static_cast<std::int32_t>(std::floor(total * (kMaxSignal - 1))) + 1, 0, kMaxSignal);
}

std::int32_t bestNeighbourSignal(const WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y,
                                 std::int32_t z) {
    std::int32_t best = 0;
    for (const BlockFace face : kFaces) {
        best = std::max(best, signalFrom(world, worldName, x + registry::stepX(face), y + registry::stepY(face),
                                         z + registry::stepZ(face), face));
        if (best >= kMaxSignal) {
            return kMaxSignal;
        }
    }
    return best;
}

std::int32_t wireStrength(const WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y,
                          std::int32_t z) {
    // The fifteen wins outright, which the maximum below would say too: it is here because vanilla
    // stops looking at the wires the moment a block signal maxes out.
    const std::int32_t blockSignal = blockSignalFor(world, worldName, x, y, z);
    if (blockSignal >= kMaxSignal) {
        return kMaxSignal;
    }
    return std::max(blockSignal, incomingWireSignal(world, x, y, z));
}

bool isWire(std::int32_t stateId) { return pathOf(stateId) == "redstone_wire"; }

bool isSignalSource(std::int32_t stateId) {
    const std::string path = pathOf(stateId);
    if (path.empty()) {
        return false;
    }
    // The buttons and the plates are one per wood, so they are matched rather than listed.
    return std::ranges::find(kSignalSources, path) != kSignalSources.end() || path.ends_with("_button") ||
           path.ends_with("_pressure_plate") || isLightningRod(path);
}

std::vector<registry::BlockFace> alsoTells(std::int32_t stateId) {
    const std::string path = pathOf(stateId);
    // parity: `SculkSensorBlock.updateNeighbours` tells its own square **and the one below**. A
    // sensor only feeds strongly upwards, so what is under it would never hear it otherwise.
    if (path == "sculk_sensor" || path == "calibrated_sculk_sensor") {
        return {registry::BlockFace::Down};
    }
    // parity: `PoweredRailBlock.updateState` ends by telling the square **below**, and the one above
    // as well when the shape is a slope; `DetectorRailBlock.checkPressed` tells the one below too.
    // The rail feeds nobody, so this changes no signal: what it changes is a neighbour that was left
    // out of step, which is exactly what a lamp written `lit=false` beside a block of redstone is.
    if (path == "powered_rail" || path == "activator_rail" || path == "detector_rail") {
        std::vector<registry::BlockFace> tells{registry::BlockFace::Down};
        if (valueOf(registry::blockStateDefinitions::propertiesOf(stateId), "shape").starts_with("ascending_")) {
            tells.push_back(registry::BlockFace::Up);
        }
        return tells;
    }
    if (path != "tripwire_hook" && !isLightningRod(path)) {
        return {};
    }
    const Properties properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const std::string_view facing = valueOf(properties, "facing");
    for (const BlockFace candidate : kFaces) {
        if (nameOf(candidate) == facing) {
            return {registry::opposite(candidate)};
        }
    }
    return {};
}

std::vector<std::array<std::int32_t, 3>> reachOf(const WorldState& world, std::int32_t x, std::int32_t y,
                                                 std::int32_t z) {
    std::vector<std::array<std::int32_t, 3>> reach{{x, y, z}};
    for (const BlockFace face : kFaces) {
        const std::int32_t nx = x + registry::stepX(face);
        const std::int32_t ny = y + registry::stepY(face);
        const std::int32_t nz = z + registry::stepZ(face);
        reach.push_back({nx, ny, nz});
        const std::optional<std::int32_t> neighbour = world.blockState(nx, ny, nz);
        if (!neighbour.has_value() || !registry::blockSupport::conductsRedstone(*neighbour)) {
            continue;
        }
        for (const BlockFace second : kFaces) {
            reach.push_back({nx + registry::stepX(second), ny + registry::stepY(second), nz + registry::stepZ(second)});
        }
    }
    return reach;
}

std::vector<BlockChange> settle(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y,
                                std::int32_t z, const Notify& notify) {
    // Kept by position and in the order they first moved, which is what a caller sends out.
    std::vector<BlockChange> changes;
    std::map<std::int64_t, std::size_t> changedAt;

    // The block that changed is asked first, and so is every piece of dust that can see it: a lever,
    // a torch or a block of redstone is not dust itself, so starting at it alone left the dust
    // beside a new source dark. parity: `Level.updateNeighborsAt`, which tells the neighbours rather
    // than the block itself.
    std::vector<std::array<std::int32_t, 3>> queue;
    for (const std::array<std::int32_t, 3>& position : reachOf(world, x, y, z)) {
        queue.push_back(position);
        queue.push_back({position[0], position[1] + 1, position[2]});
        queue.push_back({position[0], position[1] - 1, position[2]});
    }

    for (std::int32_t round = 0; round < kMaxRounds && !queue.empty(); ++round) {
        std::vector<std::array<std::int32_t, 3>> next;
        for (const std::array<std::int32_t, 3>& position : queue) {
            const std::optional<BlockChange> moved =
                updateWire(world, worldName, position[0], position[1], position[2]);
            if (!moved.has_value()) {
                continue;
            }
            const std::int64_t key =
                wire::protocol::play::BlockPos{.x = position[0], .y = position[1], .z = position[2]}.pack();
            if (const auto found = changedAt.find(key); found != changedAt.end()) {
                changes[found->second] = *moved;
            } else {
                changedAt.emplace(key, changes.size());
                changes.push_back(*moved);
            }
            // The dust that can see this one has to be asked again, which is the same set the signal
            // itself travels over.
            for (const BlockFace face : kFaces) {
                const std::int32_t nx = position[0] + registry::stepX(face);
                const std::int32_t ny = position[1] + registry::stepY(face);
                const std::int32_t nz = position[2] + registry::stepZ(face);
                next.push_back({nx, ny, nz});
                next.push_back({nx, ny + 1, nz});
                next.push_back({nx, ny - 1, nz});
            }
        }
        queue = std::move(next);
    }

    // parity: `RedStoneWireBlock.updatePowerStrength`, which tells the neighbours of every piece of
    // dust that moved, not only of the block that started it. Without this a repeater more than two
    // blocks from the lever never heard that the dust in front of it had changed, and sat there
    // until something broke it.
    if (notify) {
        for (const BlockChange& change : changes) {
            notify(change.x, change.y, change.z);
        }
    }
    return changes;
}

} // namespace basalt::server::runtime::world::redstone
