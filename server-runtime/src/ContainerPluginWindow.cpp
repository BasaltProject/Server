#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/plugins/PluginItemConversion.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/Furnaces.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

#include <algorithm>
#include <array>
#include <cstdint>

namespace basalt::server::runtime {
namespace {

using player::Menu;
using player::MenuSlot;
using player::PlayerSession;
using server::plugins::PluginItem;
using server::plugins::PluginWindowInfo;
using server::plugins::PluginWindowSpec;

struct WindowKind {
    std::string_view name;
    std::int32_t menuType = 0;
    std::int32_t size = 0;
};

// Menu type ids are the registry order of `minecraft:menu`.
constexpr std::array<WindowKind, 21> kKinds{{{"dispenser", 6, 9},
                                             {"dropper", 6, 9},
                                             {"crafter", 7, 9},
                                             {"anvil", 8, 3},
                                             {"beacon", 9, 1},
                                             {"blast_furnace", 10, 3},
                                             {"brewing_stand", 11, 5},
                                             {"crafting", 12, 10},
                                             {"enchanting", 13, 2},
                                             {"furnace", 14, 3},
                                             {"grindstone", 15, 3},
                                             {"hopper", 16, 5},
                                             {"lectern", 17, 1},
                                             {"loom", 18, 4},
                                             {"merchant", 19, 3},
                                             {"shulker_box", 20, 27},
                                             {"smithing", 21, 4},
                                             {"smoker", 22, 3},
                                             {"cartography_table", 23, 3},
                                             {"stonecutter", 24, 2},
                                             {"barrel", 2, 27}}};

} // namespace

std::int32_t ContainerMenuController::openWindow(const PluginWindowSpec& spec) {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return 0;
    }
    std::int32_t menuType = 0;
    std::int32_t size = 0;
    if (spec.kind == "chest" || spec.kind == "ender_chest") {
        const std::int32_t rows = std::clamp((spec.size + Menu::kColumns - 1) / Menu::kColumns, 1, 6);
        menuType = rows - 1;
        size = rows * Menu::kColumns;
    } else {
        const auto found = std::ranges::find(kKinds, spec.kind, &WindowKind::name);
        if (found == kKinds.end()) {
            return 0;
        }
        menuType = found->menuType;
        size = found->size;
    }
    if (openMenu.containerId() != 0) {
        closeOpenMenu(*session, "open_new");
    }
    returnCraftingGrid(*session);
    openMenu = Menu::blocks(allocateContainerId(), size);
    openBlock.reset();
    openBlockSecond.reset();
    openEnderChest_ = false;
    openEntity_.reset();
    openStonecutter_ = false;
    openSmithing_ = false;
    openBlockSize = size;
    openBlockData_ = {-1, -1, -1, -1};
    pluginMenu_ = std::vector<std::optional<storage::StoredItemStack>>(static_cast<std::size_t>(size));
    for (std::size_t slot = 0; slot < spec.items.size() && slot < pluginMenu_->size(); ++slot) {
        if (spec.items[slot].has_value()) {
            (*pluginMenu_)[slot] = plugins::storedItemOf(*spec.items[slot]);
        }
    }
    pluginMenuClick_ = nullptr;
    pluginKind_ = spec.kind;
    pluginEditable_ = spec.editable;
    owner_->sendCompressed(wire::protocol::play::OpenScreenPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        wire::protocol::play::OpenScreenPacket{.containerId = openMenu.containerId(),
                                               .menuType = menuType,
                                               .title = wire::text::componentNbt::deserialize(spec.title)}
            .write(buffer);
    });
    sendInventory();
    return openMenu.containerId();
}

std::int32_t ContainerMenuController::openBlockWindow(const std::int32_t x, const std::int32_t y,
                                                      const std::int32_t z) {
    PlayerSession* session = owner_->playerSession();
    world::LoadedWorld* world = owner_->currentWorld();
    if (session == nullptr || world == nullptr) {
        return 0;
    }
    const std::optional<std::int32_t> stateId = world->state().blockState(x, y, z);
    const wire::common::Identifier* block =
        stateId.has_value() ? registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
    if (block == nullptr) {
        return 0;
    }
    const std::string path{block->path()};
    const wire::protocol::play::BlockPos pos{.x = x, .y = y, .z = z};
    const std::int32_t before = nextContainerId;
    if (path == "crafting_table") {
        openCraftingTable(*session);
    } else if (world::furnaces::cookerFor(path).has_value()) {
        openFurnace(*session, pos, path);
    } else if (path == "brewing_stand") {
        openBrewingStand(*session, pos);
    } else if (world::containers::isPlain(path)) {
        openChest(*session, pos, path);
    } else if (path == "crafter") {
        openCrafter(*session, pos);
    } else if (path == "ender_chest") {
        openEnderChest(*session, pos);
    } else {
        return 0;
    }
    return nextContainerId != before ? openMenu.containerId() : 0;
}

std::int32_t ContainerMenuController::openEntityWindow(const std::int32_t entityId) {
    PlayerSession* session = owner_->playerSession();
    world::LoadedWorld* world = owner_->currentWorld();
    entity::ServerEntity* vehicle = world == nullptr ? nullptr : world->state().entity(entityId);
    if (session == nullptr || vehicle == nullptr) {
        return 0;
    }
    const std::string_view path = vehicle->type().path();
    if (path != "chest_minecart" && !path.ends_with("_chest_boat") && !path.ends_with("_chest_raft")) {
        return 0;
    }
    const std::int32_t before = nextContainerId;
    openEntityChest(*session, *vehicle);
    return nextContainerId != before ? openMenu.containerId() : 0;
}

void ContainerMenuController::closeWindow() { closeOpenMenu(); }

void ContainerMenuController::announceResult(std::string_view kind, std::optional<storage::StoredItemStack>& result,
                                             std::int32_t& cost) {
    if (!hooks_.prepared) {
        return;
    }
    const std::string recipe = recipeUsed_.has_value() ? recipeUsed_->toString() : std::string{};
    hooks_.prepared(kind, recipe, openAnvil_ ? anvilName_.value_or("") : "", result, cost);
}

bool ContainerMenuController::resultMayBeTaken(const MenuSlot& slot, bool shift) const {
    if (!hooks_.takeAllowed) {
        return true;
    }
    const PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return true;
    }
    std::string_view kind;
    std::optional<storage::StoredItemStack> result;
    if (std::holds_alternative<player::menuSlot::Result>(slot)) {
        kind = "crafting";
        result = craftingResult_;
    } else if (const auto* block = std::get_if<player::menuSlot::Block>(&slot); block != nullptr) {
        if (openAnvil_ && block->index == Menu::kAnvilResult) {
            kind = "anvil";
            result = anvilSlots_[Menu::kAnvilResult];
        } else if (openSmithing_ && block->index == Menu::kSmithingResult) {
            kind = "smithing";
            result = smithingSlots_[Menu::kSmithingResult];
        } else if (openGrindstone_ && block->index == Menu::kGrindstoneResult) {
            kind = "grindstone";
            result = grindstoneSlots_[Menu::kGrindstoneResult];
        } else if (openStonecutter_ && block->index == Menu::kStonecutterResult) {
            kind = "stonecutter";
            result = stonecutterResult_;
        }
    }
    if (kind.empty() || !result.has_value()) {
        return true;
    }
    return hooks_.takeAllowed(kind, recipeUsed_.has_value() ? recipeUsed_->toString() : std::string{}, *result, shift);
}

std::string ContainerMenuController::windowKind() const {
    if (pluginMenu_.has_value()) {
        return pluginKind_;
    }
    if (openMenu.containerId() == 0) {
        return "player";
    }
    if (openMerchant_) {
        return "merchant";
    }
    if (openCartography_) {
        return "cartography_table";
    }
    if (openBeacon_) {
        return "beacon";
    }
    if (openLoom_) {
        return "loom";
    }
    if (openEnchantment_) {
        return "enchanting";
    }
    if (openGrindstone_) {
        return "grindstone";
    }
    if (openAnvil_) {
        return "anvil";
    }
    if (openSmithing_) {
        return "smithing";
    }
    if (openStonecutter_) {
        return "stonecutter";
    }
    if (openEnderChest_) {
        return "ender_chest";
    }
    if (openEntity_.has_value()) {
        return openMount_ ? "mount" : "chest";
    }
    if (openMenu.gridWidth() == 3) {
        return "crafting";
    }
    if (openBlock.has_value()) {
        world::LoadedWorld* world = owner_->currentWorld();
        const std::optional<std::int32_t> state =
            world == nullptr ? std::nullopt : world->state().blockState(openBlock->x, openBlock->y, openBlock->z);
        const wire::common::Identifier* block =
            state.has_value() ? server::registry::blockStateDefinitions::blockOf(*state) : nullptr;
        if (block != nullptr) {
            const std::string_view path = block->path();
            if (path.ends_with("shulker_box")) {
                return "shulker_box";
            }
            return std::string{path};
        }
    }
    return "chest";
}

PluginWindowInfo ContainerMenuController::windowInfo() const {
    PluginWindowInfo info{.containerId = openMenu.containerId(),
                          .kind = windowKind(),
                          .slots = openMenu.slotCount(),
                          .plugin = pluginMenu_.has_value()};
    if (openBlock.has_value() && !pluginMenu_.has_value()) {
        info.block = std::array<std::int32_t, 3>{openBlock->x, openBlock->y, openBlock->z};
        if (const player::PlayerSession* session = owner_->playerSession(); session != nullptr) {
            info.world = session->worldName;
        }
    }
    return info;
}

bool ContainerMenuController::setWindowProperty(std::int32_t id, std::int32_t value) {
    if (owner_->playerSession() == nullptr || openMenu.containerId() == 0) {
        return false;
    }
    const wire::protocol::play::ContainerSetDataPacket packet{.containerId = openMenu.containerId(),
                                                              .id = static_cast<std::int16_t>(id),
                                                              .value = static_cast<std::int16_t>(value)};
    owner_->sendCompressed(wire::protocol::play::ContainerSetDataPacket::kId,
                           [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    return true;
}

std::optional<PluginItem> ContainerMenuController::windowItem(std::int32_t slot) const {
    const PlayerSession* session = owner_->playerSession();
    const std::optional<MenuSlot> found = openMenu.slotAt(slot);
    if (session == nullptr || !found.has_value()) {
        return std::nullopt;
    }
    const std::optional<storage::StoredItemStack> stack = readSlot(*session, *found);
    return stack.has_value() ? std::optional{plugins::pluginItemOf(*stack)} : std::nullopt;
}

void ContainerMenuController::setWindowItem(std::int32_t slot, const std::optional<PluginItem>& item) {
    PlayerSession* session = owner_->playerSession();
    const std::optional<MenuSlot> found = openMenu.slotAt(slot);
    if (session == nullptr || !found.has_value()) {
        return;
    }
    writeSlot(*session, *found, item.has_value() ? std::optional{plugins::storedItemOf(*item)} : std::nullopt);
    owner_->sendPlayPacket(wire::protocol::play::ContainerSetSlotPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        wire::protocol::play::ContainerSetSlotPacket{.containerId = openMenu.containerId(),
                                                     .stateId = ++containerStateId_,
                                                     .slot = static_cast<std::int16_t>(slot),
                                                     .item = session->inventory.toWireStack(readSlot(*session, *found))}
            .write(buffer);
    });
}

std::optional<PluginItem> ContainerMenuController::cursorItem() const {
    return carriedItem_.has_value() ? std::optional{plugins::pluginItemOf(*carriedItem_)} : std::nullopt;
}

void ContainerMenuController::setCursorItem(const std::optional<PluginItem>& item) {
    carriedItem_ = item.has_value() ? std::optional{plugins::storedItemOf(*item)} : std::nullopt;
    if (const PlayerSession* session = owner_->playerSession(); session != nullptr) {
        owner_->sendPlayPacket(wire::protocol::play::ContainerSetSlotPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            wire::protocol::play::ContainerSetSlotPacket{.containerId = -1,
                                                         .stateId = ++containerStateId_,
                                                         .slot = -1,
                                                         .item = session->inventory.toWireStack(carriedItem_)}
                .write(buffer);
        });
    }
}

} // namespace basalt::server::runtime
