#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/UiStateSync.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/Furnaces.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/PlayerProgress.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime {
namespace {

using player::Menu;
using player::MenuSlot;
using player::PlayerInventory;
using player::PlayerSession;
using wire::common::Identifier;
using wire::protocol::play::BlockPos;

std::optional<Identifier> parsedOrNothing(const std::string& id) {
    const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(id);
    return parsed.has_value() ? std::optional{*parsed} : std::nullopt;
}

/// A stored stack as the crafting and smithing tables want one.
std::optional<server::world::loot::ItemStack> craftingStackOf(const std::optional<storage::StoredItemStack>& stack) {
    if (!stack.has_value()) {
        return std::nullopt;
    }
    const std::optional<Identifier> id = parsedOrNothing(stack->id);
    if (!id.has_value()) {
        return std::nullopt;
    }
    return server::world::loot::ItemStack{.item = *id, .count = stack->count, .components = stack->components};
}

storage::StoredItemStack storedOf(const server::world::loot::ItemStack& stack) {
    return storage::StoredItemStack{.id = stack.item.toString(), .count = stack.count, .components = stack.components};
}

} // namespace

std::optional<std::pair<BlockPos, std::int32_t>> ContainerMenuController::halfFor(std::int32_t index) const {
    if (openBlockSecond.has_value() && index >= Menu::kChestSize) {
        return std::pair{*openBlockSecond, index - Menu::kChestSize};
    }
    if (!openBlock.has_value()) {
        return std::nullopt;
    }
    return std::pair{*openBlock, index};
}

std::int32_t ContainerMenuController::halfSize() const {
    return openBlockSecond.has_value() ? Menu::kChestSize : openBlockSize;
}

std::optional<storage::StoredItemStack> ContainerMenuController::readBlockSlot(std::int32_t index) const {
    if (openSmithing_) {
        return index >= 0 && static_cast<std::size_t>(index) < smithingSlots_.size()
                   ? smithingSlots_[static_cast<std::size_t>(index)]
                   : std::nullopt;
    }
    if (openStonecutter_) {
        return index == Menu::kStonecutterInput ? stonecutterInput_ : stonecutterResult_;
    }
    if (pluginMenu_.has_value()) {
        return index >= 0 && static_cast<std::size_t>(index) < pluginMenu_->size()
                   ? (*pluginMenu_)[static_cast<std::size_t>(index)]
                   : std::nullopt;
    }
    if (openEnderChest_) {
        const PlayerSession* session = owner_->playerSession();
        if (session == nullptr || index < 0 || static_cast<std::size_t>(index) >= session->enderChest.size()) {
            return std::nullopt;
        }
        return session->enderChest[static_cast<std::size_t>(index)];
    }
    const std::optional<std::pair<BlockPos, std::int32_t>> half = halfFor(index);
    world::LoadedWorld* world = owner_->currentWorld();
    if (!half.has_value() || world == nullptr) {
        return std::nullopt;
    }
    server::world::Chunk* chunk = world->state().chunkIfLoaded(half->first.x >> 4, half->first.z >> 4);
    const server::world::ChunkBlockEntity* entity =
        chunk == nullptr ? nullptr : chunk->blockEntityAt(half->first.x & 15, half->first.y, half->first.z & 15);
    if (entity == nullptr) {
        return std::nullopt;
    }
    const world::blockInventory::Slots slots = world::blockInventory::read(entity->data(), halfSize());
    if (half->second < 0 || static_cast<std::size_t>(half->second) >= slots.size()) {
        return std::nullopt;
    }
    const std::optional<server::world::loot::ItemStack>& stack = slots[static_cast<std::size_t>(half->second)];
    return stack.has_value() ? std::optional{storedOf(*stack)} : std::nullopt;
}

void ContainerMenuController::writeBlockSlot(std::int32_t index, const std::optional<storage::StoredItemStack>& stack) {
    if (openSmithing_) {
        if (index < 0 || static_cast<std::size_t>(index) >= smithingSlots_.size()) {
            return;
        }
        if (index == Menu::kSmithingResult) {
            // Taking the result is the only way this slot is written, and it costs the inputs.
            if (!stack.has_value()) {
                takeSmithingResult();
            }
            return;
        }
        smithingSlots_[static_cast<std::size_t>(index)] = stack;
        recomputeSmithingResult();
        return;
    }
    if (openStonecutter_) {
        if (index == Menu::kStonecutterInput) {
            stonecutterInput_ = stack;
            // parity: `slotsChanged`, which clears the choice whenever the input changes.
            stonecutterResult_.reset();
        } else if (!stack.has_value()) {
            takeStonecutterResult();
        }
        return;
    }
    if (openEnderChest_) {
        PlayerSession* session = owner_->playerSession();
        if (session == nullptr || index < 0 || static_cast<std::size_t>(index) >= session->enderChest.size()) {
            return;
        }
        session->enderChest[static_cast<std::size_t>(index)] = stack;
        return;
    }
    const std::optional<std::pair<BlockPos, std::int32_t>> half = halfFor(index);
    world::LoadedWorld* world = owner_->currentWorld();
    if (!half.has_value() || world == nullptr) {
        return;
    }
    server::world::Chunk* chunk = world->state().chunkIfLoaded(half->first.x >> 4, half->first.z >> 4);
    if (chunk == nullptr) {
        return;
    }
    server::world::ChunkBlockEntity* entity =
        chunk->blockEntityAt(half->first.x & 15, half->first.y, half->first.z & 15);
    if (entity == nullptr) {
        // A block that never got its block entity (one from worldgen, or one written by a command)
        // used to swallow whatever was put into it: the cursor was already empty and the write had
        // nowhere to land. It gets one here instead.
        const std::optional<std::int32_t> stateId =
            world->state().blockState(half->first.x, half->first.y, half->first.z);
        const Identifier* block =
            stateId.has_value() ? server::registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
        if (block == nullptr) {
            return;
        }
        chunk->setBlockEntity(
            server::world::ChunkBlockEntity{half->first.x & 15, half->first.y, half->first.z & 15, *block, {}});
        entity = chunk->blockEntityAt(half->first.x & 15, half->first.y, half->first.z & 15);
        if (entity == nullptr) {
            return;
        }
    }
    // Every slot of the block, not the three a cooker has: writing back fewer than it holds is what
    // would empty a chest the moment somebody moved its first item.
    world::blockInventory::Slots slots = world::blockInventory::read(entity->data(), halfSize());
    if (half->second < 0 || static_cast<std::size_t>(half->second) >= slots.size()) {
        owner_->logger().warn([&] {
            return "dropped a write to slot " + std::to_string(half->second) + " of a container that has " +
                   std::to_string(slots.size());
        });
        return;
    }
    const std::optional<server::world::loot::ItemStack>& was = slots[static_cast<std::size_t>(half->second)];
    const std::optional<server::world::loot::ItemStack> put = craftingStackOf(stack);
    const bool sameItem = was.has_value() && put.has_value() && was->item == put->item;
    slots[static_cast<std::size_t>(half->second)] = put;
    world::blockInventory::write(entity->data(), slots);
    // parity: `AbstractFurnaceBlockEntity.setItem`, which recomputes the cook time when the thing
    // being cooked changes: a new item does not inherit the last one's progress.
    const std::optional<std::int32_t> cookingState =
        world->state().blockState(half->first.x, half->first.y, half->first.z);
    const Identifier* cooking =
        cookingState.has_value() ? server::registry::blockStateDefinitions::blockOf(*cookingState) : nullptr;
    const std::optional<server::world::crafting::Cooker> cooker =
        cooking == nullptr ? std::nullopt : world::furnaces::cookerFor(cooking->path());
    if (half->second == world::furnaces::kInputSlot && cooker.has_value()) {
        world::furnaces::State state = world::furnaces::read(entity->data());
        state.cookingTimer = 0;
        world::RealRecipes* recipes = owner_->options().recipes;
        state.cookingTotalTime =
            world::furnaces::cookTimeFor(state, [recipes, cooker](const server::world::loot::ItemStack& source) {
                return recipes == nullptr ? nullptr : recipes->cookingFor(*cooker, source);
            });
        world::furnaces::write(state, entity->data());
    }
    chunk->markDirty();
    if (cooking != nullptr && world::containers::setItemTellsComparators(cooking->path(), half->second, sameItem)) {
        world::redstoneComponents::scheduleComparatorsAround(world->state(), half->first.x, half->first.y,
                                                             half->first.z);
    }
}

std::optional<world::furnaces::State> ContainerMenuController::furnaceState() const {
    world::LoadedWorld* world = owner_->currentWorld();
    if (!openBlock.has_value() || world == nullptr) {
        return std::nullopt;
    }
    server::world::Chunk* chunk = world->state().chunkIfLoaded(openBlock->x >> 4, openBlock->z >> 4);
    const server::world::ChunkBlockEntity* entity =
        chunk == nullptr ? nullptr : chunk->blockEntityAt(openBlock->x & 15, openBlock->y, openBlock->z & 15);
    if (entity == nullptr) {
        return std::nullopt;
    }
    return world::furnaces::read(entity->data());
}

void ContainerMenuController::syncOpenContainer() {
    sendFurnaceBars();
    PlayerSession* session = owner_->playerSession();
    const std::optional<world::furnaces::State> state = furnaceState();
    if (session == nullptr || !state.has_value()) {
        return;
    }
    for (std::size_t index = 0; index < state->items.size(); index++) {
        const std::optional<std::int32_t> menuSlot =
            openMenu.menuSlotOf(player::menuSlot::Block{static_cast<std::int32_t>(index)});
        if (!menuSlot.has_value()) {
            continue;
        }
        const std::optional<storage::StoredItemStack> now =
            state->items[index].has_value() ? std::optional{storedOf(*state->items[index])} : std::nullopt;
        if (index < shownBlockSlots_.size() && shownBlockSlots_[index] == now) {
            continue;
        }
        if (index < shownBlockSlots_.size()) {
            shownBlockSlots_[index] = now;
        }
        owner_->sendPlayPacket(wire::protocol::play::ContainerSetSlotPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            wire::protocol::play::ContainerSetSlotPacket{.containerId = openMenu.containerId(),
                                                         .stateId = ++containerStateId_,
                                                         .slot = static_cast<std::int16_t>(*menuSlot),
                                                         .item = session->inventory.toWireStack(now)}
                .write(buffer);
        });
    }
}

void ContainerMenuController::sendFurnaceBars() {
    const std::optional<world::furnaces::State> state = furnaceState();
    if (!state.has_value()) {
        return;
    }
    const std::array<std::int32_t, 4> values{state->litTimeRemaining, state->litTotalTime, state->cookingTimer,
                                             state->cookingTotalTime};
    for (std::size_t index = 0; index < values.size(); index++) {
        if (values[index] == openBlockData_[index]) {
            continue;
        }
        openBlockData_[index] = values[index];
        owner_->sendPlayPacket(wire::protocol::play::ContainerSetDataPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            wire::protocol::play::ContainerSetDataPacket{.containerId = openMenu.containerId(),
                                                         .id = static_cast<std::int16_t>(index),
                                                         .value = static_cast<std::int16_t>(values[index])}
                .write(buffer);
        });
    }
}

std::optional<storage::StoredItemStack> ContainerMenuController::readSlot(const PlayerSession& session,
                                                                          const MenuSlot& slot) const {
    if (const auto* inventory = std::get_if<player::menuSlot::Inventory>(&slot); inventory != nullptr) {
        return session.inventory.at(inventory->index);
    }
    if (const auto* grid = std::get_if<player::menuSlot::Grid>(&slot); grid != nullptr) {
        return grid->index >= 0 && static_cast<std::size_t>(grid->index) < craftingGrid_.size()
                   ? craftingGrid_[static_cast<std::size_t>(grid->index)]
                   : std::nullopt;
    }
    if (const auto* block = std::get_if<player::menuSlot::Block>(&slot); block != nullptr) {
        return readBlockSlot(block->index);
    }
    return craftingResult_;
}

void ContainerMenuController::writeSlot(PlayerSession& session, const MenuSlot& slot,
                                        const std::optional<storage::StoredItemStack>& stack) {
    if (const auto* inventory = std::get_if<player::menuSlot::Inventory>(&slot); inventory != nullptr) {
        session.inventory.set(inventory->index, stack);
        return;
    }
    if (const auto* grid = std::get_if<player::menuSlot::Grid>(&slot); grid != nullptr) {
        if (grid->index >= 0 && static_cast<std::size_t>(grid->index) < craftingGrid_.size()) {
            craftingGrid_[static_cast<std::size_t>(grid->index)] = stack;
        }
        recomputeCraftingResult();
        return;
    }
    if (const auto* block = std::get_if<player::menuSlot::Block>(&slot); block != nullptr) {
        writeBlockSlot(block->index, stack);
    }
}

bool ContainerMenuController::canPlaceInto(const MenuSlot& slot) const {
    if (std::holds_alternative<player::menuSlot::Result>(slot)) {
        return false;
    }
    const auto* block = std::get_if<player::menuSlot::Block>(&slot);
    if (block == nullptr) {
        return true;
    }
    // parity: `FurnaceResultSlot.mayPlace`, which is a *furnace's* third slot and nobody else's: a
    // chest's third slot took nothing until this stopped being unconditional.
    if (openBlockSize == world::furnaces::kSize && block->index == world::furnaces::kResultSlot) {
        return false;
    }
    // parity: `StonecutterMenu.ResultSlot.mayPlace`, and the smithing table's, both always false.
    if (openStonecutter_ && block->index == Menu::kStonecutterResult) {
        return false;
    }
    return !(openSmithing_ && block->index == Menu::kSmithingResult);
}

bool ContainerMenuController::hasBlockSlots() const {
    return openBlock.has_value() || openEnderChest_ || openStonecutter_ || openSmithing_;
}

bool ContainerMenuController::accepts(const MenuSlot& target, const storage::StoredItemStack& stack) const {
    const auto* block = std::get_if<player::menuSlot::Block>(&target);
    if (block == nullptr) {
        return true;
    }
    // parity: `StonecutterMenu.quickMoveStack` only ever moves into the input.
    if (openStonecutter_) {
        return block->index == Menu::kStonecutterInput;
    }
    // parity: `SmithingMenu.quickMoveStack`, which tries the three inputs and never the result.
    if (openSmithing_) {
        return block->index != Menu::kSmithingResult;
    }
    if (openBlockSize != world::furnaces::kSize) {
        return true;
    }
    const std::optional<Identifier> item = parsedOrNothing(stack.id);
    if (!item.has_value()) {
        return false;
    }
    if (block->index == world::furnaces::kInputSlot) {
        return true;
    }
    // Nothing goes into the slot the cooked thing comes out of.
    return block->index == world::furnaces::kFuelSlot && server::registry::fuels::isFuel(*item);
}

void ContainerMenuController::recomputeSmithingResult() {
    world::RealRecipes* recipes = owner_->options().recipes;
    const std::optional<server::world::loot::ItemStack> templateStack =
        craftingStackOf(smithingSlots_[Menu::kSmithingTemplate]);
    const std::optional<server::world::loot::ItemStack> base = craftingStackOf(smithingSlots_[Menu::kSmithingBase]);
    const std::optional<server::world::loot::ItemStack> addition =
        craftingStackOf(smithingSlots_[Menu::kSmithingAddition]);
    const server::world::crafting::Recipe* recipe =
        recipes == nullptr
            ? nullptr
            : recipes->smithingFor(templateStack.has_value() ? &*templateStack : nullptr,
                                   base.has_value() ? &*base : nullptr, addition.has_value() ? &*addition : nullptr);
    if (recipe == nullptr || !base.has_value() || !addition.has_value()) {
        smithingSlots_[Menu::kSmithingResult] = std::nullopt;
        return;
    }
    const std::optional<server::world::loot::ItemStack> assembled = recipe->assembleSmithing(*base, *addition);
    smithingSlots_[Menu::kSmithingResult] = assembled.has_value() ? std::optional{storedOf(*assembled)} : std::nullopt;
}

void ContainerMenuController::takeSmithingResult() {
    for (const std::int32_t slot : {Menu::kSmithingTemplate, Menu::kSmithingBase, Menu::kSmithingAddition}) {
        std::optional<storage::StoredItemStack>& stack = smithingSlots_[static_cast<std::size_t>(slot)];
        if (!stack.has_value()) {
            continue;
        }
        if (stack->count <= 1) {
            stack.reset();
        } else {
            stack->count -= 1;
        }
    }
    recomputeSmithingResult();
}

void ContainerMenuController::selectStonecutterRecipe(std::int32_t button) {
    world::RealRecipes* recipes = owner_->options().recipes;
    const std::optional<server::world::loot::ItemStack> input = craftingStackOf(stonecutterInput_);
    if (!input.has_value() || recipes == nullptr) {
        stonecutterResult_.reset();
        sendInventory();
        return;
    }
    const std::vector<const server::world::crafting::Recipe*> options = recipes->stonecuttingFor(*input);
    if (button < 0 || static_cast<std::size_t>(button) >= options.size()) {
        stonecutterResult_.reset();
    } else {
        stonecutterResult_ = storedOf(options[static_cast<std::size_t>(button)]->result);
    }
    sendInventory();
}

void ContainerMenuController::takeStonecutterResult() {
    if (!stonecutterInput_.has_value()) {
        return;
    }
    if (stonecutterInput_->count <= 1) {
        stonecutterInput_.reset();
        stonecutterResult_.reset();
    } else {
        stonecutterInput_->count -= 1;
    }
}

void ContainerMenuController::recomputeCraftingResult() {
    world::RealRecipes* recipes = owner_->options().recipes;
    if (recipes == nullptr) {
        craftingResult_.reset();
        return;
    }
    const std::int32_t width = openMenu.gridWidth();
    const std::int32_t height = openMenu.gridHeight();
    std::vector<std::optional<server::world::loot::ItemStack>> cells;
    for (std::int32_t index = 0; index < width * height; index++) {
        // The components go into the grid too: a dye, a copy or a repair is defined by what it does
        // to them, so a recipe that cannot see them cannot exist.
        cells.push_back(index < static_cast<std::int32_t>(craftingGrid_.size())
                            ? craftingStackOf(craftingGrid_[static_cast<std::size_t>(index)])
                            : std::nullopt);
    }
    const server::world::crafting::CraftingInput input =
        server::world::crafting::CraftingInput::of(width, height, cells);
    const std::optional<world::RealRecipes::Match> match = recipes->recipeMatching(input);
    // parity: the `limited_crafting` game rule, which is what makes `/recipe give` and `/recipe
    // take` mean something: with it on, a player crafts only what they have been taught.
    const bool limited = owner_->currentWorld() != nullptr &&
                         owner_->currentWorld()->gameRules().get(world::gameRules::kLimitedCrafting);
    bool known = !limited;
    if (limited && match.has_value() && owner_->options().progress != nullptr && owner_->playerSession() != nullptr) {
        known =
            owner_->options().progress->knownRecipes(owner_->playerSession()->uuid().toString()).contains(match->id);
    }
    if (!match.has_value() || !known) {
        craftingResult_.reset();
        return;
    }
    const std::optional<server::world::loot::ItemStack> result = recipes->resultFor(input);
    craftingResult_ = result.has_value() ? std::optional{storedOf(*result)} : std::nullopt;
}

std::vector<std::optional<storage::StoredItemStack>> ContainerMenuController::craftingRemainders() const {
    world::RealRecipes* recipes = owner_->options().recipes;
    if (recipes == nullptr) {
        return {};
    }
    const std::int32_t width = openMenu.gridWidth();
    const std::int32_t height = openMenu.gridHeight();
    std::vector<std::optional<server::world::loot::ItemStack>> cells;
    for (std::int32_t index = 0; index < width * height; index++) {
        cells.push_back(index < static_cast<std::int32_t>(craftingGrid_.size())
                            ? craftingStackOf(craftingGrid_[static_cast<std::size_t>(index)])
                            : std::nullopt);
    }
    const server::world::crafting::CraftingInput input =
        server::world::crafting::CraftingInput::of(width, height, cells);
    const std::optional<world::RealRecipes::Match> match = recipes->recipeMatching(input);
    if (!match.has_value() || match->recipe == nullptr) {
        return {};
    }
    // The grid is trimmed to its occupied box, so the recipe answers in that box's order: walk the
    // same cells in the same order to line the two up.
    const std::vector<std::optional<server::world::loot::ItemStack>> answered = match->recipe->remainingItems(input);
    std::vector<std::optional<storage::StoredItemStack>> remainders;
    std::size_t next = 0;
    for (std::int32_t index = 0; index < width * height; index++) {
        const bool occupied = index < static_cast<std::int32_t>(craftingGrid_.size()) &&
                              craftingGrid_[static_cast<std::size_t>(index)].has_value();
        if (!occupied || next >= answered.size()) {
            remainders.emplace_back();
            continue;
        }
        const std::optional<server::world::loot::ItemStack>& left = answered[next++];
        remainders.push_back(left.has_value() ? std::optional{storedOf(*left)} : std::nullopt);
    }
    return remainders;
}

void ContainerMenuController::takeCraftingResult(PlayerSession& session) {
    static_cast<void>(session);
    if (!craftingResult_.has_value()) {
        return;
    }
    const storage::StoredItemStack result = *craftingResult_;
    if (carriedItem_.has_value()) {
        // The cursor has to be empty or hold the same thing, and have room for a whole batch.
        if (carriedItem_->id != result.id || carriedItem_->components != result.components) {
            return;
        }
        if (carriedItem_->count + result.count > PlayerInventory::kMaxStack) {
            return;
        }
        carriedItem_->count += result.count;
    } else {
        carriedItem_ = result;
    }
    // parity: `Recipe.getRemainingItems`: a cell whose item leaves something behind (a bucket, a
    // bottle) keeps it instead of emptying, and the book being copied stays where it was.
    const std::vector<std::optional<storage::StoredItemStack>> remaining = craftingRemainders();
    for (std::int32_t index = 0; index < openMenu.gridSize(); index++) {
        if (index >= static_cast<std::int32_t>(craftingGrid_.size())) {
            break;
        }
        std::optional<storage::StoredItemStack>& cell = craftingGrid_[static_cast<std::size_t>(index)];
        if (!cell.has_value()) {
            continue;
        }
        const std::int32_t left = cell->count - 1;
        const std::optional<storage::StoredItemStack> leftover = index < static_cast<std::int32_t>(remaining.size())
                                                                     ? remaining[static_cast<std::size_t>(index)]
                                                                     : std::nullopt;
        if (left > 0) {
            cell->count = left;
        } else {
            cell = leftover;
        }
    }
    recomputeCraftingResult();
}

void ContainerMenuController::returnCraftingGrid(PlayerSession& session) {
    for (std::optional<storage::StoredItemStack>& cell : craftingGrid_) {
        if (!cell.has_value()) {
            continue;
        }
        const storage::StoredItemStack held = *cell;
        cell.reset();
        const std::int32_t leftover = session.inventory.add(held);
        if (leftover > 0) {
            storage::StoredItemStack dropped = held;
            dropped.count = leftover;
            dropStack(session, dropped);
        }
    }
    // The cursor goes the same way: it is not part of the inventory either.
    if (carriedItem_.has_value()) {
        const storage::StoredItemStack held = *carriedItem_;
        carriedItem_.reset();
        const std::int32_t leftover = session.inventory.add(held);
        if (leftover > 0) {
            storage::StoredItemStack dropped = held;
            dropped.count = leftover;
            dropStack(session, dropped);
        }
    }
    craftingResult_.reset();
}

void ContainerMenuController::sendInventory() {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return;
    }
    const std::int32_t state = ++containerStateId_;
    std::vector<std::optional<wire::protocol::play::WireItemStack>> items;
    for (std::int32_t index = 0; index < openMenu.slotCount(); index++) {
        const std::optional<MenuSlot> slot = openMenu.slotAt(index);
        items.push_back(slot.has_value() ? session->inventory.toWireStack(readSlot(*session, *slot)) : std::nullopt);
    }
    owner_->sendCompressed(wire::protocol::play::ContainerSetContentPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        wire::protocol::play::ContainerSetContentPacket{.containerId = openMenu.containerId(),
                                                        .stateId = state,
                                                        .items = items,
                                                        .carriedItem = session->inventory.toWireStack(carriedItem_)}
            .write(buffer);
    });
    owner_->sendCompressed(wire::protocol::play::SetHeldSlotPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        wire::protocol::play::SetHeldSlotPacket{.slot = session->inventory.heldSlot()}.write(buffer);
    });
}

void ContainerMenuController::sendInventoryAll() {
    sendInventory();
    owner_->uiState().syncHeldAttributes();
}

void ContainerMenuController::sendInventorySlot(std::int32_t slot) {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return;
    }
    if (slot == PlayerInventory::kHotbarStart + session->inventory.heldSlot()) {
        owner_->uiState().syncHeldAttributes();
    }
    const std::optional<std::int32_t> menuSlot = openMenu.menuSlotOf(player::menuSlot::Inventory{slot});
    owner_->sendPlayPacket(wire::protocol::play::ContainerSetSlotPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        wire::protocol::play::ContainerSetSlotPacket{
            .containerId = openMenu.containerId(),
            .stateId = ++containerStateId_,
            .slot = static_cast<std::int16_t>(menuSlot.value_or(PlayerInventory::menuSlotOf(slot))),
            .item = session->inventory.toWireSlot(slot)}
            .write(buffer);
    });
}

} // namespace basalt::server::runtime
