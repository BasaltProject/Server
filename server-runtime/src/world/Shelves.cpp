#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/SelectableSlots.hpp>
#include <basalt/server/runtime/world/Shelves.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <string>

namespace basalt::server::runtime::world::shelves {
namespace {

constexpr std::string_view kUnconnected = "unconnected";
constexpr std::string_view kLeft = "left";
constexpr std::string_view kRight = "right";
constexpr std::string_view kCenter = "center";

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : std::string{block->path()};
}

/// parity: `Direction.getClockWise`/`getCounterClockWise`, which is how a shelf finds its sides.
[[nodiscard]] std::array<std::int32_t, 2> turn(const std::string& facing, std::int32_t way) {
    static const std::array<std::string_view, 4> order{"north", "east", "south", "west"};
    const auto found = std::ranges::find(order, facing);
    if (found == order.end()) {
        return {0, 0};
    }
    const auto index = static_cast<std::int32_t>(found - order.begin());
    const std::string_view turned = order[static_cast<std::size_t>(((index + way) % 4 + 4) % 4)];
    if (turned == "north") {
        return {0, -1};
    }
    if (turned == "south") {
        return {0, 1};
    }
    return turned == "west" ? std::array<std::int32_t, 2>{-1, 0} : std::array<std::int32_t, 2>{1, 0};
}

/// parity: `isConnectable`, which is a wooden shelf that is powered and nothing else.
[[nodiscard]] bool connectable(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || !isShelf(pathOf(*stateId))) {
        return false;
    }
    return propertyOf(*stateId, "powered") == "true";
}

[[nodiscard]] std::string partOf(WorldState& world, const Position& at) {
    const std::optional<std::int32_t> stateId = world.blockState(at[0], at[1], at[2]);
    if (!stateId.has_value()) {
        return std::string{kUnconnected};
    }
    const std::string part = propertyOf(*stateId, "side_chain");
    return part.empty() ? std::string{kUnconnected} : part;
}

[[nodiscard]] std::string facingOf(WorldState& world, const Position& at) {
    const std::optional<std::int32_t> stateId = world.blockState(at[0], at[1], at[2]);
    return stateId.has_value() ? propertyOf(*stateId, "facing") : std::string{};
}

[[nodiscard]] std::optional<Position> neighbour(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                const std::string& facing, bool clockwise) {
    const std::array<std::int32_t, 2> step = turn(facing, clockwise ? 1 : -1);
    const Position at{x + step[0], y, z + step[1]};
    if (!connectable(world, at[0], at[1], at[2]) || facingOf(world, at) != facing) {
        return std::nullopt;
    }
    return at;
}

void walk(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, const std::string& facing, bool clockwise,
          std::string_view towards, const std::function<void(const Position&)>& take) {
    const std::array<std::int32_t, 2> step = turn(facing, clockwise ? 1 : -1);
    for (std::int32_t steps = 1; steps < kMaxChain; ++steps) {
        const Position at{x + (step[0] * steps), y, z + (step[1] * steps)};
        if (!connectable(world, at[0], at[1], at[2]) || facingOf(world, at) != facing) {
            return;
        }
        const std::string part = partOf(world, at);
        if (part == kCenter || part == towards) {
            take(at);
        }
        // parity: `isUnconnectableOrChainEnd`, which stops the walk at anything but a centre.
        if (part != kCenter) {
            return;
        }
    }
}

/// parity: `SideChainPart.whenConnectedToTheLeft`, whose names read the other way round.
[[nodiscard]] std::string connectedToLeft(const std::string& part) {
    return part == kCenter || part == kLeft ? std::string{kCenter} : std::string{kRight};
}

[[nodiscard]] std::string connectedToRight(const std::string& part) {
    return part == kCenter || part == kRight ? std::string{kCenter} : std::string{kLeft};
}

[[nodiscard]] std::string disconnectedFromRight(const std::string& part) {
    return part == kRight || part == kCenter ? std::string{kRight} : std::string{kUnconnected};
}

[[nodiscard]] std::string disconnectedFromLeft(const std::string& part) {
    return part == kLeft || part == kCenter ? std::string{kLeft} : std::string{kUnconnected};
}

void setPart(WorldState& world, const Position& at, const std::string& part, std::vector<BlockChange>& changes) {
    const std::optional<std::int32_t> stateId = world.blockState(at[0], at[1], at[2]);
    if (!stateId.has_value() || propertyOf(*stateId, "side_chain") == part) {
        return;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr) {
        return;
    }
    const std::optional<std::int32_t> turned =
        registry::blockStateDefinitions::stateIdOf(*block, {{"side_chain", part}}, *stateId);
    if (!turned.has_value() || !world.setBlockState(at[0], at[1], at[2], *turned)) {
        return;
    }
    changes.push_back(BlockChange{.x = at[0], .y = at[1], .z = at[2], .stateId = *turned});
}

/// parity: `updateSelfAndNeighborsOnPoweringUp`, which only joins chains that still fit in three.
void connect(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::vector<BlockChange>& changes) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return;
    }
    const std::string facing = propertyOf(*stateId, "facing");
    if (facing.empty()) {
        return;
    }
    const std::optional<Position> left = neighbour(world, x, y, z, facing, true);
    const std::optional<Position> right = neighbour(world, x, y, z, facing, false);
    std::string part{kUnconnected};
    std::int32_t length = 1;
    const auto onLeft =
        left.has_value() ? static_cast<std::int32_t>(chainAt(world, (*left)[0], (*left)[1], (*left)[2]).size()) : 0;
    const auto onRight =
        right.has_value() ? static_cast<std::int32_t>(chainAt(world, (*right)[0], (*right)[1], (*right)[2]).size()) : 0;
    if (onLeft > 0 && length + onLeft <= kMaxChain) {
        part = connectedToLeft(part);
        setPart(world, *left, connectedToRight(partOf(world, *left)), changes);
        length += onLeft;
    }
    if (onRight > 0 && length + onRight <= kMaxChain) {
        part = connectedToRight(part);
        setPart(world, *right, connectedToLeft(partOf(world, *right)), changes);
    }
    setPart(world, Position{x, y, z}, part, changes);
}

/// parity: `updateNeighborsAfterPoweringDown`, which leaves the shelves either side as they were.
void disconnectFacing(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, const std::string& facing,
                      std::vector<BlockChange>& changes) {
    if (facing.empty()) {
        return;
    }
    if (const std::optional<Position> left = neighbour(world, x, y, z, facing, true); left.has_value()) {
        setPart(world, *left, disconnectedFromRight(partOf(world, *left)), changes);
    }
    if (const std::optional<Position> right = neighbour(world, x, y, z, facing, false); right.has_value()) {
        setPart(world, *right, disconnectedFromLeft(partOf(world, *right)), changes);
    }
}

void disconnect(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::vector<BlockChange>& changes) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return;
    }
    disconnectFacing(world, x, y, z, propertyOf(*stateId, "facing"), changes);
}

[[nodiscard]] std::string opposite(std::string_view facing) {
    if (facing == "north") {
        return "south";
    }
    if (facing == "south") {
        return "north";
    }
    return facing == "west" ? "east" : "west";
}

[[nodiscard]] server::world::ChunkBlockEntity* entityAt(WorldState& world, std::int32_t x, std::int32_t y,
                                                        std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return nullptr;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr || !isShelf(block->path())) {
        return nullptr;
    }
    server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    if (chunk == nullptr) {
        return nullptr;
    }
    if (server::world::ChunkBlockEntity* existing = chunk->blockEntityAt(x & 15, y, z & 15); existing != nullptr) {
        return existing;
    }
    chunk->setBlockEntity(server::world::ChunkBlockEntity{x & 15, y, z & 15, *block, wire::nbt::NbtCompound{}});
    return chunk->blockEntityAt(x & 15, y, z & 15);
}

} // namespace

/// parity: `ShelfBlock`, which is every `<wood>_shelf` and nothing else. The suffix carries the
/// underscore on purpose: the plain `bookshelf` is not one of these, and neither is the chiseled one.
bool isShelf(std::string_view path) { return path.ends_with("_shelf"); }

std::optional<std::int32_t> slotAt(std::int32_t stateId, std::string_view face, float clickX, float clickY,
                                   float clickZ) {
    const std::string facing = propertyOf(stateId, "facing");
    if (facing.empty()) {
        return std::nullopt;
    }
    return selectableSlots::hitSlot(facing, face, clickX, clickY, clickZ, kRows, kColumns);
}

std::optional<SwapResult> swap(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t slot,
                               const std::optional<server::world::loot::ItemStack>& held) {
    if (slot < 0 || slot >= kSize) {
        return std::nullopt;
    }
    server::world::ChunkBlockEntity* entity = entityAt(world, x, y, z);
    if (entity == nullptr) {
        return std::nullopt;
    }
    blockInventory::Slots slots = blockInventory::read(entity->data(), kSize);
    const std::optional<server::world::loot::ItemStack> removed = slots[static_cast<std::size_t>(slot)];
    if (!removed.has_value() && !held.has_value()) {
        return std::nullopt;
    }
    slots[static_cast<std::size_t>(slot)] = held;
    blockInventory::write(entity->data(), slots);
    world.chunkIfLoaded(x >> 4, z >> 4)->markDirty();
    redstoneComponents::scheduleComparatorsAround(world, {}, x, y, z, comparatorReading::of(world));
    std::string sound;
    if (removed.has_value() && !held.has_value()) {
        sound = "block.shelf.take_item";
    } else if (removed.has_value()) {
        sound = "block.shelf.single_swap";
    } else {
        sound = "block.shelf.place_item";
    }
    return SwapResult{.removed = removed,
                      .effects = {blockEffect::Sound{.name = std::move(sound),
                                                     .source = wire::protocol::play::SoundSource::Blocks,
                                                     .volume = 1.0F,
                                                     .pitch = 1.0F}}};
}

bool swapHotbar(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, blockInventory::Slots& hotbar) {
    const std::vector<Position> chain = chainAt(world, x, y, z);
    if (chain.empty()) {
        return false;
    }
    bool swapped = false;
    for (std::size_t index = 0; index < chain.size(); ++index) {
        const Position& position = chain[index];
        server::world::ChunkBlockEntity* entity = entityAt(world, position[0], position[1], position[2]);
        if (entity == nullptr) {
            continue;
        }
        blockInventory::Slots slots = blockInventory::read(entity->data(), kSize);
        for (std::int32_t slot = 0; slot < kSize; ++slot) {
            const auto hotbarSlot = static_cast<std::int32_t>(hotbar.size()) -
                                    ((static_cast<std::int32_t>(chain.size() - index)) * kSize) + slot;
            if (hotbarSlot < 0 || hotbarSlot >= static_cast<std::int32_t>(hotbar.size())) {
                continue;
            }
            const std::optional<server::world::loot::ItemStack> fromPlayer =
                hotbar[static_cast<std::size_t>(hotbarSlot)];
            const std::optional<server::world::loot::ItemStack> fromShelf = slots[static_cast<std::size_t>(slot)];
            if (!fromPlayer.has_value() && !fromShelf.has_value()) {
                continue;
            }
            slots[static_cast<std::size_t>(slot)] = fromPlayer;
            hotbar[static_cast<std::size_t>(hotbarSlot)] = fromShelf;
            swapped = true;
        }
        blockInventory::write(entity->data(), slots);
        world.chunkIfLoaded(position[0] >> 4, position[2] >> 4)->markDirty();
        redstoneComponents::scheduleComparatorsAround(world, {}, position[0], position[1], position[2],
                                                      comparatorReading::of(world));
    }
    return swapped;
}

void leftBehind(WorldState& world, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                const std::int32_t wasStateId, std::vector<BlockChange>& changes) {
    if (!isShelf(pathOf(wasStateId))) {
        return;
    }
    const std::optional<std::int32_t> now = world.blockState(x, y, z);
    if (now.has_value() && isShelf(pathOf(*now))) {
        return;
    }
    disconnectFacing(world, x, y, z, propertyOf(wasStateId, "facing"), changes);
}

void arrived(WorldState& world, const std::int32_t x, const std::int32_t y, const std::int32_t z,
             const bool wasConnected, std::vector<BlockChange>& changes) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || !isShelf(pathOf(*stateId))) {
        return;
    }
    if (propertyOf(*stateId, "powered") != "true") {
        disconnect(world, x, y, z, changes);
        return;
    }
    // parity: `isBeingUpdatedByNeighbor`: a shelf that already carries a part, or that carried one
    // before the write, is being joined by whoever is writing it and does not redo the chain itself.
    if (propertyOf(*stateId, "side_chain") != kUnconnected || wasConnected) {
        return;
    }
    connect(world, x, y, z, changes);
}

void reactToSignal(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
                   const std::int64_t gameTime, std::vector<BlockChange>& changes) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || !isShelf(pathOf(*stateId))) {
        return;
    }
    const bool signal = redstone::bestNeighbourSignal(world, worldName, x, y, z) > 0;
    if (signal == (propertyOf(*stateId, "powered") == "true")) {
        return;
    }
    std::map<std::string, std::string> wanted{{"powered", signal ? "true" : "false"}};
    if (!signal) {
        wanted.emplace("side_chain", kUnconnected);
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr) {
        return;
    }
    const std::optional<std::int32_t> turned = registry::blockStateDefinitions::stateIdOf(*block, wanted, *stateId);
    if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
        return;
    }
    changes.push_back(
        BlockChange{.x = x,
                    .y = y,
                    .z = z,
                    .stateId = *turned,
                    .effects = {blockEffect::Sound{.name = signal ? "block.shelf.activate" : "block.shelf.deactivate",
                                                   .source = wire::protocol::play::SoundSource::Blocks,
                                                   .volume = 1.0F,
                                                   .pitch = 1.0F}}});
    // parity: the `gameEvent` beside the sound, which is what a sculk sensor beside a shelf hears.
    if (world.hasSculkSensorNear(x, y, z, vibrations::kCalibratedRange)) {
        vibrations::emit(world, signal ? "block_activate" : "block_deactivate", static_cast<double>(x) + 0.5,
                         static_cast<double>(y) + 0.5, static_cast<double>(z) + 0.5, gameTime);
    }
    if (signal) {
        connect(world, x, y, z, changes);
    } else {
        disconnect(world, x, y, z, changes);
    }
}

void slotsChanged(WorldState& world, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                  const std::int64_t gameTime) {
    if (!world.hasSculkSensorNear(x, y, z, vibrations::kCalibratedRange)) {
        return;
    }
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || !isShelf(pathOf(*stateId))) {
        return;
    }
    vibrations::emit(world, "block_activate", static_cast<double>(x) + 0.5, static_cast<double>(y) + 0.5,
                     static_cast<double>(z) + 0.5, gameTime);
}

std::vector<Position> chainAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || !connectable(world, x, y, z)) {
        return {};
    }
    const std::string facing = propertyOf(*stateId, "facing");
    if (facing.empty()) {
        return {};
    }
    std::deque<Position> chain{Position{x, y, z}};
    walk(world, x, y, z, facing, true, kLeft, [&chain](const Position& at) { chain.push_front(at); });
    walk(world, x, y, z, facing, false, kRight, [&chain](const Position& at) { chain.push_back(at); });
    return std::vector<Position>{chain.begin(), chain.end()};
}

std::int32_t comparatorSignal(const wire::nbt::NbtCompound* data, std::string_view facing, std::string_view from) {
    if (facing.empty() || from.empty() || from != opposite(facing) || data == nullptr) {
        return 0;
    }
    const blockInventory::Slots slots = blockInventory::read(*data, kSize);
    std::int32_t signal = 0;
    for (std::int32_t slot = 0; slot < kSize; ++slot) {
        if (slots[static_cast<std::size_t>(slot)].has_value()) {
            signal |= 1 << slot;
        }
    }
    return signal;
}

} // namespace basalt::server::runtime::world::shelves
