#include "ContainerSlotsShared.hpp"

#include <basalt/server/plugins/PluginHookEventTypes.hpp>
#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/ExperienceOrbSystem.hpp>
#include <basalt/server/runtime/MapSystem.hpp>
#include <basalt/server/runtime/UiStateSync.hpp>
#include <basalt/server/runtime/VillagerTradeController.hpp>
#include <basalt/server/runtime/item/ItemComponents.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/plugins/EventSink.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/Furnaces.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Looms.hpp>
#include <basalt/server/runtime/world/PlayerProgress.hpp>
#include <basalt/server/runtime/world/RealItemTags.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>

#include <algorithm>
#include <cstdint>
#include <random>
#include <utility>

namespace basalt::server::runtime {
using namespace containerSlots;

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
    // parity: `FurnaceResultSlot.mayPlace`.
    if (openBlockSize == world::furnaces::kSize && block->index == world::furnaces::kResultSlot) {
        return false;
    }
    // parity: `MerchantResultSlot.mayPlace`.
    if (openMerchant_ && block->index == VillagerTradeController::kResult) {
        return false;
    }
    // parity: `StonecutterMenu.ResultSlot.mayPlace`.
    if (openStonecutter_ && block->index == Menu::kStonecutterResult) {
        return false;
    }
    if (openAnvil_ && block->index == Menu::kAnvilResult) {
        return false;
    }
    if (openGrindstone_ && block->index == Menu::kGrindstoneResult) {
        return false;
    }
    if (openCartography_ && block->index == Menu::kCartographyResult) {
        return false;
    }
    if (openLoom_ && block->index == Menu::kLoomResult) {
        return false;
    }
    return !(openSmithing_ && block->index == Menu::kSmithingResult);
}

bool ContainerMenuController::hasBlockSlots() const {
    return openBlock.has_value() || openEnderChest_ || openEntity_.has_value() || openMerchant_ || openStonecutter_ ||
           openSmithing_ || openAnvil_ || openGrindstone_ || openEnchantment_ || openBeacon_ || openLoom_ ||
           openCartography_;
}

bool ContainerMenuController::accepts(const MenuSlot& target, const storage::StoredItemStack& stack) const {
    const auto* block = std::get_if<player::menuSlot::Block>(&target);
    if (block == nullptr) {
        return true;
    }
    if (mountOpen()) {
        return mountAccepts(block->index, stack);
    }
    // parity: `BeaconMenu.PaymentSlot.mayPlace`, which is the `beacon_payment_items` tag.
    if (openBeacon_) {
        const std::optional<Identifier> payment = parsedOrNothing(stack.id);
        return block->index == Menu::kBeaconPayment && payment.has_value() && owner_->options().itemTags != nullptr &&
               world::realItemTags::contains(*owner_->options().itemTags, "minecraft:beacon_payment_items", *payment);
    }
    // parity: `CartographyTableMenu`'s two inputs.
    if (openCartography_) {
        if (block->index == Menu::kCartographyMap) {
            return stack.components.contains("minecraft:map_id");
        }
        return block->index == Menu::kCartographyAdditional &&
               (stack.id == "minecraft:paper" || stack.id == "minecraft:map" || stack.id == "minecraft:glass_pane");
    }
    // parity: `LoomMenu`'s three inputs.
    if (openLoom_) {
        if (block->index == Menu::kLoomBanner) {
            return world::looms::isBanner(stack.id);
        }
        if (block->index == Menu::kLoomDye) {
            return world::looms::dyeColourOf(stack.id).has_value();
        }
        return block->index == Menu::kLoomPattern && world::looms::patternTagOf(stack.id).has_value();
    }
    // parity: `StonecutterMenu.quickMoveStack` only ever moves into the input.
    if (openStonecutter_) {
        return block->index == Menu::kStonecutterInput;
    }
    // parity: `SmithingMenu.quickMoveStack`.
    if (openSmithing_) {
        return block->index != Menu::kSmithingResult;
    }
    // parity: `ItemCombinerMenu.quickMoveStack`.
    if (openAnvil_) {
        return block->index != Menu::kAnvilResult;
    }
    // parity: `EnchantmentMenu`.
    if (openEnchantment_) {
        return block->index != Menu::kEnchantmentLapis || stack.id == "minecraft:lapis_lazuli";
    }
    // parity: `GrindstoneMenu`'s two slots take only what has wear or enchantments on it.
    if (openGrindstone_) {
        return block->index != Menu::kGrindstoneResult &&
               (item::itemWear::isDamageable(stack) ||
                stack.components.find(item::itemComponents::kEnchantments) != nullptr ||
                stack.components.find(item::itemComponents::kStoredEnchantments) != nullptr);
    }
    // parity: `BrewingStandMenu`'s potion, ingredient and fuel slots.
    if (brewingStandOpen()) {
        const std::optional<Identifier> brewed = parsedOrNothing(stack.id);
        return brewed.has_value() &&
               world::containers::acceptsThroughFace(world::brewingStands::block().path(), block->index, *brewed, true);
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
        recipeUsed_.reset();
        return;
    }
    const std::optional<server::world::loot::ItemStack> assembled = recipe->assembleSmithing(*base, *addition);
    smithingSlots_[Menu::kSmithingResult] = assembled.has_value() ? std::optional{storedOf(*assembled)} : std::nullopt;
    const wire::common::Identifier* name = recipes->nameOf(recipe);
    recipeUsed_ = name != nullptr && assembled.has_value() ? std::optional{*name} : std::nullopt;
    std::int32_t noCost = 0;
    announceResult("smithing", smithingSlots_[Menu::kSmithingResult], noCost);
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
        recipeUsed_.reset();
        sendInventory();
        return;
    }
    const std::vector<const server::world::crafting::Recipe*> options = recipes->stonecuttingFor(*input);
    if (button < 0 || static_cast<std::size_t>(button) >= options.size()) {
        stonecutterResult_.reset();
        recipeUsed_.reset();
    } else {
        const server::world::crafting::Recipe* chosen = options[static_cast<std::size_t>(button)];
        stonecutterResult_ = storedOf(chosen->result);
        // parity: `StonecutterMenu.setupResultSlot` sets the recipe used along with the result.
        const wire::common::Identifier* name = recipes->nameOf(chosen);
        recipeUsed_ = name != nullptr ? std::optional{*name} : std::nullopt;
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
        recipeUsed_.reset();
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
        // Components go into the grid too: dye, copy and repair recipes are defined by what they do to them.
        cells.push_back(index < static_cast<std::int32_t>(craftingGrid_.size())
                            ? craftingStackOf(craftingGrid_[static_cast<std::size_t>(index)])
                            : std::nullopt);
    }
    const server::world::crafting::CraftingInput input =
        server::world::crafting::CraftingInput::of(width, height, cells);
    const std::optional<world::RealRecipes::Match> match = recipes->recipeMatching(input);
    // parity: `limited_crafting`.
    const bool limited = owner_->currentWorld() != nullptr &&
                         owner_->currentWorld()->gameRules().get(world::gameRules::kLimitedCrafting);
    bool known = !limited;
    if (limited && match.has_value() && owner_->options().progress != nullptr && owner_->playerSession() != nullptr) {
        known =
            owner_->options().progress->knownRecipes(owner_->playerSession()->uuid().toString()).contains(match->id);
    }
    if (!match.has_value() || !known) {
        craftingResult_.reset();
        recipeUsed_.reset();
        return;
    }
    const std::optional<server::world::loot::ItemStack> result = recipes->resultFor(input);
    craftingResult_ = result.has_value() ? std::optional{storedOf(*result)} : std::nullopt;
    // parity: `MapExtendingRecipe.matches`.
    if (craftingResult_.has_value() && craftingResult_->components.contains("minecraft:map_post_processing")) {
        const std::optional<std::int32_t> mapId = MapSystem::mapIdOf(craftingResult_->components);
        const std::optional<MapSystem::Summary> data = mapId.has_value() && owner_->options().maps != nullptr
                                                           ? owner_->options().maps->summaryOf(*mapId)
                                                           : std::nullopt;
        if (!data.has_value() || data->scale >= 4) {
            craftingResult_.reset();
        }
    }
    recipeUsed_ = craftingResult_.has_value() ? std::optional{match->id} : std::nullopt;
    std::int32_t noCost = 0;
    announceResult("crafting", craftingResult_, noCost);
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
    // The grid is trimmed to its occupied box, so the recipe answers in that box's order.
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

void ContainerMenuController::awardRecipeCrafted() {
    world::AdvancementTriggers* triggers = owner_->options().advancementTriggers;
    const PlayerSession* session = owner_->playerSession();
    if (triggers == nullptr || session == nullptr || !recipeUsed_.has_value()) {
        return;
    }
    triggers->onRecipeCrafted(session->name(), session->uuid().toString(), *recipeUsed_);
    // parity: `RecipeCraftingHolder.awardUsedRecipes`, the recipe learnt after the trigger.
    if (PlayerSession* crafter = owner_->playerSession(); crafter != nullptr) {
        awardRecipes(*crafter, {*recipeUsed_});
    }
}

void ContainerMenuController::takeCraftingResult(PlayerSession& session) {
    if (!craftingResult_.has_value()) {
        return;
    }
    const storage::StoredItemStack result = *craftingResult_;
    if (carriedItem_.has_value()) {
        if (carriedItem_->id != result.id || carriedItem_->components != result.components) {
            return;
        }
        if (carriedItem_->count + result.count > PlayerInventory::maxStackIn(result)) {
            return;
        }
        carriedItem_->count += result.count;
    } else {
        carriedItem_ = result;
    }
    // parity: `ResultSlot.checkTakeAchievements` over `ItemStack.onCraftedBy`.
    session.stats.increment(storage::stats::crafted(result.id), result.count);
    awardRecipeCrafted();
    settleCraftedMaps(session);
    // parity: `Recipe.getRemainingItems`.
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
    if (session->connection().setHeldSlot) {
        session->connection().setHeldSlot(session->inventory.heldSlot());
    }
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
