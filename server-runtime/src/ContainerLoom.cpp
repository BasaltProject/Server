#include <basalt/server/runtime/BlockInteractionHandler.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Looms.hpp>
#include <basalt/server/runtime/world/RealBannerPatterns.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime {
namespace {

using player::Menu;
using player::PlayerSession;
using wire::protocol::play::BlockPos;
using wire::protocol::play::ContainerSetDataPacket;
using wire::protocol::play::OpenScreenPacket;
using wire::protocol::play::SoundSource;
using wire::text::Component;

/// parity: `BannerPatternTags.NO_ITEM_REQUIRED`.
constexpr std::string_view kFreePatterns = "minecraft:no_item_required";

} // namespace

void ContainerMenuController::openLoomMenu(PlayerSession& session, const BlockPos& pos) {
    if (!mayOpen("loom", pos)) {
        return;
    }
    returnCraftingGrid(session);
    openMenu = Menu::loom(allocateContainerId());
    openBlock = pos;
    openBlockSecond.reset();
    openEnderChest_ = false;
    openEntity_.reset();
    openStonecutter_ = false;
    openSmithing_ = false;
    openAnvil_ = false;
    openGrindstone_ = false;
    openEnchantment_ = false;
    openBeacon_ = false;
    openLoom_ = true;
    std::ranges::fill(loomSlots_, std::nullopt);
    loomPatterns_.clear();
    loomPattern_ = -1;
    openBlockSize = static_cast<std::int32_t>(loomSlots_.size());
    openBlockData_ = {-1, -1, -1, -1};
    owner_->sendCompressed(OpenScreenPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        OpenScreenPacket{.containerId = openMenu.containerId(),
                         .menuType = wire::protocol::play::menuType::kLoom,
                         .title = Component::translatable("container.loom")}
            .write(buffer);
    });
    sendInventory();
    sendLoomNumbers();
}

void ContainerMenuController::recomputeLoomResult() {
    const std::optional<storage::StoredItemStack>& banner = loomSlots_[Menu::kLoomBanner];
    const std::optional<storage::StoredItemStack>& dye = loomSlots_[Menu::kLoomDye];
    const std::optional<storage::StoredItemStack>& pattern = loomSlots_[Menu::kLoomPattern];
    loomSlots_[Menu::kLoomResult].reset();
    // parity: `slotsChanged` clears everything until both a banner and a dye are in it.
    if (!banner.has_value() || !dye.has_value()) {
        loomPatterns_.clear();
        loomPattern_ = -1;
        sendLoomNumbers();
        return;
    }
    const world::realBannerPatterns::TagOrder* tags = owner_->options().bannerPatterns;
    const std::string tag = pattern.has_value() ? world::looms::patternTagOf(pattern->id).value_or(std::string{})
                                                : std::string{kFreePatterns};
    std::vector<std::string> offered;
    if (tags != nullptr && !tag.empty()) {
        for (const wire::common::Identifier& one : world::realBannerPatterns::membersOf(*tags, tag)) {
            offered.push_back(one.toString());
        }
    }
    // parity: the pick follows the pattern it was on rather than its place, and a list of one picks.
    // itself.
    const std::string was = loomPattern_ >= 0 && static_cast<std::size_t>(loomPattern_) < loomPatterns_.size()
                                ? loomPatterns_[static_cast<std::size_t>(loomPattern_)]
                                : std::string{};
    loomPatterns_ = std::move(offered);
    if (loomPatterns_.size() == 1) {
        loomPattern_ = 0;
    } else if (was.empty()) {
        loomPattern_ = -1;
    } else {
        const auto found = std::ranges::find(loomPatterns_, was);
        loomPattern_ = found == loomPatterns_.end() ? -1 : static_cast<std::int32_t>(found - loomPatterns_.begin());
    }
    if (loomPattern_ >= 0) {
        const std::optional<std::string> colour = world::looms::dyeColourOf(dye->id);
        std::optional<storage::StoredItemStack> made =
            colour.has_value()
                ? world::looms::weave(*banner, loomPatterns_[static_cast<std::size_t>(loomPattern_)], *colour)
                : std::nullopt;
        if (!made.has_value()) {
            // parity: a banner already carrying six layers loses the pick as well as the result.
            loomPattern_ = -1;
        }
        loomSlots_[Menu::kLoomResult] = std::move(made);
    }
    sendLoomNumbers();
}

void ContainerMenuController::selectLoomPattern(const std::int32_t button) {
    if (button < 0 || static_cast<std::size_t>(button) >= loomPatterns_.size()) {
        return;
    }
    loomPattern_ = button;
    const std::optional<storage::StoredItemStack>& banner = loomSlots_[Menu::kLoomBanner];
    const std::optional<storage::StoredItemStack>& dye = loomSlots_[Menu::kLoomDye];
    if (!banner.has_value() || !dye.has_value()) {
        return;
    }
    const std::optional<std::string> colour = world::looms::dyeColourOf(dye->id);
    loomSlots_[Menu::kLoomResult] =
        colour.has_value() ? world::looms::weave(*banner, loomPatterns_[static_cast<std::size_t>(button)], *colour)
                           : std::nullopt;
    sendLoomNumbers();
    sendInventory();
}

void ContainerMenuController::takeLoomResult() {
    // parity: `LoomMenu.ResultSlot.onTake`.
    // the loom is heard once per turn.
    const auto eat = [this](const std::int32_t slot) {
        std::optional<storage::StoredItemStack>& held = loomSlots_[static_cast<std::size_t>(slot)];
        if (held.has_value() && --held->count <= 0) {
            held.reset();
        }
    };
    eat(Menu::kLoomBanner);
    eat(Menu::kLoomDye);
    if (!loomSlots_[Menu::kLoomBanner].has_value() || !loomSlots_[Menu::kLoomDye].has_value()) {
        loomPattern_ = -1;
    }
    world::LoadedWorld* world = owner_->currentWorld();
    if (world != nullptr && openBlock.has_value()) {
        owner_->blockInteraction().broadcastEffects(
            *openBlock, world->state().blockState(openBlock->x, openBlock->y, openBlock->z).value_or(0),
            {world::blockEffect::Sound{
                .name = "ui.loom.take_result", .source = SoundSource::Blocks, .volume = 1.0F, .pitch = 1.0F}});
    }
    recomputeLoomResult();
}

void ContainerMenuController::sendLoomNumbers() {
    if (!openLoom_) {
        return;
    }
    owner_->sendPlayPacket(ContainerSetDataPacket::kId, [this](wire::io::ByteBuffer& buffer) {
        ContainerSetDataPacket{.containerId = openMenu.containerId(),
                               .id = ContainerSetDataPacket::kLoomPattern,
                               .value = static_cast<std::int16_t>(loomPattern_)}
            .write(buffer);
    });
}

} // namespace basalt::server::runtime
