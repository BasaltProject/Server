#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/runtime/bedrock/BedrockWindows.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/DoubleChests.hpp>
#include <basalt/server/runtime/world/Furnaces.hpp>
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
    // A cooker is a window of its own kind, with its three slots and its two bars. Everything else
    // this build opens is a box: rows of slots and nothing behind them.
    if (world::furnaces::cookerFor(path).has_value()) {
        return OpenWindow{.type = path == "blast_furnace" ? wire::bedrock::BedrockContainerType::BlastFurnace
                                  : path == "smoker"      ? wire::bedrock::BedrockContainerType::Smoker
                                                          : wire::bedrock::BedrockContainerType::Furnace,
                          .x = x,
                          .y = y,
                          .z = z,
                          .size = world::furnaces::kSize,
                          .block = path};
    }
    // A table's nine cells are not the block's: they belong to whoever is standing at it, so the
    // window has none of the block's own slots.
    // parity: `EnderChestBlock.useWithoutItem` refuses under a block that would trap the lid, and
    // then shows the player's own slots rather than anything in the world.
    if (path == "ender_chest") {
        const std::optional<std::int32_t> above = world.state().blockState(x, y + 1, z);
        if (!above.has_value() || server::registry::blockSupport::conductsRedstone(*above)) {
            return std::nullopt;
        }
        return OpenWindow{.type = wire::bedrock::BedrockContainerType::Container,
                          .x = x,
                          .y = y,
                          .z = z,
                          .size = storage::StoredPlayer::kEnderChestSlots,
                          .block = path,
                          .playerOwned = true};
    }
    // parity: `StonecutterBlock.useWithoutItem`. Its one slot belongs to the screen rather than to
    // the block, the same as a table's grid does.
    if (path == "stonecutter") {
        return OpenWindow{
            .type = wire::bedrock::BedrockContainerType::Stonecutter, .x = x, .y = y, .z = z, .size = 0, .block = path};
    }
    // parity: `EnchantingTableBlock.useWithoutItem`. Its two slots belong to the screen, and what
    // it offers comes off the shelves around it.
    if (path == "enchanting_table") {
        return OpenWindow{
            .type = wire::bedrock::BedrockContainerType::Enchantment, .x = x, .y = y, .z = z, .size = 0, .block = path};
    }
    // parity: `SmithingTableBlock.useWithoutItem`. Its four slots belong to the screen too.
    if (path == "smithing_table") {
        return OpenWindow{.type = wire::bedrock::BedrockContainerType::SmithingTable,
                          .x = x,
                          .y = y,
                          .z = z,
                          .size = 0,
                          .block = path};
    }
    // parity: `GrindstoneBlock.useWithoutItem`. Its three slots belong to the screen too.
    if (path == "grindstone") {
        return OpenWindow{
            .type = wire::bedrock::BedrockContainerType::Grindstone, .x = x, .y = y, .z = z, .size = 0, .block = path};
    }
    // parity: `AnvilBlock.useWithoutItem`. Its three slots belong to the screen, and the block
    // wears out under whoever uses it.
    if (path == "anvil" || path == "chipped_anvil" || path == "damaged_anvil") {
        return OpenWindow{
            .type = wire::bedrock::BedrockContainerType::Anvil, .x = x, .y = y, .z = z, .size = 0, .block = path};
    }
    if (path == "crafting_table") {
        return OpenWindow{
            .type = wire::bedrock::BedrockContainerType::Workbench, .x = x, .y = y, .z = z, .size = 0, .block = path};
    }
    // Three of the boxes below have a screen of their own over there, and a brewing stand and a
    // crafter are not boxes at all: a client sent the wrong kind draws the wrong shape.
    if (path == "brewing_stand" || path == "crafter") {
        const std::optional<std::int32_t> size = world::containers::sizeOf(path);
        if (!size.has_value()) {
            return std::nullopt;
        }
        return OpenWindow{.type = path == "crafter" ? wire::bedrock::BedrockContainerType::Crafter
                                                    : wire::bedrock::BedrockContainerType::BrewingStand,
                          .x = x,
                          .y = y,
                          .z = z,
                          .size = *size,
                          .block = path};
    }
    if (!world::containers::isPlain(path)) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> size = world::containers::sizeOf(path);
    if (!size.has_value()) {
        return std::nullopt;
    }
    OpenWindow window;
    window.type = path == "hopper"      ? wire::bedrock::BedrockContainerType::Hopper
                  : path == "dispenser" ? wire::bedrock::BedrockContainerType::Dispenser
                  : path == "dropper"   ? wire::bedrock::BedrockContainerType::Dropper
                                        : wire::bedrock::BedrockContainerType::Container;
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

std::optional<std::int32_t> blockSlotOf(const OpenWindow& window, const std::int32_t container,
                                        const std::int32_t slot) {
    const auto named = static_cast<wire::bedrock::BedrockContainerSlot>(container);
    if (window.size == 0) {
        return std::nullopt; // Nothing in this window is the block's.
    }
    if (window.type == wire::bedrock::BedrockContainerType::Container) {
        return named == wire::bedrock::BedrockContainerSlot::LevelEntity ? std::optional{slot} : std::nullopt;
    }
    // A cooker gives each of its three slots a container of its own, so the slot number inside it
    // says nothing: which container it is *is* the slot.
    switch (named) {
    case wire::bedrock::BedrockContainerSlot::FurnaceIngredient:
        return world::furnaces::kInputSlot;
    case wire::bedrock::BedrockContainerSlot::FurnaceFuel:
        return world::furnaces::kFuelSlot;
    case wire::bedrock::BedrockContainerSlot::FurnaceResult:
        return world::furnaces::kResultSlot;
    default:
        return std::nullopt;
    }
}

std::optional<FurnaceNumbers> furnaceNumbers(world::LoadedWorld& world, const OpenWindow& window) {
    if (window.type == wire::bedrock::BedrockContainerType::Container) {
        return std::nullopt;
    }
    const server::world::ChunkBlockEntity* entity = entityAt(world, {window.x, window.y, window.z});
    if (entity == nullptr) {
        return std::nullopt;
    }
    const world::furnaces::State state = world::furnaces::read(entity->data());
    return FurnaceNumbers{
        .cookTime = state.cookingTimer, .litTime = state.litTimeRemaining, .litDuration = state.litTotalTime};
}

std::vector<std::optional<storage::StoredItemStack>> readWindow(world::LoadedWorld& world, const OpenWindow& window,
                                                                const player::PlayerSession* owner) {
    std::vector<std::optional<storage::StoredItemStack>> slots;
    slots.reserve(static_cast<std::size_t>(window.size));
    for (std::int32_t slot = 0; slot < window.size; ++slot) {
        slots.push_back(readWindowSlot(world, window, slot, owner));
    }
    return slots;
}

std::optional<storage::StoredItemStack> readWindowSlot(world::LoadedWorld& world, const OpenWindow& window,
                                                       const std::int32_t slot, const player::PlayerSession* owner) {
    // An ender chest is a window over the player rather than over the block that opened it.
    if (window.playerOwned) {
        if (owner == nullptr || slot < 0 || static_cast<std::size_t>(slot) >= owner->enderChest.size()) {
            return std::nullopt;
        }
        return owner->enderChest[static_cast<std::size_t>(slot)];
    }
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
                     const std::optional<storage::StoredItemStack>& stack, player::PlayerSession* owner) {
    if (window.playerOwned) {
        if (owner == nullptr || slot < 0 || static_cast<std::size_t>(slot) >= owner->enderChest.size()) {
            return false;
        }
        owner->enderChest[static_cast<std::size_t>(slot)] = stack;
        return true;
    }
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
