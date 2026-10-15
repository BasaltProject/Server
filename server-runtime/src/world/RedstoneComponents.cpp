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
#include <set>
#include <string>

namespace basalt::server::runtime::world::redstoneComponents {
namespace {

using registry::BlockFace;

constexpr std::array<BlockFace, 6> kFaces{BlockFace::Down,  BlockFace::Up,   BlockFace::North,
                                          BlockFace::South, BlockFace::West, BlockFace::East};

/// parity: `NeighborUpdater.UPDATE_ORDER`.
constexpr std::array<BlockFace, 6> kUpdateOrder{BlockFace::West, BlockFace::East,  BlockFace::Down,
                                                BlockFace::Up,   BlockFace::North, BlockFace::South};

struct HashedPos {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;

    [[nodiscard]] bool operator==(const HashedPos& other) const noexcept = default;
};

constexpr std::array<std::string_view, 5> kComponents{"redstone_torch", "redstone_wall_torch", "repeater", "comparator",
                                                      "observer"};

/// parity: `TickPriority`.
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

/// parity: `ComparatorBlock.checkTickOnNeighbor` books a turn only when the reading or the state.
[[nodiscard]] bool isLocked(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                            const Properties& properties);

void scheduleComparator(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
                        std::int32_t stateId, const ContainerSignalAt& containerSignal) {
    // parity: `ComparatorBlock.checkTickOnNeighbor` asks this first.
    if (world.willTickThisTick(x, y, z, comparator())) {
        return;
    }
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
        // parity: `RedstoneTorchBlock.neighborChanged` books a turn only while the torch and what it.
        const Properties properties = registry::blockStateDefinitions::propertiesOf(*stateId);
        if ((valueOf(properties, "lit") == "true") != torchIsFed(world, worldName, x, y, z, properties)) {
            return;
        }
        // parity: the turn is stored under the block that booked it, so a wall torch books a.
        const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
        // parity: `RedstoneTorchBlock.neighborChanged` books nothing while its own turn is waiting.
        if (world.willTickThisTick(x, y, z, block == nullptr ? torch() : *block)) {
            return;
        }
        world.scheduleBlockTick(x, y, z, block == nullptr ? torch() : *block, kTorchDelay);
        return;
    }
    if (path == "repeater") {
        // parity: `DiodeBlock.checkTickOnNeighbor` books nothing while the repeater already says what.
        const Properties properties = registry::blockStateDefinitions::propertiesOf(*stateId);
        // parity: and it books nothing at all while the repeater is **locked**, which is the first.
        if (isLocked(world, x, y, z, properties)) {
            return;
        }
        const bool on = valueOf(properties, "powered") == "true";
        if (on == (repeaterInput(world, worldName, x, y, z, properties) > 0)) {
            return;
        }
        // parity: and nothing either while the repeater's own turn is waiting in this tick's batch.
        if (world.willTickThisTick(x, y, z, repeater())) {
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

/// parity: `RedstoneTorchBlock.hasNeighborSignal`, which asks the block below, and.
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

/// parity: `DiodeBlock.shouldPrioritize`.
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

/// parity: `SignalGetter.getControlInputSignal`.
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
    // parity: dust answers its own strength here rather than the strong signal it gives off, which.
    if (redstone::isWire(*neighbour)) {
        return numberOf(registry::blockStateDefinitions::propertiesOf(*neighbour), "power", 0);
    }
    return redstone::isSignalSource(*neighbour) ? redstone::directSignalOf(*neighbour, side, 0, output) : 0;
}

/// parity: `DiodeBlock.getAlternateSignal`.
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

/// parity: `RepeaterBlock.isLocked` over `DiodeBlock.getAlternateSignal`.
[[nodiscard]] bool isLocked(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                            const Properties& properties) {
    return alternateSignal(world, x, y, z, properties, true) > 0;
}

/// parity: `ComparatorBlock.calculateOutputSignal`.
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
    // parity: `ComparatorBlockEntity.setOutputSignal` writes its field and nothing else. No.
    if (server::world::ChunkBlockEntity* existing = chunk->blockEntityAt(x & 15, y, z & 15); existing != nullptr) {
        existing->data().putInt(redstone::kComparatorOutputKey, output);
        return;
    }
    wire::nbt::NbtCompound data;
    data.putInt(redstone::kComparatorOutputKey, output);
    chunk->setBlockEntity(server::world::ChunkBlockEntity{x & 15, y, z & 15, comparator(), std::move(data)});
}

/// parity: `ObserverBlock.tick`.
[[nodiscard]] std::optional<BlockChange> observerTick(WorldState& world, std::int32_t x, std::int32_t y,
                                                      std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const bool powered = valueOf(registry::blockStateDefinitions::propertiesOf(*stateId), "powered") == "true";
    // parity: `ObserverBlock.tick` writes its new state and only *then* books its own next turn.
    const std::optional<BlockChange> moved = turn(world, x, y, z, *stateId, "powered", powered ? "false" : "true");
    watchFor(world, x, y, z);
    if (!powered) {
        world.scheduleBlockTick(x, y, z, observer(), kObserverDelay);
    }
    return moved;
}

/// parity: `RedstoneTorchBlock.tick`.
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
        // parity: `isToggledTooFrequently(add = true)` puts the toggle in and then counts, so the.
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

/// parity: `DiodeBlock.tick`.
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
        return shouldTurnOn ? std::nullopt : turn(world, x, y, z, *stateId, "powered", "false");
    }
    // parity: `DiodeBlock.tick`'s `else if (!on)`.
    if (!shouldTurnOn) {
        // parity: booked very high, so it beats whatever it feeds.
        world.scheduleBlockTick(x, y, z, repeater(), numberOf(properties, "delay", 1) * 2, kVeryHigh);
    }
    return turn(world, x, y, z, *stateId, "powered", "true");
}

/// parity: `ComparatorBlock.tick` and its `shouldTurnOn`.
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
    // parity: `ComparatorBlockEntity.setOutputSignal`.
    storeOutput(world, x, y, z, output);
    // parity: `ComparatorBlock.refreshOutputState` does the rest only when the **level** moved or the.
    if (output == before && valueOf(properties, "mode") == "subtract") {
        return std::nullopt;
    }
    const bool shouldPower = output > 0;
    if ((valueOf(properties, "powered") == "true") == shouldPower) {
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
    // parity: a diode and an observer both feed the square **behind** the way they face, and the one.
    const BlockFace out = registry::opposite(*facing);
    return {Reach{
        .x = x + registry::stepX(out), .y = y + registry::stepY(out), .z = z + registry::stepZ(out), .skip = *facing}};
}

bool tellsComparatorsOnRemoval(const std::int32_t stateId) {
    const std::string path = pathOf(stateId);
    // parity: exactly the blocks whose `affectNeighborsAfterRemoval` is.
    static const std::set<std::string, std::less<>> kByName{
        "barrel",  "blast_furnace",  "brewing_stand", "chest",     "chiseled_bookshelf",
        "crafter", "creaking_heart", "decorated_pot", "dispenser", "dropper",
        "furnace", "hopper",         "jukebox",       "smoker",    "trapped_chest"};
    return kByName.contains(path) || path.ends_with("shulker_box") || path.ends_with("_shelf") ||
           path.ends_with("copper_golem_statue");
}

std::vector<Reach> removalReach(const std::int32_t stateId, const std::int32_t x, const std::int32_t y,
                                const std::int32_t z) {
    const std::string path = pathOf(stateId);
    // parity: `LecternBlock.affectNeighborsAfterRemoval`.
    if (path == "lectern") {
        return valueOf(registry::blockStateDefinitions::propertiesOf(stateId), "powered") == "true"
                   ? std::vector<Reach>{Reach{.x = x, .y = y - 1, .z = z}}
                   : std::vector<Reach>{};
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    // parity: `LeverBlock`.
    if (path == "lever" || path.ends_with("_button")) {
        if (valueOf(properties, "powered") != "true") {
            return {};
        }
        // parity: `getConnectedDirection`, the face it hangs on, and `updateNeighbours` walks the.
        const std::string_view face = valueOf(properties, "face");
        if (face == "floor") {
            return {Reach{.x = x, .y = y - 1, .z = z}};
        }
        if (face == "ceiling") {
            return {Reach{.x = x, .y = y + 1, .z = z}};
        }
        const std::optional<BlockFace> facing = faceNamed(valueOf(properties, "facing"));
        if (!facing.has_value()) {
            return {};
        }
        const BlockFace behind = registry::opposite(*facing);
        return {Reach{
            .x = x + registry::stepX(behind), .y = y + registry::stepY(behind), .z = z + registry::stepZ(behind)}};
    }
    // parity: `LightningRodBlock.affectNeighborsAfterRemoval` and `TripWireHookBlock.onRemoved`.
    if (path == "lightning_rod" || path == "tripwire_hook") {
        if (valueOf(properties, "powered") != "true") {
            return {};
        }
        const std::optional<BlockFace> facing = faceNamed(valueOf(properties, "facing"));
        if (!facing.has_value()) {
            return {};
        }
        const BlockFace behind = registry::opposite(*facing);
        return {Reach{
            .x = x + registry::stepX(behind), .y = y + registry::stepY(behind), .z = z + registry::stepZ(behind)}};
    }
    // parity: `SculkSensorBlock.affectNeighborsAfterRemoval`.
    if (path == "sculk_sensor" || path == "calibrated_sculk_sensor") {
        return valueOf(properties, "sculk_sensor_phase") == "active"
                   ? std::vector<Reach>{Reach{.x = x, .y = y - 1, .z = z}}
                   : std::vector<Reach>{};
    }
    if (path.ends_with("_pressure_plate")) {
        // parity: `getSignalForState`.
        const std::string_view powered = valueOf(properties, "powered");
        const std::string_view power = valueOf(properties, "power");
        const bool on = powered == "true" || (!power.empty() && power != "0");
        return on ? std::vector<Reach>{Reach{.x = x, .y = y - 1, .z = z}} : std::vector<Reach>{};
    }
    // parity: `isStraight` is the constructor flag.
    const bool straight = path == "powered_rail" || path == "detector_rail" || path == "activator_rail";
    if (!straight && path != "rail") {
        return {};
    }
    std::vector<Reach> out;
    if (valueOf(properties, "shape").starts_with("ascending_")) {
        out.push_back(Reach{.x = x, .y = y + 1, .z = z});
    }
    if (straight) {
        out.push_back(Reach{.x = x, .y = y, .z = z});
        out.push_back(Reach{.x = x, .y = y - 1, .z = z});
    }
    return out;
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
    // parity: `Level.updateNeighbourForOutputSignal` walks `Direction.Plane.HORIZONTAL`.
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
            bx += registry::stepX(face);
            by += registry::stepY(face);
            bz += registry::stepZ(face);
            stateId = world.blockState(bx, by, bz);
            if (!stateId.has_value() || pathOf(*stateId) != "comparator") {
                continue;
            }
        }
        scheduleComparator(world, worldName, bx, by, bz, *stateId, containerSignal);
    }
}

/// parity: `Level.updateNeighborsAt`.
void scheduleNeighbours(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
                        const ContainerSignalAt& containerSignal) {
    for (const BlockFace face : kUpdateOrder) {
        schedule(world, worldName, x + registry::stepX(face), y + registry::stepY(face), z + registry::stepZ(face),
                 containerSignal);
    }
}

std::vector<std::array<std::int32_t, 3>> wireReach(const std::int32_t x, const std::int32_t y, const std::int32_t z) {
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
    const std::string path = pathOf(*stateId);
    const Properties properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    // parity: `RedstoneTorchBlock.notifyNeighbors`.
    if (arrived && (path == "redstone_torch" || path == "redstone_wall_torch")) {
        for (const BlockFace face : kFaces) {
            scheduleNeighbours(world, worldName, x + registry::stepX(face), y + registry::stepY(face),
                               z + registry::stepZ(face), containerSignal);
        }
        return;
    }
    // parity: `ObserverBlock.updateNeighborsInFront` does two things.
    if (path == "observer") {
        if (const std::optional<BlockFace> facing = faceNamed(valueOf(properties, "facing")); facing.has_value()) {
            const BlockFace back = registry::opposite(*facing);
            scheduleNeighbours(world, worldName, x + registry::stepX(back), y + registry::stepY(back),
                               z + registry::stepZ(back), containerSignal);
        }
        return;
    }
    // parity: `DiodeBlock.updateNeighborsInFront` reaches past the square the diode feeds.
}

bool wouldBeLocked(const WorldState& world, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                   const std::map<std::string, std::string>& properties) {
    return isLocked(world, x, y, z, properties);
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
    // parity: this books `ObserverBlock.updateShape`'s `startSignal`.
    if (valueOf(registry::blockStateDefinitions::propertiesOf(*stateId), "powered") == "true") {
        return;
    }
    // parity: `startSignal` is gated on `!hasScheduledTick(pos.
    if (world.hasScheduledBlockTick(x, y, z, observer())) {
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
        // parity: `ObserverBlock.updateShape` only books a turn while the observer is off.
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
    // parity: a comparator reads a container one block further when a solid block is in the way, and.
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

void bookOnPlacement(WorldState& world, std::string_view worldName, const std::int32_t x, const std::int32_t y,
                     const std::int32_t z, const ContainerSignalAt& containerSignal) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return;
    }
    const std::string path = pathOf(*stateId);
    const Properties properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    if (path == "repeater") {
        if (repeaterInput(world, worldName, x, y, z, properties) > 0) {
            world.scheduleBlockTick(x, y, z, repeater(), kPlacementDelay);
        }
        return;
    }
    if (path != "comparator") {
        return;
    }
    // parity: `ComparatorBlock.shouldTurnOn`.
    const std::int32_t input = comparatorInput(world, worldName, x, y, z, properties, containerSignal);
    if (input == 0) {
        return;
    }
    const std::int32_t sides = alternateSignal(world, x, y, z, properties, false);
    if (input > sides || (input == sides && valueOf(properties, "mode") == "compare")) {
        world.scheduleBlockTick(x, y, z, comparator(), kPlacementDelay);
    }
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
