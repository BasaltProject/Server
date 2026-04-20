#include <basalt/server/runtime/BlockInteractionHandler.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/EnchantmentBooks.hpp>
#include <basalt/server/runtime/world/Grindstones.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime {
namespace {

using player::Menu;
using player::PlayerSession;
using wire::protocol::play::BlockPos;
using wire::protocol::play::OpenScreenPacket;
using wire::protocol::play::SoundSource;
using wire::text::Component;

} // namespace

void ContainerMenuController::openGrindstoneMenu(PlayerSession& session, const BlockPos& pos) {
    returnCraftingGrid(session);
    openMenu = Menu::grindstone(allocateContainerId());
    openBlock = pos;
    openBlockSecond.reset();
    openEnderChest_ = false;
    openEntity_.reset();
    openStonecutter_ = false;
    openSmithing_ = false;
    openAnvil_ = false;
    openGrindstone_ = true;
    std::ranges::fill(grindstoneSlots_, std::nullopt);
    grindstoneWorth_ = 0;
    openBlockSize = static_cast<std::int32_t>(grindstoneSlots_.size());
    openBlockData_ = {-1, -1, -1, -1};
    owner_->sendCompressed(OpenScreenPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        OpenScreenPacket{.containerId = openMenu.containerId(),
                         .menuType = wire::protocol::play::menuType::kGrindstone,
                         .title = Component::translatable("container.grindstone_title")}
            .write(buffer);
    });
    sendInventory();
}

void ContainerMenuController::recomputeGrindstoneResult() {
    const world::EnchantmentBooks books{.enchantments = owner_->options().enchantments,
                                        .itemTags = owner_->options().itemTags,
                                        .enchantmentTags = owner_->options().enchantmentTags};
    const std::optional<storage::StoredItemStack>& input = grindstoneSlots_[Menu::kGrindstoneInput];
    const std::optional<storage::StoredItemStack>& addition = grindstoneSlots_[Menu::kGrindstoneAddition];
    const world::grindstones::Result made = world::grindstones::combine(
        input.has_value() ? &*input : nullptr, addition.has_value() ? &*addition : nullptr,
        [&books](const wire::common::Identifier& id) { return books.isCurse(id); },
        [&books](const wire::common::Identifier& id, const std::int32_t level) { return books.minCost(id, level); });
    grindstoneSlots_[Menu::kGrindstoneResult] = made.output;
    grindstoneWorth_ = made.experience;
}

void ContainerMenuController::takeGrindstoneResult(PlayerSession& session) {
    // parity: `GrindstoneMenu.ResultSlot.onTake`.
    // block as orbs and the stone grinds. Whoever took it gets nothing directly.
    static_cast<void>(session);
    const std::int32_t half = (grindstoneWorth_ + 1) / 2;
    const double roll = options_.nextDouble ? options_.nextDouble() : 0.0;
    const std::int32_t points = world::grindstones::experienceFor(
        grindstoneWorth_, static_cast<std::int32_t>(roll * static_cast<double>(half)));
    world::LoadedWorld* world = owner_->currentWorld();
    if (points > 0 && openBlock.has_value() && world != nullptr && owner_->options().awardExperience) {
        owner_->options().awardExperience(*world, openBlock->x + 0.5, openBlock->y + 0.5, openBlock->z + 0.5, points);
    }
    grindstoneSlots_[Menu::kGrindstoneInput].reset();
    grindstoneSlots_[Menu::kGrindstoneAddition].reset();
    grindstoneWorth_ = 0;
    if (world != nullptr && openBlock.has_value()) {
        owner_->blockInteraction().broadcastEffects(
            *openBlock, world->state().blockState(openBlock->x, openBlock->y, openBlock->z).value_or(0),
            {world::blockEffect::Sound{
                .name = "block.grindstone.use", .source = SoundSource::Blocks, .volume = 1.0F, .pitch = 1.0F}});
    }
    recomputeGrindstoneResult();
}

} // namespace basalt::server::runtime
