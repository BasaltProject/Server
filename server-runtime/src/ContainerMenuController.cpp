#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockInteractionHandler.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/UiStateSync.hpp>
#include <basalt/server/runtime/VillagerTradeController.hpp>
#include <basalt/server/runtime/item/ItemComponentCodecs.hpp>
#include <basalt/server/runtime/player/PlayerContainers.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/ContainerOpeners.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/Crafters.hpp>
#include <basalt/server/runtime/world/DoubleChests.hpp>
#include <basalt/server/runtime/world/Lecterns.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/Shulkers.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/protocol/play/BlockEntityPackets.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>
#include <basalt/wire/text/MiniMessage.hpp>

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
using wire::protocol::play::OpenScreenPacket;
using wire::protocol::play::SoundSource;
using wire::text::Component;

std::string containerTitleKey(const std::string& block, bool paired) {
    if (world::shulkers::isShulkerBox(block)) {
        return "container.shulkerBox";
    }
    if (paired) {
        return "container.chestDouble";
    }
    if (block == "chest" || block == "trapped_chest") {
        return "container.chest";
    }
    return "container." + block;
}

} // namespace

ContainerMenuController::ContainerMenuController(ClientSession& owner, Hooks hooks, Options options)
    : owner_(&owner), hooks_(std::move(hooks)), options_(std::move(options)) {}

std::int32_t ContainerMenuController::allocateContainerId() {
    nextContainerId =
        nextContainerId >= ClientSession::kMaxContainerId ? ClientSession::kFirstBlockContainerId : nextContainerId + 1;
    return nextContainerId;
}

void ContainerMenuController::openCraftingTable(PlayerSession& session) {
    if (hooks_.openAllowed && !hooks_.openAllowed("crafting", std::nullopt)) {
        return;
    }
    returnCraftingGrid(session);
    openMenu = Menu::craftingTable(allocateContainerId());
    recomputeCraftingResult();
    owner_->sendCompressed(OpenScreenPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        OpenScreenPacket{.containerId = openMenu.containerId(),
                         .menuType = wire::protocol::play::menuType::kCrafting,
                         .title = Component::translatable("container.crafting")}
            .write(buffer);
    });
    sendInventory();
}

void ContainerMenuController::openFurnace(PlayerSession& session, const BlockPos& pos, const std::string& cooker) {
    if (hooks_.openAllowed && !hooks_.openAllowed(cooker, pos)) {
        return;
    }
    returnCraftingGrid(session);
    openMenu = Menu::furnace(allocateContainerId());
    openBlock = pos;
    openBlockSize = world::furnaces::kSize;
    openBlockData_ = {-1, -1, -1, -1};
    const std::int32_t menuType = cooker == "blast_furnace" ? wire::protocol::play::menuType::kBlastFurnace
                                  : cooker == "smoker"      ? wire::protocol::play::menuType::kSmoker
                                                            : wire::protocol::play::menuType::kFurnace;
    owner_->sendCompressed(OpenScreenPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        OpenScreenPacket{.containerId = openMenu.containerId(),
                         .menuType = menuType,
                         .title = Component::translatable("container." + cooker)}
            .write(buffer);
    });
    sendInventory();
    sendFurnaceBars();
}

void ContainerMenuController::openEnderChest(PlayerSession& session, const BlockPos& pos) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return;
    }
    const std::optional<std::int32_t> above = world->state().blockState(pos.x, pos.y + 1, pos.z);
    if (!above.has_value() || server::registry::blockSupport::conductsRedstone(*above)) {
        return;
    }
    returnCraftingGrid(session);
    openMenu = Menu::chest(allocateContainerId());
    openBlock.reset();
    openBlockSecond.reset();
    openEnderChest_ = true;
    openBlockSize = storage::StoredPlayer::kEnderChestSlots;
    openBlockData_ = {-1, -1, -1, -1};
    owner_->sendCompressed(OpenScreenPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        OpenScreenPacket{.containerId = openMenu.containerId(),
                         .menuType = wire::protocol::play::menuType::kGeneric9x3,
                         .title = Component::translatable("container.enderchest")}
            .write(buffer);
    });
    sendInventory();
    owner_->blockInteraction().broadcastEffects(
        pos, world->state().blockState(pos.x, pos.y, pos.z).value_or(0),
        {world::blockEffect::Sound{
            .name = "block.ender_chest.open", .source = SoundSource::Blocks, .volume = 0.5F, .pitch = 1.0F}});
}

void ContainerMenuController::openChest(PlayerSession& session, const BlockPos& pos, const std::string& block) {
    returnCraftingGrid(session);
    const std::int32_t containerId = allocateContainerId();
    // parity: `ChestBlock.getMenuProvider`, which hands both halves over as one container.
    world::LoadedWorld* world = owner_->currentWorld();
    const std::optional<std::pair<world::doubleChests::Half, world::doubleChests::Half>> halves =
        world == nullptr ? std::nullopt : world::doubleChests::halves(world->state(), pos.x, pos.y, pos.z);
    openEnderChest_ = false;
    openEntity_.reset();
    openBlockSecond = halves.has_value()
                          ? std::optional{BlockPos{halves->second[0], halves->second[1], halves->second[2]}}
                          : std::nullopt;
    openBlock = halves.has_value() ? BlockPos{halves->first[0], halves->first[1], halves->first[2]} : pos;
    const bool paired = openBlockSecond.has_value();
    // parity: `ChestBlock.getMenuProvider` unpacks both halves before the menu reads a slot.
    if (world != nullptr) {
        world->state().unpackLoot(openBlock->x, openBlock->y, openBlock->z, session.uuid());
        if (paired) {
            world->state().unpackLoot(openBlockSecond->x, openBlockSecond->y, openBlockSecond->z, session.uuid());
        }
    }
    openMenu = paired ? Menu::doubleChest(containerId) : Menu::chest(containerId);
    openBlockSize = paired ? Menu::kDoubleChestSize : Menu::kChestSize;
    openBlockData_ = {-1, -1, -1, -1};
    owner_->sendCompressed(OpenScreenPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        OpenScreenPacket{.containerId = openMenu.containerId(),
                         .menuType = paired ? wire::protocol::play::menuType::kGeneric9x6
                                            : wire::protocol::play::menuType::kGeneric9x3,
                         .title = Component::translatable(containerTitleKey(block, paired))}
            .write(buffer);
    });
    sendInventory();
    openedAt_ = pos;
    announceOpeners(pos, block, world::containerOpeners::open(session.worldName, pos.x, pos.y, pos.z));
    // parity: `ChestBlock`'s lid is per block entity, so both halves of a double chest open.
    if (openBlockSecond.has_value()) {
        const BlockPos at = openBlock == pos ? *openBlockSecond : *openBlock;
        announceOpeners(at, block, world::containerOpeners::open(session.worldName, at.x, at.y, at.z));
    }
}

void ContainerMenuController::announceOpeners(const BlockPos& pos, const std::string& block,
                                              const std::int32_t openers) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return;
    }
    // The lid, the barrel, the sound and the vibration are the same on both editions, so both
    // doors say it the same way: see `player/PlayerContainers`.
    player::OpenersContext context;
    context.blocks = owner_->blockInteraction().breakContext();
    context.nextFloat = options_.nextFloat;
    context.nextSeed = options_.nextSeed;
    player::announceOpeners(context, *world, pos.x, pos.y, pos.z, block, openers);
}

void ContainerMenuController::openPluginMenu(const std::string& title, std::int32_t rows,
                                             const std::map<std::int32_t, PluginMenuItem>& items,
                                             std::function<void(std::int32_t)> onClick) {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return;
    }
    returnCraftingGrid(*session);
    openMenu = Menu::rows(allocateContainerId(), rows);
    openBlock.reset();
    openBlockSecond.reset();
    openEnderChest_ = false;
    openEntity_.reset();
    openStonecutter_ = false;
    openSmithing_ = false;
    openBlockSize = rows * Menu::kColumns;
    openBlockData_ = {-1, -1, -1, -1};
    pluginMenu_ = std::vector<std::optional<storage::StoredItemStack>>(static_cast<std::size_t>(rows * Menu::kColumns));
    pluginMenuClick_ = std::move(onClick);
    fillPluginMenu(items);
    owner_->sendCompressed(OpenScreenPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        OpenScreenPacket{.containerId = openMenu.containerId(),
                         // `minecraft:generic_9x1` through `9x6` are consecutive from zero.
                         .menuType = rows - 1,
                         .title = wire::text::miniMessage::parse(title)}
            .write(buffer);
    });
    sendInventory();
}

void ContainerMenuController::fillPluginMenu(const std::map<std::int32_t, PluginMenuItem>& items) {
    if (!pluginMenu_.has_value()) {
        return;
    }
    std::ranges::fill(*pluginMenu_, std::nullopt);
    for (const auto& [slot, item] : items) {
        if (slot < 0 || static_cast<std::size_t>(slot) >= pluginMenu_->size()) {
            continue;
        }
        wire::nbt::NbtCompound components;
        // The name is MiniMessage on the plugin's side and a component on the wire, so it is parsed
        // here rather than stored as the tags a client would show literally.
        if (item.name.has_value()) {
            components.set("minecraft:custom_name",
                           wire::text::componentNbt::serialize(wire::text::miniMessage::parse(*item.name)));
        }
        if (!item.lore.empty()) {
            wire::nbt::NbtList lines{wire::nbt::NbtType::Compound};
            for (const std::string& line : item.lore) {
                static_cast<void>(
                    lines.push(wire::text::componentNbt::serialize(wire::text::miniMessage::parse(line))));
            }
            components.set("minecraft:lore", wire::nbt::NbtTag::ofList(std::move(lines)));
        }
        (*pluginMenu_)[static_cast<std::size_t>(slot)] =
            storage::StoredItemStack{.id = item.id, .count = std::max(item.count, 1), .components = components};
    }
}

void ContainerMenuController::refreshPluginMenu(const std::map<std::int32_t, PluginMenuItem>& items) {
    if (!pluginMenu_.has_value()) {
        return;
    }
    fillPluginMenu(items);
    sendInventory();
}

void ContainerMenuController::closeOpenMenu() {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr || openMenu.containerId() == 0) {
        return;
    }
    owner_->sendPlayPacket(
        wire::protocol::play::ContainerClosePacket::kClientboundId, [&](wire::io::ByteBuffer& buffer) {
            wire::protocol::play::ContainerClosePacket{.containerId = openMenu.containerId()}.write(buffer);
        });
    closeOpenMenu(*session);
}

void ContainerMenuController::openMerchantMenu(PlayerSession& session) {
    returnCraftingGrid(session);
    openMenu = Menu::merchant(allocateContainerId());
    openBlock.reset();
    openBlockSecond.reset();
    openEnderChest_ = false;
    openEntity_.reset();
    openStonecutter_ = false;
    openSmithing_ = false;
    openAnvil_ = false;
    openGrindstone_ = false;
    openEnchantment_ = false;
    openBeacon_ = false;
    openLoom_ = false;
    openCartography_ = false;
    openMerchant_ = true;
    openBlockSize = 3;
    openBlockData_ = {-1, -1, -1, -1};
}

void ContainerMenuController::openSmithingMenu(PlayerSession& session) {
    returnCraftingGrid(session);
    openMenu = Menu::smithing(allocateContainerId());
    openBlock.reset();
    openBlockSecond.reset();
    openEnderChest_ = false;
    openEntity_.reset();
    openStonecutter_ = false;
    openSmithing_ = true;
    std::ranges::fill(smithingSlots_, std::nullopt);
    openBlockSize = static_cast<std::int32_t>(smithingSlots_.size());
    openBlockData_ = {-1, -1, -1, -1};
    owner_->sendCompressed(OpenScreenPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        OpenScreenPacket{.containerId = openMenu.containerId(),
                         .menuType = wire::protocol::play::menuType::kSmithing,
                         .title = Component::translatable("container.upgrade")}
            .write(buffer);
    });
    sendInventory();
}

void ContainerMenuController::openStonecutterMenu(PlayerSession& session) {
    returnCraftingGrid(session);
    openMenu = Menu::stonecutter(allocateContainerId());
    openBlock.reset();
    openBlockSecond.reset();
    openEnderChest_ = false;
    openEntity_.reset();
    openStonecutter_ = true;
    stonecutterInput_.reset();
    stonecutterResult_.reset();
    openBlockSize = 2;
    openBlockData_ = {-1, -1, -1, -1};
    owner_->sendCompressed(OpenScreenPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        OpenScreenPacket{.containerId = openMenu.containerId(),
                         .menuType = wire::protocol::play::menuType::kStonecutter,
                         .title = Component::translatable("container.stonecutter")}
            .write(buffer);
    });
    sendInventory();
}

void ContainerMenuController::openCrafter(PlayerSession& session, const BlockPos& pos) {
    returnCraftingGrid(session);
    openMenu = Menu::crafter(allocateContainerId());
    openBlock = pos;
    openBlockSize = Menu::kCrafterSize;
    openBlockData_ = {-1, -1, -1, -1};
    owner_->sendCompressed(OpenScreenPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        OpenScreenPacket{.containerId = openMenu.containerId(),
                         .menuType = wire::protocol::play::menuType::kCrafter3x3,
                         .title = Component::translatable("container.crafter")}
            .write(buffer);
    });
    sendInventory();
}

void ContainerMenuController::setSlotState(const std::int32_t slot, const bool enabled) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr || !openBlock.has_value()) {
        return;
    }
    const std::optional<std::int32_t> stateId = world->state().blockState(openBlock->x, openBlock->y, openBlock->z);
    const Identifier* block =
        stateId.has_value() ? server::registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
    // parity: the packet reaches `CrafterBlockEntity` and nothing else, so a window over anything
    // else says nothing.
    if (block == nullptr || *block != world::crafters::block()) {
        return;
    }
    if (!world::crafters::setSlotState(world->state(), openBlock->x, openBlock->y, openBlock->z, slot, enabled)) {
        return;
    }
    // parity: `setChanged` is what tells the comparator, and the window draws the cross from its own
    // data slot.
    world::redstoneComponents::scheduleComparatorsAround(world->state(), world->name(), openBlock->x, openBlock->y,
                                                         openBlock->z, world::comparatorReading::of(world->state()));
    const std::int32_t containerId = openMenu.containerId();
    using wire::protocol::play::ContainerSetDataPacket;
    owner_->sendCompressed(ContainerSetDataPacket::kId, [containerId, slot, enabled](wire::io::ByteBuffer& buffer) {
        ContainerSetDataPacket{.containerId = containerId,
                               .id = static_cast<std::int16_t>(slot),
                               .value = static_cast<std::int16_t>(enabled ? 0 : 1)}
            .write(buffer);
    });
}

void ContainerMenuController::closeOpenMenu(PlayerSession& session) {
    if (openMenu.containerId() == 0) {
        // Their own inventory closing still has a grid to empty.
        returnCraftingGrid(session);
        sendInventory();
        return;
    }
    returnCraftingGrid(session);
    owner_->trades().menuClosed();
    openMerchant_ = false;
    const std::optional<BlockPos> closing = openedAt_;
    std::optional<BlockPos> closingSecond;
    if (openBlockSecond.has_value()) {
        closingSecond = openBlock == closing ? openBlockSecond : openBlock;
    }
    std::optional<std::string> closingBlock;
    if (closing.has_value() && owner_->currentWorld() != nullptr) {
        const std::optional<std::int32_t> stateId =
            owner_->currentWorld()->state().blockState(closing->x, closing->y, closing->z);
        const wire::common::Identifier* block =
            stateId.has_value() ? server::registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
        if (block != nullptr && world::containers::isPlain(block->path())) {
            closingBlock = std::string{block->path()};
        }
    }
    openMenu = Menu::playerInventory();
    openBlock.reset();
    openBlockSecond.reset();
    openEnderChest_ = false;
    openEntity_.reset();
    pluginMenu_.reset();
    pluginMenuClick_ = nullptr;
    // parity: `SmithingMenu.removed`, which hands the three inputs back.
    if (openSmithing_) {
        for (const std::int32_t slot : {Menu::kSmithingTemplate, Menu::kSmithingBase, Menu::kSmithingAddition}) {
            const std::optional<storage::StoredItemStack>& leftover = smithingSlots_[static_cast<std::size_t>(slot)];
            if (!leftover.has_value()) {
                continue;
            }
            const std::int32_t left = session.inventory.add(*leftover);
            if (left > 0) {
                storage::StoredItemStack dropped = *leftover;
                dropped.count = left;
                dropStack(session, dropped);
            }
        }
        std::ranges::fill(smithingSlots_, std::nullopt);
        openSmithing_ = false;
    }
    // parity: `BeaconMenu.removed`, which drops the payment on the floor rather than putting it
    // back into the player.
    if (openBeacon_) {
        if (beaconPayment_.has_value()) {
            dropStack(session, *beaconPayment_);
            beaconPayment_.reset();
        }
        openBeacon_ = false;
    }
    // parity: `CartographyTableMenu.removed`: the result goes, the two inputs come back.
    if (openCartography_) {
        for (const std::int32_t slot : {Menu::kCartographyMap, Menu::kCartographyAdditional}) {
            const std::optional<storage::StoredItemStack>& leftover = cartographySlots_[static_cast<std::size_t>(slot)];
            if (!leftover.has_value()) {
                continue;
            }
            const std::int32_t left = session.inventory.add(*leftover);
            if (left > 0) {
                storage::StoredItemStack dropped = *leftover;
                dropped.count = left;
                dropStack(session, dropped);
            }
        }
        std::ranges::fill(cartographySlots_, std::nullopt);
        openCartography_ = false;
    }
    // parity: `LoomMenu.removed`, which hands its three inputs back.
    if (openLoom_) {
        for (const std::int32_t slot : {Menu::kLoomBanner, Menu::kLoomDye, Menu::kLoomPattern}) {
            const std::optional<storage::StoredItemStack>& leftover = loomSlots_[static_cast<std::size_t>(slot)];
            if (!leftover.has_value()) {
                continue;
            }
            const std::int32_t left = session.inventory.add(*leftover);
            if (left > 0) {
                storage::StoredItemStack dropped = *leftover;
                dropped.count = left;
                dropStack(session, dropped);
            }
        }
        std::ranges::fill(loomSlots_, std::nullopt);
        loomPatterns_.clear();
        loomPattern_ = -1;
        openLoom_ = false;
    }
    // parity: `EnchantmentMenu.removed`, which hands both slots back.
    if (openEnchantment_) {
        for (std::optional<storage::StoredItemStack>& leftover : enchantmentSlots_) {
            if (!leftover.has_value()) {
                continue;
            }
            const std::int32_t left = session.inventory.add(*leftover);
            if (left > 0) {
                storage::StoredItemStack dropped = *leftover;
                dropped.count = left;
                dropStack(session, dropped);
            }
            leftover.reset();
        }
        enchantmentCosts_ = {0, 0, 0};
        openEnchantment_ = false;
    }
    // parity: `GrindstoneMenu.removed`, which hands the two inputs back.
    if (openGrindstone_) {
        for (const std::int32_t slot : {Menu::kGrindstoneInput, Menu::kGrindstoneAddition}) {
            const std::optional<storage::StoredItemStack>& leftover = grindstoneSlots_[static_cast<std::size_t>(slot)];
            if (!leftover.has_value()) {
                continue;
            }
            const std::int32_t left = session.inventory.add(*leftover);
            if (left > 0) {
                storage::StoredItemStack dropped = *leftover;
                dropped.count = left;
                dropStack(session, dropped);
            }
        }
        std::ranges::fill(grindstoneSlots_, std::nullopt);
        grindstoneWorth_ = 0;
        openGrindstone_ = false;
    }
    // parity: `ItemCombinerMenu.removed`, which hands the two inputs back.
    if (openAnvil_) {
        for (const std::int32_t slot : {Menu::kAnvilInput, Menu::kAnvilAddition}) {
            const std::optional<storage::StoredItemStack>& leftover = anvilSlots_[static_cast<std::size_t>(slot)];
            if (!leftover.has_value()) {
                continue;
            }
            const std::int32_t left = session.inventory.add(*leftover);
            if (left > 0) {
                storage::StoredItemStack dropped = *leftover;
                dropped.count = left;
                dropStack(session, dropped);
            }
        }
        std::ranges::fill(anvilSlots_, std::nullopt);
        anvilName_.reset();
        anvilCost_ = 0;
        openAnvil_ = false;
    }
    // parity: `StonecutterMenu.removed`, which hands whatever was in the input back.
    if (openStonecutter_) {
        if (stonecutterInput_.has_value()) {
            const std::int32_t left = session.inventory.add(*stonecutterInput_);
            if (left > 0) {
                storage::StoredItemStack dropped = *stonecutterInput_;
                dropped.count = left;
                dropStack(session, dropped);
            }
        }
        stonecutterInput_.reset();
        stonecutterResult_.reset();
        openStonecutter_ = false;
    }
    if (closing.has_value() && closingBlock.has_value()) {
        announceOpeners(*closing, *closingBlock,
                        world::containerOpeners::close(session.worldName, closing->x, closing->y, closing->z));
        if (closingSecond.has_value()) {
            announceOpeners(*closingSecond, *closingBlock,
                            world::containerOpeners::close(session.worldName, closingSecond->x, closingSecond->y,
                                                           closingSecond->z));
        }
    }
    openedAt_.reset();
    recomputeCraftingResult();
    sendInventory();
}

void ContainerMenuController::clickContainerButton(std::int32_t button) {
    PlayerSession* session = owner_->playerSession();
    world::LoadedWorld* world = owner_->currentWorld();
    if (session == nullptr || world == nullptr) {
        return;
    }
    // parity: `StonecutterMenu.clickMenuButton`, whose button is an index into the recipes that fit
    // whatever is in the input slot, in the order the client was sent them.
    if (openStonecutter_) {
        selectStonecutterRecipe(button);
        return;
    }
    // parity: `LoomMenu.clickMenuButton`, whose button is a place in the patterns on offer.
    if (openLoom_) {
        selectLoomPattern(button);
        return;
    }
    // parity: `EnchantmentMenu.clickMenuButton`, whose button is which of the three rows was pressed.
    if (openEnchantment_) {
        takeEnchantmentOffer(*session, button);
        return;
    }
    if (!openLecternAt.has_value()) {
        return;
    }
    const BlockPos pos = *openLecternAt;
    using wire::protocol::play::ContainerButtonClickPacket;
    std::int32_t wanted = 0;
    if (button >= ContainerButtonClickPacket::kLecternJumpToPage) {
        wanted = button - ContainerButtonClickPacket::kLecternJumpToPage;
    } else if (button == ContainerButtonClickPacket::kLecternPreviousPage) {
        wanted = lecternPage_ - 1;
    } else if (button == ContainerButtonClickPacket::kLecternNextPage) {
        wanted = lecternPage_ + 1;
    } else if (button == ContainerButtonClickPacket::kLecternTakeBook) {
        const std::optional<std::pair<world::BlockChange, server::world::loot::ItemStack>> taken =
            world::lecterns::takeBook(world->state(), pos.x, pos.y, pos.z);
        if (!taken.has_value()) {
            return;
        }
        owner_->blockInteraction().broadcastBlockUpdate(BlockPos{taken->first.x, taken->first.y, taken->first.z},
                                                        taken->first.stateId);
        const std::int32_t left = session->inventory.add(storage::StoredItemStack{
            .id = taken->second.item.toString(), .count = 1, .components = taken->second.components});
        if (left > 0) {
            owner_->spawnDroppedItem(taken->second.item, left, session->x, session->y, session->z);
        }
        sendInventory();
        owner_->blockInteraction().notifyNeighbours(pos);
        openLecternAt.reset();
        return;
    } else {
        return;
    }
    if (wanted < 0) {
        return;
    }
    lecternPage_ = wanted;
    const std::optional<world::BlockChange> change =
        world::lecterns::turnPage(world->state(), pos.x, pos.y, pos.z, wanted);
    if (!change.has_value()) {
        return;
    }
    owner_->blockInteraction().broadcastBlockUpdate(BlockPos{change->x, change->y, change->z}, change->stateId);
    owner_->blockInteraction().notifyNeighbours(pos);
}

} // namespace basalt::server::runtime
