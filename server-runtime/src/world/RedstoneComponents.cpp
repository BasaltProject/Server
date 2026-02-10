#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/runtime/world/ItemFrames.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/world/terrain/decoration/JavaHashSetOrder.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>

namespace basalt::server::runtime::world::redstoneComponents {
namespace {

using registry::BlockFace;

constexpr std::array<BlockFace, 6> kFaces{BlockFace::Down,  BlockFace::Up,   BlockFace::North,
                                          BlockFace::South, BlockFace::West, BlockFace::East};

/// parity: `NeighborUpdater.UPDATE_ORDER`, the order `updateNeighborsAt` tells the six squares in.
/// It is not `Direction.values()`, and with the turns now sorted by when they were booked it decides
/// which of two components on the same turn goes first.
constexpr std::array<BlockFace, 6> kUpdateOrder{BlockFace::West, BlockFace::East,  BlockFace::Down,
                                                BlockFace::Up,   BlockFace::North, BlockFace::South};

/// A block position `JavaHashSetOrder` can hash, which is `Vec3i.hashCode` over x, y and z.
struct HashedPos {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;

    [[nodiscard]] bool operator==(const HashedPos& other) const noexcept = default;
};

constexpr std::array<std::string_view, 5> kComponents{"redstone_torch", "redstone_wall_torch", "repeater", "comparator",
                                                      "observer"};

/// parity: `TickPriority`. Only the diodes use anything but the plain one, and what they use it for
/// is to run before the diode they feed on the same turn.
constexpr std::int32_t kExtremelyHigh = -3;
constexpr std::int32_t kVeryHigh = -2;
constexpr std::int32_t kHigh = -1;
constexpr std::int32_t kNormal = 0;

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

[[nodiscard]] bool torchIsFed(const WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y,
                              std::int32_t z, const Properties& properties);
[[nodiscard]] std::int32_t comparatorOutput(const WorldState& world, std::string_view worldName, std::int32_t x,
                                            std::int32_t y, std::int32_t z, const Properties& properties,
                                            const ContainerSignalAt& containerSignal);
[[nodiscard]] std::int32_t storedOutput(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);
[[nodiscard]] std::int32_t repeaterInput(const WorldState& world, std::string_view worldName, std::int32_t x,
                                         std::int32_t y, std::int32_t z, const Properties& properties);
[[nodiscard]] bool feedsAnotherDiode(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                     const Properties& properties);

/// parity: `ComparatorBlock.checkTickOnNeighbor` books a turn only when the reading or the state
/// would actually move. Booking on every nudge left turns pending from the build of a circuit that
/// fired later and read a world that had changed underneath them, a tick early every time.
[[nodiscard]] bool isLocked(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                            const Properties& properties);

void scheduleComparator(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
                        std::int32_t stateId, const ContainerSignalAt& containerSignal) {
    const Properties properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const std::int32_t output = comparatorOutput(world, worldName, x, y, z, properties, containerSignal);
    if (output == storedOutput(world, x, y, z) && (valueOf(properties, "powered") == "true") == (output > 0)) {
        return;
    }
    world.scheduleBlockTick(x, y, z, comparator(), kComparatorDelay,
                            feedsAnotherDiode(world, x, y, z, properties) ? kHigh : kNormal);
}

void schedule(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
              const ContainerSignalAt& containerSignal) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return;
    }
    const std::string path = pathOf(*stateId);
    if (path == "redstone_torch" || path == "redstone_wall_torch") {
        // parity: `RedstoneTorchBlock.neighborChanged` books a turn only while the torch and what it
        // is stuck to say the same thing, which is the torch's way of saying it is in the wrong
        // state. Booking on every nudge left a torch flicking along with a lever it never heard.
        const Properties properties = registry::blockStateDefinitions::propertiesOf(*stateId);
        if ((valueOf(properties, "lit") == "true") != torchIsFed(world, worldName, x, y, z, properties)) {
            return;
        }
        // parity: the turn is stored under the block that booked it, so a wall torch books a
        // `redstone_wall_torch` and not the standing one: vanilla drops a tick whose block is not
        // the one standing there.
        const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
        world.scheduleBlockTick(x, y, z, block == nullptr ? torch() : *block, kTorchDelay);
        return;
    }
    if (path == "repeater") {
        // parity: `DiodeBlock.checkTickOnNeighbor` books nothing while the repeater already says what
        // its input says. Booking anyway left turns pending that fired on a world that had moved on.
        const Properties properties = registry::blockStateDefinitions::propertiesOf(*stateId);
        // parity: and it books nothing at all while the repeater is **locked**, which is the first
        // thing `checkTickOnNeighbor` asks. Without that, a locked repeater whose input disagrees
        // with it re-books a turn every time anything nudges it, for ever.
        if (isLocked(world, x, y, z, properties)) {
            return;
        }
        const bool on = valueOf(properties, "powered") == "true";
        if (on == (repeaterInput(world, worldName, x, y, z, properties) > 0)) {
            return;
        }
        std::int32_t priority = kHigh;
        if (feedsAnotherDiode(world, x, y, z, properties)) {
            priority = kExtremelyHigh;
        } else if (on) {
            priority = kVeryHigh;
        }
        world.scheduleBlockTick(x, y, z, repeater(), numberOf(properties, "delay", 1) * 2, priority);
        return;
    }
    if (path == "comparator") {
        scheduleComparator(world, worldName, x, y, z, *stateId, containerSignal);
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

/// parity: `DiodeBlock.shouldPrioritize`: the square this diode feeds holds another diode that is
/// not pointing away, so that one reads this one and has to go second.
[[nodiscard]] bool feedsAnotherDiode(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                     const Properties& properties) {
    const std::optional<BlockFace> facing = faceNamed(valueOf(properties, "facing"));
    if (!facing.has_value()) {
        return false;
    }
    const BlockFace out = registry::opposite(*facing);
    const std::optional<std::int32_t> ahead =
        world.blockState(x + registry::stepX(out), y + registry::stepY(out), z + registry::stepZ(out));
    if (!ahead.has_value()) {
        return false;
    }
    const std::string path = pathOf(*ahead);
    if (path != "repeater" && path != "comparator") {
        return false;
    }
    return valueOf(registry::blockStateDefinitions::propertiesOf(*ahead), "facing") != nameOf(out);
}

/// parity: `SignalGetter.getControlInputSignal`. Measured in `ladodiodo`: a repeater with a lit line
/// of dust running along its side is **not** locked, because only another diode counts for it; and a
/// comparator subtracting with a block of redstone at its side comes out at zero, because for a
/// comparator everything counts.
[[nodiscard]] std::int32_t controlInput(const WorldState& world, const std::int32_t x, const std::int32_t y,
                                        const std::int32_t z, const BlockFace side, const bool onlyDiodes) {
    const std::int32_t nx = x + registry::stepX(side);
    const std::int32_t ny = y + registry::stepY(side);
    const std::int32_t nz = z + registry::stepZ(side);
    const std::optional<std::int32_t> neighbour = world.blockState(nx, ny, nz);
    if (!neighbour.has_value()) {
        return 0;
    }
    const std::string path = pathOf(*neighbour);
    const std::int32_t output = redstone::comparatorOutputAt(world, nx, ny, nz);
    if (onlyDiodes) {
        return path == "repeater" || path == "comparator" ? redstone::directSignalOf(*neighbour, side, 0, output) : 0;
    }
    if (path == "redstone_block") {
        return redstone::kMaxSignal;
    }
    // parity: dust answers its own strength here rather than the strong signal it gives off, which
    // is the only place the two are asked apart.
    if (redstone::isWire(*neighbour)) {
        return numberOf(registry::blockStateDefinitions::propertiesOf(*neighbour), "power", 0);
    }
    return redstone::isSignalSource(*neighbour) ? redstone::directSignalOf(*neighbour, side, 0, output) : 0;
}

/// parity: `DiodeBlock.getAlternateSignal`, the two sides a diode listens to. `onlyDiodes` is
/// `sideInputDiodesOnly`, true for a repeater and false for a comparator.
[[nodiscard]] std::int32_t alternateSignal(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                           const Properties& properties, const bool onlyDiodes) {
    const std::optional<BlockFace> facing = faceNamed(valueOf(properties, "facing"));
    if (!facing.has_value()) {
        return 0;
    }
    std::int32_t best = 0;
    for (const BlockFace side : sidesOf(*facing)) {
        best = std::max(best, controlInput(world, x, y, z, side, onlyDiodes));
    }
    return best;
}

/// parity: `DiodeBlock.getInputSignal`, which reads the block the repeater faces.
/// What a diode reads out of the square it faces. parity: `DiodeBlock.getInputSignal`, which takes
/// the plain signal and, short of fifteen, the strength of the dust there even when that dust is not
/// drawn as connected to it.
[[nodiscard]] std::int32_t diodeInput(const WorldState& world, std::string_view worldName, std::int32_t x,
                                      std::int32_t y, std::int32_t z, BlockFace from) {
    const std::optional<std::int32_t> target = world.blockState(x, y, z);
    if (!target.has_value()) {
        return 0;
    }
    const std::int32_t signal = redstone::signalFrom(world, worldName, x, y, z, from);
    if (signal >= redstone::kMaxSignal || !redstone::isWire(*target)) {
        return signal;
    }
    return std::max(signal, numberOf(registry::blockStateDefinitions::propertiesOf(*target), "power", 0));
}

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
    return diodeInput(world, worldName, nx, ny, nz, *facing);
}

/// parity: `RepeaterBlock.isLocked` over `DiodeBlock.getAlternateSignal`: a repeater pointed at from
/// either side by another powered repeater is held where it is.
/// parity: `RepeaterBlock.isLocked` is `getAlternateSignal(...) > 0` with the diodes-only rule, and
/// nothing else. What locks a repeater is a diode at its side **feeding** it, which in vanilla's
/// terms is one whose `facing` equals the direction from here to it: its own output then points back
/// this way. The condition written here before was the mirror of that.
[[nodiscard]] bool isLocked(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                            const Properties& properties) {
    return alternateSignal(world, x, y, z, properties, true) > 0;
}

/// parity: `ComparatorBlock.calculateOutputSignal`: nothing at all when the sides win, and otherwise
/// the input, less the sides in subtract mode.
[[nodiscard]] std::int32_t comparatorOutput(const WorldState& world, std::string_view worldName, std::int32_t x,
                                            std::int32_t y, std::int32_t z, const Properties& properties,
                                            const ContainerSignalAt& containerSignal) {
    const std::int32_t input = comparatorInput(world, worldName, x, y, z, properties, containerSignal);
    const std::int32_t sides = alternateSignal(world, x, y, z, properties, false);
    if (input == 0 || sides > input) {
        return 0;
    }
    return valueOf(properties, "mode") == "subtract" ? input - sides : input;
}

/// The level a comparator last wrote down. parity: `ComparatorBlockEntity.getOutputSignal`.
[[nodiscard]] std::int32_t storedOutput(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    if (chunk == nullptr) {
        return 0;
    }
    const server::world::ChunkBlockEntity* entity = chunk->blockEntityAt(x & 15, y, z & 15);
    return entity == nullptr ? 0 : entity->data().getInt(redstone::kComparatorOutputKey, 0);
}

void storeOutput(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t output) {
    server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    if (chunk == nullptr) {
        return;
    }
    // parity: `ComparatorBlockEntity.setOutputSignal` writes its field and nothing else. No
    // `setChanged`, so the chunk is not dirtied and the level only reaches the disk when something
    // else there is written: a comparator that changed level without changing state and then had its
    // chunk unloaded comes back with the old one, in the jar and here.
    if (server::world::ChunkBlockEntity* existing = chunk->blockEntityAt(x & 15, y, z & 15); existing != nullptr) {
        existing->data().putInt(redstone::kComparatorOutputKey, output);
        return;
    }
    wire::nbt::NbtCompound data;
    data.putInt(redstone::kComparatorOutputKey, output);
    chunk->setBlockEntity(server::world::ChunkBlockEntity{x & 15, y, z & 15, comparator(), std::move(data)});
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
    // parity: `ObserverBlock.tick` writes its new state and only *then* books its own next turn, so
    // a neighbour that the write wakes goes into the queue ahead of it. Measured with the jar
    // patched to say when a turn is booked: that order is the whole difference between a chain of
    // observers that alternates and one that answers a turn late.
    const std::optional<BlockChange> moved = turn(world, x, y, z, *stateId, "powered", powered ? "false" : "true");
    watchFor(world, x, y, z);
    if (!powered) {
        world.scheduleBlockTick(x, y, z, observer(), kObserverDelay);
    }
    return moved;
}

/// parity: `RedstoneTorchBlock.tick`: lit while the block it hangs on is not powered, out while it
/// is, and burnt out after eight flicks inside sixty ticks.
[[nodiscard]] std::optional<BlockChange> torchTick(WorldState& world, std::string_view worldName, std::int32_t x,
                                                   std::int32_t y, std::int32_t z, std::int64_t gameTime) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const Properties properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const bool lit = valueOf(properties, "lit") == "true";
    const bool fed = torchIsFed(world, worldName, x, y, z, properties);
    // parity: the list is trimmed at the front on every torch's turn, before anything is decided.
    world.pruneTorchToggles(gameTime, kTorchToggleWindow);
    if (lit) {
        if (!fed) {
            return std::nullopt;
        }
        std::optional<BlockChange> moved = turn(world, x, y, z, *stateId, "lit", "false");
        // parity: `isToggledTooFrequently(add = true)` puts the toggle in and then counts, so the
        // eighth one is the one that burns the torch out and books its long wait.
        world.addTorchToggle(TorchToggle{.x = x, .y = y, .z = z, .when = gameTime});
        if (world.torchToggleCount(x, y, z) >= kMaxTorchToggles) {
            const wire::common::Identifier* burnt = registry::blockStateDefinitions::blockOf(*stateId);
            world.scheduleBlockTick(x, y, z, burnt == nullptr ? torch() : *burnt, kTorchRestartDelay);
            // parity: `level.levelEvent(1502, pos, 0)`, the puff of smoke that says it burnt out.
            if (moved.has_value()) {
                moved->effects.push_back(
                    blockEffect::Effect{.type = blockEffect::Effect::kRedstoneTorchBurnout, .data = 0});
            }
        }
        return moved;
    }
    if (fed) {
        return std::nullopt;
    }
    // parity: a burnt out torch will not come back while its toggles are still inside the window.
    if (world.torchToggleCount(x, y, z) >= kMaxTorchToggles) {
        return std::nullopt;
    }
    return turn(world, x, y, z, *stateId, "lit", "true");
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
    const bool on = valueOf(properties, "powered") == "true";
    const bool shouldTurnOn = repeaterInput(world, worldName, x, y, z, properties) > 0;
    if (on) {
        // On and still fed: nothing to do. On and no longer fed: off it goes.
        return shouldTurnOn ? std::nullopt : turn(world, x, y, z, *stateId, "powered", "false");
    }
    // parity: `DiodeBlock.tick`'s `else if (!on)`, which turns a repeater **on whatever the input
    // says**, and books its own turning off when the input has already gone. That is what stretches a
    // pulse shorter than the delay to the delay's own length, and without it a one-tick pulse into a
    // repeater vanished: measured in `pulsocorto`, where the jar holds the lamp lit for eight ticks
    // and we never lit it at all.
    if (!shouldTurnOn) {
        // parity: booked very high, so it beats whatever it feeds.
        world.scheduleBlockTick(x, y, z, repeater(), numberOf(properties, "delay", 1) * 2, kVeryHigh);
    }
    return turn(world, x, y, z, *stateId, "powered", "true");
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
    const std::int32_t output = comparatorOutput(world, worldName, x, y, z, properties, containerSignal);
    const std::int32_t before = storedOutput(world, x, y, z);
    // parity: `ComparatorBlockEntity.setOutputSignal`, which is where the level lives: the state
    // only says whether there is one, so without this every comparator fed a flat fifteen.
    storeOutput(world, x, y, z, output);
    // parity: `ComparatorBlock.refreshOutputState` does the rest only when the **level** moved or the
    // comparator is comparing, so one that compares tells the square in front on every turn it takes,
    // changed or not. Measured in `comparadornivel`: one going from one to three never changes
    // `powered` and the dust in front follows it anyway.
    if (output == before && valueOf(properties, "mode") == "subtract") {
        return std::nullopt;
    }
    const bool shouldPower = output > 0;
    if ((valueOf(properties, "powered") == "true") == shouldPower) {
        // The state did not move, so the caller sends nothing out: what it needs is the settle and
        // the neighbour updates.
        return BlockChange{.x = x, .y = y, .z = z, .stateId = *stateId};
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

std::vector<Reach> outputReach(const std::int32_t stateId, const std::int32_t x, const std::int32_t y,
                               const std::int32_t z) {
    const std::string path = pathOf(stateId);
    if (path == "redstone_torch" || path == "redstone_wall_torch") {
        std::vector<Reach> out;
        out.reserve(kFaces.size());
        for (const BlockFace face : kFaces) {
            out.push_back(
                Reach{.x = x + registry::stepX(face), .y = y + registry::stepY(face), .z = z + registry::stepZ(face)});
        }
        return out;
    }
    if (path != "repeater" && path != "comparator" && path != "observer") {
        return {};
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const std::optional<BlockFace> facing = faceNamed(valueOf(properties, "facing"));
    if (!facing.has_value()) {
        return {};
    }
    // parity: a diode and an observer both feed the square **behind** the way they face, and the one
    // square that square must not tell back is the diode itself.
    const BlockFace out = registry::opposite(*facing);
    return {Reach{
        .x = x + registry::stepX(out), .y = y + registry::stepY(out), .z = z + registry::stepZ(out), .skip = *facing}};
}

void scheduleInFront(WorldState& world, const std::string_view worldName, const std::int32_t x, const std::int32_t y,
                     const std::int32_t z, const BlockFace back, const ContainerSignalAt& containerSignal) {
    schedule(world, worldName, x, y, z, containerSignal);
    for (const BlockFace face : kUpdateOrder) {
        if (face == back) {
            continue;
        }
        schedule(world, worldName, x + registry::stepX(face), y + registry::stepY(face), z + registry::stepZ(face),
                 containerSignal);
    }
}

void scheduleComparatorsAround(WorldState& world, const std::string_view worldName, const std::int32_t x,
                               const std::int32_t y, const std::int32_t z, const ContainerSignalAt& containerSignal) {
    // parity: `Level.updateNeighbourForOutputSignal` walks `Direction.Plane.HORIZONTAL`, which is
    // **north, east, south, west**, and not the horizontal half of `Direction.values()`, which is
    // north, south, west, east. It decides which of two comparators around the same container books
    // its turn first, and two turns booked on the same tick at the same priority run in that order.
    constexpr std::array<BlockFace, 4> kPlaneHorizontal{BlockFace::North, BlockFace::East, BlockFace::South,
                                                        BlockFace::West};
    for (const BlockFace face : kPlaneHorizontal) {
        std::int32_t bx = x + registry::stepX(face);
        std::int32_t by = y + registry::stepY(face);
        std::int32_t bz = z + registry::stepZ(face);
        std::optional<std::int32_t> stateId = world.blockState(bx, by, bz);
        if (!stateId.has_value()) {
            continue;
        }
        if (pathOf(*stateId) != "comparator") {
            if (!registry::blockSupport::conductsRedstone(*stateId)) {
                continue;
            }
            // The one square further is only reached through a block that conducts, and only in the
            // same direction: a comparator reading a chest behind a wall.
            bx += registry::stepX(face);
            by += registry::stepY(face);
            bz += registry::stepZ(face);
            stateId = world.blockState(bx, by, bz);
            if (!stateId.has_value() || pathOf(*stateId) != "comparator") {
                continue;
            }
        }
        // The same guard the rest of the neighbour updates go through: `neighborChanged` on a
        // comparator is `checkTickOnNeighbor`, which reads before it books.
        scheduleComparator(world, worldName, bx, by, bz, *stateId, containerSignal);
    }
}

/// parity: `Level.updateNeighborsAt`, which is the six neighbours of one square and **not that
/// square**. Telling it too left a repeater put down by command with a state it could not have
/// correcting itself, and the jar leaves it lying: measured in `repetidormentira`, ten ticks with a
/// block of redstone in its input and the repeater still off.
void scheduleNeighbours(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
                        const ContainerSignalAt& containerSignal) {
    for (const BlockFace face : kUpdateOrder) {
        schedule(world, worldName, x + registry::stepX(face), y + registry::stepY(face), z + registry::stepZ(face),
                 containerSignal);
    }
}

std::vector<std::array<std::int32_t, 3>> wireReach(const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    // The order they go into the set is the dust and then `Direction.values()`, which only decides
    // anything when two of them land in the same bucket.
    std::vector<HashedPos> added;
    added.reserve(1 + kFaces.size());
    added.push_back({x, y, z});
    for (const BlockFace face : kFaces) {
        added.push_back({x + registry::stepX(face), y + registry::stepY(face), z + registry::stepZ(face)});
    }
    const std::vector<HashedPos> ordered = server::world::terrain::decoration::javaHashSetOrder(added);
    std::vector<std::array<std::int32_t, 3>> out;
    out.reserve(ordered.size());
    for (const HashedPos& at : ordered) {
        out.push_back({at.x, at.y, at.z});
    }
    return out;
}

void scheduleAroundWire(WorldState& world, const std::string_view worldName, const std::int32_t x, const std::int32_t y,
                        const std::int32_t z, const ContainerSignalAt& containerSignal) {
    for (const std::array<std::int32_t, 3>& at : wireReach(x, y, z)) {
        scheduleNeighbours(world, worldName, at[0], at[1], at[2], containerSignal);
    }
}

void scheduleAround(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
                    const bool arrived, const ContainerSignalAt& containerSignal) {
    scheduleNeighbours(world, worldName, x, y, z, containerSignal);
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return;
    }
    // Two steps is not the general rule: only a handful of blocks tell the neighbours of their own
    // neighbours, and each of them does it its own way. Reaching two steps for everything made a
    // torch answer a lever put down by command two blocks away, which the jar never does.
    const std::string path = pathOf(*stateId);
    const Properties properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    // parity: `RedstoneTorchBlock.notifyNeighbors`, which updates around each of its six sides. It
    // only runs when the torch arrives or leaves: flicking on its own turn writes with `UPDATE_ALL`
    // and stops at its six squares like anything else.
    if (arrived && (path == "redstone_torch" || path == "redstone_wall_torch")) {
        // This outer loop really is `Direction.values()`: `notifyNeighbors` writes it out by hand
        // rather than going through `updateNeighborsAt`, which is the one that uses `UPDATE_ORDER`.
        for (const BlockFace face : kFaces) {
            scheduleNeighbours(world, worldName, x + registry::stepX(face), y + registry::stepY(face),
                               z + registry::stepZ(face), containerSignal);
        }
        return;
    }
    // parity: `ObserverBlock.updateNeighborsInFront` does two things where a diode does one: a
    // `neighborChanged` on the square behind it and an `updateNeighborsAtExceptFromFacing` on that
    // same square. So an observer reaches two steps backwards, and a repeater on the far side of the
    // block it feeds hears it. Measured: the jar fires that repeater, and with one step we did not.
    if (path == "observer") {
        if (const std::optional<BlockFace> facing = faceNamed(valueOf(properties, "facing")); facing.has_value()) {
            const BlockFace back = registry::opposite(*facing);
            scheduleNeighbours(world, worldName, x + registry::stepX(back), y + registry::stepY(back),
                               z + registry::stepZ(back), containerSignal);
        }
        return;
    }
    // parity: `DiodeBlock.updateNeighborsInFront` reaches past the square the diode feeds, but not
    // from here: `outputReach` and `scheduleInFront` are where that lives, because the exclusion it
    // carries (the way back to the diode) has no meaning for a caller that only knows a centre. What
    // this used to add instead was reach on the **input** side, which the jar never touches.
}

std::optional<std::int32_t> relock(WorldState& world, const std::int32_t x, const std::int32_t y,
                                   const std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || pathOf(*stateId) != "repeater") {
        return std::nullopt;
    }
    const Properties properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const bool locked = isLocked(world, x, y, z, properties);
    if ((valueOf(properties, "locked") == "true") == locked) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> turned =
        registry::blockStateDefinitions::stateIdOf(repeater(), {{"locked", locked ? "true" : "false"}}, *stateId);
    if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
        return std::nullopt;
    }
    return turned;
}

std::vector<BlockChange> relockAround(WorldState& world, const std::int32_t x, const std::int32_t y,
                                      const std::int32_t z) {
    std::vector<BlockChange> changes;
    // The axis rule vanilla writes (`directionToNeighbour.getAxis() != facing.getAxis()`) is not
    // needed here: what a lock reads is only the two sides, so an update along the facing axis
    // cannot change the answer and recomputing it costs nothing.
    for (const BlockFace face : kUpdateOrder) {
        const std::int32_t nx = x + registry::stepX(face);
        const std::int32_t ny = y + registry::stepY(face);
        const std::int32_t nz = z + registry::stepZ(face);
        if (const std::optional<std::int32_t> held = relock(world, nx, ny, nz); held.has_value()) {
            changes.push_back(BlockChange{.x = nx, .y = ny, .z = nz, .stateId = *held});
        }
    }
    return changes;
}

void watchOnPlace(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || pathOf(*stateId) != "observer") {
        return;
    }
    if (valueOf(registry::blockStateDefinitions::propertiesOf(*stateId), "powered") == "true") {
        return;
    }
    world.scheduleBlockTick(x, y, z, observer(), kObserverDelay);
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
        // parity: `ObserverBlock.updateShape` only books a turn while the observer is off. One that
        // is already pulsing ignores what it sees, and that is what keeps a chain of them in step.
        if (valueOf(registry::blockStateDefinitions::propertiesOf(*stateId), "powered") == "true") {
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
    const std::int32_t plain = diodeInput(world, worldName, bx, by, bz, *facing);
    const std::optional<std::int32_t> target = world.blockState(bx, by, bz);
    if (!target.has_value()) {
        return plain;
    }
    // parity: a comparator reads a container one block further when a solid block is in the way, and
    // an item frame hanging there counts as well: `max(frame, container)`, with an **empty** frame
    // answering zero rather than answering nothing.
    if (plain < redstone::kMaxSignal && registry::blockSupport::conductsRedstone(*target)) {
        const std::int32_t fx = bx + registry::stepX(*facing);
        const std::int32_t fy = by + registry::stepY(*facing);
        const std::int32_t fz = bz + registry::stepZ(*facing);
        const std::optional<std::int32_t> behind = containerSignal ? containerSignal(fx, fy, fz, from) : std::nullopt;
        const std::optional<std::int32_t> frame = itemFrames::analogOutput(world, fx, fy, fz, *facing);
        if (behind.has_value() && frame.has_value()) {
            return std::max(*behind, *frame);
        }
        if (behind.has_value() || frame.has_value()) {
            return behind.value_or(frame.value_or(0));
        }
    }
    return plain;
}

std::optional<BlockChange> tick(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y,
                                std::int32_t z, std::int64_t gameTime, const ContainerSignalAt& containerSignal) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const std::string path = pathOf(*stateId);
    if (path == "redstone_torch" || path == "redstone_wall_torch") {
        return torchTick(world, worldName, x, y, z, gameTime);
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
