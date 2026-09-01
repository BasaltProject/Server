#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>

#include <algorithm>
#include <array>
#include <charconv>

namespace basalt::server::runtime::world::redstoneComponents {
namespace {

using registry::BlockFace;

constexpr std::array<BlockFace, 6> kFaces{BlockFace::Down,  BlockFace::Up,   BlockFace::North,
                                          BlockFace::South, BlockFace::West, BlockFace::East};

constexpr std::array<std::string_view, 5> kComponents{"redstone_torch", "redstone_wall_torch", "repeater", "comparator",
                                                      "observer"};

using Properties = std::map<std::string, std::string>;

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

[[nodiscard]] std::optional<BlockFace> faceNamed(std::string_view name) {
    for (const BlockFace face : kFaces) {
        if (nameOf(face) == name) {
            return face;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

[[nodiscard]] std::string_view valueOf(const Properties& properties, std::string_view key) {
    const auto found = properties.find(std::string{key});
    return found == properties.end() ? std::string_view{} : std::string_view{found->second};
}

[[nodiscard]] std::int32_t numberOf(const Properties& properties, std::string_view key, std::int32_t fallback) {
    const std::string_view text = valueOf(properties, key);
    if (text.empty()) {
        return fallback;
    }
    std::int32_t value = 0;
    const std::from_chars_result result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size() ? value : fallback;
}

[[nodiscard]] BlockFace clockwise(BlockFace face) {
    switch (face) {
    case BlockFace::North:
        return BlockFace::East;
    case BlockFace::East:
        return BlockFace::South;
    case BlockFace::South:
        return BlockFace::West;
    case BlockFace::West:
        return BlockFace::North;
    default:
        return face;
    }
}

/// The two sides a diode listens to, which are the ones across its own axis.
[[nodiscard]] std::array<BlockFace, 2> sidesOf(BlockFace facing) {
    return {clockwise(facing), clockwise(clockwise(clockwise(facing)))};
}

[[nodiscard]] std::optional<BlockChange> turn(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                              std::int32_t stateId, const std::string& property,
                                              const std::string& value) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> turned =
        registry::blockStateDefinitions::stateIdOf(*block, {{property, value}}, stateId);
    if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
        return std::nullopt;
    }
    return BlockChange{.x = x, .y = y, .z = z, .stateId = *turned};
}

void schedule(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return;
    }
    const std::string path = pathOf(*stateId);
    if (path == "redstone_torch" || path == "redstone_wall_torch") {
        world.scheduleBlockTick(x, y, z, torch(), kTorchDelay);
        return;
    }
    if (path == "repeater") {
        // A repeater already on its way to turning off keeps that appointment, which is what
        // `hasScheduledTick` guards in vanilla.
        const std::int32_t delay = numberOf(registry::blockStateDefinitions::propertiesOf(*stateId), "delay", 1) * 2;
        world.scheduleBlockTick(x, y, z, repeater(), delay);
        return;
    }
    if (path == "comparator") {
        world.scheduleBlockTick(x, y, z, comparator(), kComparatorDelay);
    }
}

/// parity: `RedstoneTorchBlock.hasNeighborSignal`, which asks the block below, and
/// `RedstoneWallTorchBlock`'s own, which asks the wall behind it.
[[nodiscard]] bool torchIsFed(const WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y,
                              std::int32_t z, const Properties& properties) {
    BlockFace behind = BlockFace::Down;
    if (const std::string_view facing = valueOf(properties, "facing"); !facing.empty()) {
        const std::optional<BlockFace> named = faceNamed(facing);
        behind = named.has_value() ? registry::opposite(*named) : BlockFace::Down;
    }
    return redstone::signalFrom(world, worldName, x + registry::stepX(behind), y + registry::stepY(behind),
                                z + registry::stepZ(behind), behind) > 0;
}

/// parity: `DiodeBlock.getAlternateSignal`, the two sides a diode listens to.
[[nodiscard]] std::int32_t alternateSignal(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                           const Properties& properties) {
    const std::optional<BlockFace> facing = faceNamed(valueOf(properties, "facing"));
    if (!facing.has_value()) {
        return 0;
    }
    std::int32_t best = 0;
    for (const BlockFace side : sidesOf(*facing)) {
        const std::optional<std::int32_t> neighbour =
            world.blockState(x + registry::stepX(side), y + registry::stepY(side), z + registry::stepZ(side));
        if (!neighbour.has_value()) {
            continue;
        }
        const std::string path = pathOf(*neighbour);
        // parity: `getControlInputSignal` with `sideInputDiodesOnly`: only a diode or dust on the
        // side counts, which is why a lever beside a comparator does nothing to it.
        if (path != "repeater" && path != "comparator" && !redstone::isWire(*neighbour)) {
            continue;
        }
        best = std::max(best, redstone::signalOf(*neighbour, side));
    }
    return best;
}

/// parity: `DiodeBlock.getInputSignal`, which reads the block the repeater faces.
[[nodiscard]] std::int32_t repeaterInput(const WorldState& world, std::string_view worldName, std::int32_t x,
                                         std::int32_t y, std::int32_t z, const Properties& properties) {
    const std::optional<BlockFace> facing = faceNamed(valueOf(properties, "facing"));
    if (!facing.has_value()) {
        return 0;
    }
    const std::int32_t nx = x + registry::stepX(*facing);
    const std::int32_t ny = y + registry::stepY(*facing);
    const std::int32_t nz = z + registry::stepZ(*facing);
    const std::optional<std::int32_t> target = world.blockState(nx, ny, nz);
    if (!target.has_value()) {
        return 0;
    }
    const std::int32_t signal = redstone::signalFrom(world, worldName, nx, ny, nz, *facing);
    if (signal >= redstone::kMaxSignal || !redstone::isWire(*target)) {
        return signal;
    }
    return std::max(signal, numberOf(registry::blockStateDefinitions::propertiesOf(*target), "power", 0));
}

/// parity: `RepeaterBlock.isLocked` over `DiodeBlock.getAlternateSignal`: a repeater pointed at from
/// either side by another powered repeater is held where it is.
[[nodiscard]] bool isLocked(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                            const Properties& properties) {
    const std::optional<BlockFace> facing = faceNamed(valueOf(properties, "facing"));
    if (!facing.has_value()) {
        return false;
    }
    for (const BlockFace side : sidesOf(*facing)) {
        const std::optional<std::int32_t> neighbour =
            world.blockState(x + registry::stepX(side), y + registry::stepY(side), z + registry::stepZ(side));
        if (!neighbour.has_value()) {
            continue;
        }
        const std::string path = pathOf(*neighbour);
        if (path != "repeater" && path != "comparator") {
            continue;
        }
        const Properties theirs = registry::blockStateDefinitions::propertiesOf(*neighbour);
        if (valueOf(theirs, "powered") != "true") {
            continue;
        }
        // Only a diode pointing *at* this one locks it.
        if (valueOf(theirs, "facing") == nameOf(registry::opposite(side))) {
            return true;
        }
    }
    return false;
}

void storeOutput(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t output) {
    server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    if (chunk == nullptr) {
        return;
    }
    const server::world::ChunkBlockEntity* existing = chunk->blockEntityAt(x & 15, y, z & 15);
    wire::nbt::NbtCompound data = existing == nullptr ? wire::nbt::NbtCompound{} : existing->data();
    data.putInt(redstone::kComparatorOutputKey, output);
    chunk->setBlockEntity(server::world::ChunkBlockEntity{x & 15, y, z & 15, comparator(), std::move(data)});
    chunk->markDirty();
}

/// parity: `ObserverBlock.tick`: it pulses. On its turn it goes on and books another turn to go off
/// again, which is the two-tick blink everybody builds clocks out of.
[[nodiscard]] std::optional<BlockChange> observerTick(WorldState& world, std::int32_t x, std::int32_t y,
                                                      std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const bool powered = valueOf(registry::blockStateDefinitions::propertiesOf(*stateId), "powered") == "true";
    if (!powered) {
        world.scheduleBlockTick(x, y, z, observer(), kObserverDelay);
    }
    return turn(world, x, y, z, *stateId, "powered", powered ? "false" : "true");
}

/// parity: `RedstoneTorchBlock.tick`: lit while the block it hangs on is not powered, out while it
/// is. The burn-out after eight flicks in sixty ticks is not modelled.
[[nodiscard]] std::optional<BlockChange> torchTick(WorldState& world, std::string_view worldName, std::int32_t x,
                                                   std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const Properties properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const bool lit = valueOf(properties, "lit") == "true";
    if (lit != torchIsFed(world, worldName, x, y, z, properties)) {
        return std::nullopt;
    }
    return turn(world, x, y, z, *stateId, "lit", lit ? "false" : "true");
}

/// parity: `DiodeBlock.tick`: a repeater that should be off turns off, and one that should be on
/// turns on and books another turn to check whether the input has already gone.
[[nodiscard]] std::optional<BlockChange> repeaterTick(WorldState& world, std::string_view worldName, std::int32_t x,
                                                      std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const Properties properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    if (isLocked(world, x, y, z, properties)) {
        return std::nullopt;
    }
    const bool powered = valueOf(properties, "powered") == "true";
    const bool shouldPower = repeaterInput(world, worldName, x, y, z, properties) > 0;
    if (powered == shouldPower) {
        return std::nullopt;
    }
    if (!shouldPower) {
        // Turning off is booked one delay later in vanilla, which this reaches by scheduling.
        world.scheduleBlockTick(x, y, z, repeater(), numberOf(properties, "delay", 1) * 2);
    }
    return turn(world, x, y, z, *stateId, "powered", shouldPower ? "true" : "false");
}

/// parity: `ComparatorBlock.tick` and its `shouldTurnOn`: a comparator is on while its rear input
/// beats both sides, or ties them while it is comparing rather than subtracting.
[[nodiscard]] std::optional<BlockChange> comparatorTick(WorldState& world, std::string_view worldName, std::int32_t x,
                                                        std::int32_t y, std::int32_t z,
                                                        const ContainerSignalAt& containerSignal) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const Properties properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const std::int32_t input = comparatorInput(world, worldName, x, y, z, properties, containerSignal);
    const std::int32_t sides = alternateSignal(world, x, y, z, properties);
    const bool subtracting = valueOf(properties, "mode") == "subtract";
    // parity: `ComparatorBlock.calculateOutputSignal`: nothing at all when the sides win, and
    // otherwise the input, less the sides in subtract mode.
    std::int32_t output = 0;
    if (input != 0 && sides <= input) {
        output = subtracting ? input - sides : input;
    }
    // parity: `ComparatorBlockEntity.setOutputSignal`, which is where the level lives: the state
    // only says whether there is one, so without this every comparator fed a flat fifteen.
    storeOutput(world, x, y, z, output);
    const bool shouldPower = output > 0;
    if ((valueOf(properties, "powered") == "true") == shouldPower) {
        return std::nullopt;
    }
    return turn(world, x, y, z, *stateId, "powered", shouldPower ? "true" : "false");
}

} // namespace

const wire::common::Identifier& torch() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("redstone_torch");
    return instance;
}

const wire::common::Identifier& repeater() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("repeater");
    return instance;
}

const wire::common::Identifier& comparator() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("comparator");
    return instance;
}

const wire::common::Identifier& observer() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("observer");
    return instance;
}

bool isComponent(std::int32_t stateId) { return std::ranges::find(kComponents, pathOf(stateId)) != kComponents.end(); }

void scheduleAround(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    // The same reach the dust settles over: a torch sitting on the far side of the block a lever is
    // attached to is two steps away, and it has to hear about the lever.
    for (const std::array<std::int32_t, 3>& position : redstone::reachOf(world, x, y, z)) {
        schedule(world, position[0], position[1], position[2]);
    }
}

void watchFor(WorldState& world, std::int32_t changedX, std::int32_t changedY, std::int32_t changedZ) {
    for (const BlockFace face : kFaces) {
        const std::int32_t x = changedX + registry::stepX(face);
        const std::int32_t y = changedY + registry::stepY(face);
        const std::int32_t z = changedZ + registry::stepZ(face);
        const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
        if (!stateId.has_value() || pathOf(*stateId) != "observer") {
            continue;
        }
        // The observer here watches the block it faces; that has to be the one that moved.
        const std::optional<BlockFace> watching =
            faceNamed(valueOf(registry::blockStateDefinitions::propertiesOf(*stateId), "facing"));
        if (!watching.has_value()) {
            continue;
        }
        if (x + registry::stepX(*watching) != changedX || y + registry::stepY(*watching) != changedY ||
            z + registry::stepZ(*watching) != changedZ) {
            continue;
        }
        world.scheduleBlockTick(x, y, z, observer(), kObserverDelay);
    }
}

std::int32_t comparatorInput(const WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y,
                             std::int32_t z, const Properties& properties, const ContainerSignalAt& containerSignal) {
    const std::optional<BlockFace> facing = faceNamed(valueOf(properties, "facing"));
    if (!facing.has_value()) {
        return 0;
    }
    const std::int32_t bx = x + registry::stepX(*facing);
    const std::int32_t by = y + registry::stepY(*facing);
    const std::int32_t bz = z + registry::stepZ(*facing);
    const std::string_view from = nameOf(registry::opposite(*facing));
    if (containerSignal) {
        if (const std::optional<std::int32_t> held = containerSignal(bx, by, bz, from); held.has_value()) {
            return *held;
        }
    }
    const std::int32_t plain = redstone::signalFrom(world, worldName, bx, by, bz, *facing);
    const std::optional<std::int32_t> target = world.blockState(bx, by, bz);
    if (!target.has_value()) {
        return plain;
    }
    // parity: a comparator reads a container one block further when a solid block is in the way.
    if (plain < redstone::kMaxSignal && registry::blockSupport::conductsRedstone(*target) && containerSignal) {
        const std::optional<std::int32_t> behind = containerSignal(
            bx + registry::stepX(*facing), by + registry::stepY(*facing), bz + registry::stepZ(*facing), from);
        if (behind.has_value()) {
            return *behind;
        }
    }
    return plain;
}

std::optional<BlockChange> tick(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y,
                                std::int32_t z, const ContainerSignalAt& containerSignal) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const std::string path = pathOf(*stateId);
    if (path == "redstone_torch" || path == "redstone_wall_torch") {
        return torchTick(world, worldName, x, y, z);
    }
    if (path == "repeater") {
        return repeaterTick(world, worldName, x, y, z);
    }
    if (path == "comparator") {
        return comparatorTick(world, worldName, x, y, z, containerSignal);
    }
    if (path == "observer") {
        return observerTick(world, x, y, z);
    }
    return std::nullopt;
}

} // namespace basalt::server::runtime::world::redstoneComponents
