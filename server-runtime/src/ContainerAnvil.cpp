#include <basalt/server/runtime/BlockInteractionHandler.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/player/PlayerAnvil.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/AnvilEnchantments.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <utility>

namespace basalt::server::runtime {
namespace {

using player::Menu;
using player::MenuSlot;
using player::PlayerSession;
using wire::protocol::play::BlockPos;
using wire::protocol::play::OpenScreenPacket;
using wire::text::Component;

} // namespace

void ContainerMenuController::openAnvilMenu(PlayerSession& session, const BlockPos& pos) {
    returnCraftingGrid(session);
    openMenu = Menu::anvil(allocateContainerId());
    openBlock = pos;
    openBlockSecond.reset();
    openEnderChest_ = false;
    openStonecutter_ = false;
    openSmithing_ = false;
    openAnvil_ = true;
    std::ranges::fill(anvilSlots_, std::nullopt);
    anvilName_.reset();
    anvilCost_ = 0;
    anvilMaterials_ = 0;
    anvilOnlyRenaming_ = false;
    openBlockSize = static_cast<std::int32_t>(anvilSlots_.size());
    openBlockData_ = {-1, -1, -1, -1};
    owner_->sendCompressed(OpenScreenPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        OpenScreenPacket{.containerId = openMenu.containerId(),
                         .menuType = wire::protocol::play::menuType::kAnvil,
                         .title = Component::translatable("container.repair")}
            .write(buffer);
    });
    sendInventory();
    sendAnvilCost();
}

void ContainerMenuController::setAnvilName(const std::string& name) {
    if (!openAnvil_ || name.size() > wire::protocol::play::ServerboundRenameItemPacket::kMaxLength) {
        return;
    }
    anvilName_ = name;
    recomputeAnvilResult();
    sendInventory();
}

void ContainerMenuController::recomputeAnvilResult() {
    const std::optional<storage::StoredItemStack>& input = anvilSlots_[Menu::kAnvilInput];
    const std::optional<storage::StoredItemStack>& addition = anvilSlots_[Menu::kAnvilAddition];
    const world::anvils::Result made =
        world::anvils::combine(input.has_value() ? &*input : nullptr, addition.has_value() ? &*addition : nullptr,
                               anvilName_, world::anvils::isRepairMaterialFor,
                               world::anvils::enchantmentMerge({.enchantments = owner_->options().enchantments,
                                                                .itemTags = owner_->options().itemTags,
                                                                .enchantmentTags = owner_->options().enchantmentTags}));
    anvilSlots_[Menu::kAnvilResult] = made.output;
    anvilCost_ = made.cost;
    anvilMaterials_ = made.materialCount;
    anvilOnlyRenaming_ = made.onlyRenaming;
    sendAnvilCost();
}

void ContainerMenuController::takeAnvilResult(PlayerSession& session) {
    // parity: `AnvilMenu.onTake`, minus the pickup check, which has already been made.
    const bool free = session.gamemode == wire::protocol::play::Gamemode::Creative;
    if (!free) {
        session.experience.giveLevels(-anvilCost_);
        if (session.connection().syncExperience) {
            session.connection().syncExperience();
        }
    }
    std::optional<storage::StoredItemStack>& addition = anvilSlots_[Menu::kAnvilAddition];
    if (anvilMaterials_ > 0) {
        if (addition.has_value() && addition->count > anvilMaterials_) {
            addition->count -= anvilMaterials_;
        } else {
            addition.reset();
        }
    } else if (!anvilOnlyRenaming_) {
        addition.reset();
    }
    anvilSlots_[Menu::kAnvilInput].reset();
    anvilCost_ = 0;
    if (!free) {
        wearAnvil();
    }
    recomputeAnvilResult();
}

void ContainerMenuController::wearAnvil() {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr || !openBlock.has_value()) {
        return;
    }
    const float roll = options_.nextFloat ? options_.nextFloat() : 1.0F;
    if (player::wearAnvilAt(owner_->blockInteraction().breakContext(), *world, openBlock->x, openBlock->y, openBlock->z,
                            roll)) {
        closeOpenMenu();
    }
}

void ContainerMenuController::sendAnvilCost() {
    if (!openAnvil_) {
        return;
    }
    owner_->sendPlayPacket(wire::protocol::play::ContainerSetDataPacket::kId, [this](wire::io::ByteBuffer& buffer) {
        wire::protocol::play::ContainerSetDataPacket{.containerId = openMenu.containerId(),
                                                     .id = wire::protocol::play::ContainerSetDataPacket::kAnvilCost,
                                                     .value =
                                                         static_cast<std::int16_t>(std::clamp(anvilCost_, 0, 0x7fff))}
            .write(buffer);
    });
}

bool ContainerMenuController::mayPickup(const MenuSlot& slot) const {
    if (!openAnvil_) {
        return true;
    }
    const auto* block = std::get_if<player::menuSlot::Block>(&slot);
    if (block == nullptr || block->index != Menu::kAnvilResult) {
        return true;
    }
    // parity: `AnvilMenu.mayPickup`: the levels have to be there, and there has to be a price.
    const PlayerSession* session = owner_->playerSession();
    if (session == nullptr || anvilCost_ <= 0) {
        return false;
    }
    return session->gamemode == wire::protocol::play::Gamemode::Creative || session->experience.level() >= anvilCost_;
}

} // namespace basalt::server::runtime
