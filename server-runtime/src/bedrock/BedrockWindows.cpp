#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/bedrock/BedrockWindows.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/DoubleChests.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/world/Chunk.hpp>

#include <utility>

namespace basalt::server::runtime::bedrock {
namespace {

using wire::common::Identifier;

/// The block at these coordinates, or nothing when the column is not loaded.
[[nodiscard]] const Identifier* blockAt(world::LoadedWorld& world, const std::int32_t x, const std::int32_t y,
                                        const std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.state().blockState(x, y, z);
    return stateId.has_value() ? registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
}

/// Which block entity a slot of the window belongs to, and its index inside it. A double chest is
/// one window of 54 over two of 27.
[[nodiscard]] std::optional<std::pair<std::array<std::int32_t, 3>, std::int32_t>> halfFor(const OpenWindow& window,
                                                                                          const std::int32_t slot) {
    if (slot < 0 || slot >= window.size) {
        return std::nullopt;
    }
    if (window.second.has_value() && slot >= world::containers::kChestSize) {
        return std::pair{*window.second, slot - world::containers::kChestSize};
    }
    return std::pair{std::array<std::int32_t, 3>{window.x, window.y, window.z}, slot};
}

/// How many slots each half holds, which is not the window's own size when there are two of them.
[[nodiscard]] std::int32_t halfSize(const OpenWindow& window) {
    return window.second.has_value() ? world::containers::kChestSize : window.size;
}

[[nodiscard]] storage::StoredItemStack storedOf(const server::world::loot::ItemStack& stack) {
    return storage::StoredItemStack{.id = stack.item.toString(), .count = stack.count, .components = stack.components};
}

[[nodiscard]] std::optional<server::world::loot::ItemStack>
lootOf(const std::optional<storage::StoredItemStack>& stack) {
    if (!stack.has_value() || stack->count <= 0) {
        return std::nullopt;
    }
    const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(stack->id);
    if (!id.has_value()) {
        return std::nullopt;
    }
    return server::world::loot::ItemStack{.item = *id, .count = stack->count, .components = stack->components};
}

/// The block entity at a point, without making one: a window only ever reads a block that is there.
[[nodiscard]] server::world::ChunkBlockEntity* entityAt(world::LoadedWorld& world,
                                                        const std::array<std::int32_t, 3>& at) {
    server::world::Chunk* chunk = world.state().chunkIfLoaded(at[0] >> 4, at[2] >> 4);
    return chunk == nullptr ? nullptr : chunk->blockEntityAt(at[0] & 15, at[1], at[2] & 15);
}

} // namespace

std::optional<OpenWindow> windowFor(world::LoadedWorld& world, const std::int32_t x, const std::int32_t y,
                                    const std::int32_t z) {
    const Identifier* block = blockAt(world, x, y, z);
    if (block == nullptr) {
        return std::nullopt;
    }
    const std::string path{block->path()};
    // Only the blocks that hold and nothing else, for now: a furnace and a table have machinery of
    // their own and address their slots by their own containers, not the block's.
    if (!world::containers::isPlain(path)) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> size = world::containers::sizeOf(path);
    if (!size.has_value()) {
        return std::nullopt;
    }
    OpenWindow window;
    window.type = wire::bedrock::BedrockContainerType::Container;
    window.x = x;
    window.y = y;
    window.z = z;
    window.size = *size;
    window.block = path;
    // Which half leads is not the one that was clicked: both chests of a pair show the same rows in
    // the same order, whichever one was opened.
    if (const std::optional<std::pair<world::doubleChests::Half, world::doubleChests::Half>> halves =
            world::doubleChests::halves(world.state(), x, y, z);
        halves.has_value()) {
        window.x = halves->first[0];
        window.y = halves->first[1];
        window.z = halves->first[2];
        window.second = halves->second;
        window.size = world::containers::kChestSize * 2;
    }
    return window;
}

std::vector<std::optional<storage::StoredItemStack>> readWindow(world::LoadedWorld& world, const OpenWindow& window) {
    std::vector<std::optional<storage::StoredItemStack>> slots;
    slots.reserve(static_cast<std::size_t>(window.size));
    for (std::int32_t slot = 0; slot < window.size; ++slot) {
        slots.push_back(readWindowSlot(world, window, slot));
    }
    return slots;
}

std::optional<storage::StoredItemStack> readWindowSlot(world::LoadedWorld& world, const OpenWindow& window,
                                                       const std::int32_t slot) {
    const std::optional<std::pair<std::array<std::int32_t, 3>, std::int32_t>> half = halfFor(window, slot);
    if (!half.has_value()) {
        return std::nullopt;
    }
    const server::world::ChunkBlockEntity* entity = entityAt(world, half->first);
    if (entity == nullptr) {
        return std::nullopt;
    }
    const world::blockInventory::Slots slots =
        world::blockInventory::readFor(window.block, entity->data(), halfSize(window));
    if (half->second < 0 || static_cast<std::size_t>(half->second) >= slots.size()) {
        return std::nullopt;
    }
    const std::optional<server::world::loot::ItemStack>& stack = slots[static_cast<std::size_t>(half->second)];
    return stack.has_value() ? std::optional{storedOf(*stack)} : std::nullopt;
}

bool writeWindowSlot(world::LoadedWorld& world, const OpenWindow& window, const std::int32_t slot,
                     const std::optional<storage::StoredItemStack>& stack) {
    const std::optional<std::pair<std::array<std::int32_t, 3>, std::int32_t>> half = halfFor(window, slot);
    if (!half.has_value()) {
        return false;
    }
    server::world::Chunk* chunk = world.state().chunkIfLoaded(half->first[0] >> 4, half->first[2] >> 4);
    server::world::ChunkBlockEntity* entity =
        chunk == nullptr ? nullptr : chunk->blockEntityAt(half->first[0] & 15, half->first[1], half->first[2] & 15);
    if (entity == nullptr) {
        return false;
    }
    // Every slot of the block goes back, not just the one that moved: writing fewer than it holds
    // is what would empty a chest the moment somebody touched its first item.
    world::blockInventory::Slots slots = world::blockInventory::readFor(window.block, entity->data(), halfSize(window));
    if (half->second < 0 || static_cast<std::size_t>(half->second) >= slots.size()) {
        return false;
    }
    const std::optional<server::world::loot::ItemStack>& was = slots[static_cast<std::size_t>(half->second)];
    const std::optional<server::world::loot::ItemStack> put = lootOf(stack);
    const bool sameItem = was.has_value() && put.has_value() && was->item == put->item;
    slots[static_cast<std::size_t>(half->second)] = put;
    world::blockInventory::writeFor(window.block, entity->data(), slots);
    chunk->markDirty();
    if (world::containers::setItemTellsComparators(window.block, half->second, sameItem)) {
        world::redstoneComponents::scheduleComparatorsAround(world.state(), world.name(), half->first[0],
                                                             half->first[1], half->first[2],
                                                             world::comparatorReading::of(world.state()));
    }
    return true;
}

} // namespace basalt::server::runtime::bedrock
