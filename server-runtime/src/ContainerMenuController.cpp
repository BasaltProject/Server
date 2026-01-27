#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockInteractionHandler.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/UiStateSync.hpp>
#include <basalt/server/runtime/item/ItemComponentCodecs.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/ContainerOpeners.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/DoubleChests.hpp>
#include <basalt/server/runtime/world/Lecterns.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>
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
    openBlockSecond = halves.has_value()
                          ? std::optional{BlockPos{halves->second[0], halves->second[1], halves->second[2]}}
                          : std::nullopt;
    openBlock = halves.has_value() ? BlockPos{halves->first[0], halves->first[1], halves->first[2]} : pos;
    const bool paired = openBlockSecond.has_value();
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

void ContainerMenuController::announceOpeners(const BlockPos& pos, const std::string& block, std::int32_t openers) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return;
    }
    const std::optional<std::int32_t> stateId = world->state().blockState(pos.x, pos.y, pos.z);
    if (!stateId.has_value()) {
        return;
    }
    if (block == "barrel") {
        const std::optional<std::int32_t> opened = server::registry::blockStateDefinitions::stateIdOf(
            Identifier::vanilla("barrel"), {{"open", openers > 0 ? "true" : "false"}}, *stateId);
        if (opened.has_value() && *opened != *stateId && world->state().setBlockState(pos.x, pos.y, pos.z, *opened)) {
            owner_->blockInteraction().broadcastBlockUpdate(pos, *opened);
        }
    } else {
        const std::int32_t blockId = server::registry::blockSupport::blockRegistryIdOf(*stateId);
        for (PlayerSession* player : owner_->playersInSameWorld()) {
            if (!player->connection().sendPlayPacket) {
                continue;
            }
            player->connection().sendPlayPacket(
                wire::protocol::play::BlockEventPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                    wire::protocol::play::BlockEventPacket{
                        .pos = pos, .b0 = 1, .b1 = static_cast<std::uint8_t>(openers), .blockId = blockId}
                        .write(buffer);
                });
        }
    }
    // Only the first in and the last out make a sound, which is why a busy chest is not a rattle.
    const std::string family = block == "barrel"                      ? "barrel"
                               : world::shulkers::isShulkerBox(block) ? "shulker_box"
                                                                      : "chest";
    std::string sound;
    if (openers == 1) {
        sound = "block." + family + ".open";
    } else if (openers == 0) {
        sound = "block." + family + ".close";
    } else {
        return;
    }
    // parity: `ContainerOpenersCounter` raises `CONTAINER_OPEN` on the first one in and
    // `CONTAINER_CLOSE` on the last one out, on the same edges as the sound. A sculk sensor beside a
    // chest hears somebody open it, and here it heard nothing at all.
    if (world->state().hasSculkSensorNear(pos.x, pos.y, pos.z, world::vibrations::kRange)) {
        world::vibrations::emit(world->state(), openers == 1 ? "container_open" : "container_close", pos.x + 0.5,
                                pos.y + 0.5, pos.z + 0.5, world->gameTime());
    }
    const float roll = options_.nextFloat ? options_.nextFloat() : 0.5F;
    for (PlayerSession* player : owner_->playersInSameWorld()) {
        if (!player->connection().sendPlayPacket) {
            continue;
        }
        player->connection().sendPlayPacket(wire::protocol::play::SoundPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            wire::protocol::play::SoundPacket{.sound = sound,
                                              .source = SoundSource::Blocks,
                                              .x = pos.x + 0.5,
                                              .y = pos.y + 0.5,
                                              .z = pos.z + 0.5,
                                              .volume = 0.5F,
                                              // parity: `ChestBlockEntity.playSound`, a tenth
                                              // of a tone of randomness.
                                              .pitch = (roll * 0.1F) + 0.9F,
                                              .seed = options_.nextSeed ? options_.nextSeed() : 0}
                .write(buffer);
        });
    }
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

void ContainerMenuController::openSmithingMenu(PlayerSession& session) {
    returnCraftingGrid(session);
    openMenu = Menu::smithing(allocateContainerId());
    openBlock.reset();
    openBlockSecond.reset();
    openEnderChest_ = false;
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

void ContainerMenuController::closeOpenMenu(PlayerSession& session) {
    if (openMenu.containerId() == 0) {
        // Their own inventory closing still has a grid to empty.
        returnCraftingGrid(session);
        sendInventory();
        return;
    }
    returnCraftingGrid(session);
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
        const std::optional<std::pair<world::BlockChange, Identifier>> taken =
            world::lecterns::takeBook(world->state(), pos.x, pos.y, pos.z);
        if (!taken.has_value()) {
            return;
        }
        owner_->blockInteraction().broadcastBlockUpdate(BlockPos{taken->first.x, taken->first.y, taken->first.z},
                                                        taken->first.stateId);
        const std::int32_t left =
            session->inventory.add(storage::StoredItemStack{.id = taken->second.toString(), .count = 1});
        if (left > 0) {
            owner_->spawnDroppedItem(taken->second, left, session->x, session->y, session->z);
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
