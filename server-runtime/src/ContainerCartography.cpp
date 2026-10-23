#include <basalt/server/runtime/BlockInteractionHandler.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/MapSystem.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/maps/Cartography.hpp>

#include <algorithm>
#include <cstdint>

namespace basalt::server::runtime {
namespace {

using player::Menu;
using player::PlayerSession;
using wire::protocol::play::BlockPos;
using wire::protocol::play::OpenScreenPacket;
using wire::protocol::play::SoundSource;

/// parity: `MenuType.CARTOGRAPHY_TABLE`'s registry id.
constexpr std::int32_t kCartographyMenu = 23;

} // namespace

void ContainerMenuController::openCartographyTable(PlayerSession& session, const BlockPos& pos) {
    if (!mayOpen("cartography_table", pos)) {
        return;
    }
    returnCraftingGrid(session);
    openMenu = Menu::cartographyTable(allocateContainerId());
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
    openLoom_ = false;
    openCartography_ = true;
    std::ranges::fill(cartographySlots_, std::nullopt);
    openBlockSize = static_cast<std::int32_t>(cartographySlots_.size());
    openBlockData_ = {-1, -1, -1, -1};
    owner_->sendCompressed(OpenScreenPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        OpenScreenPacket{.containerId = openMenu.containerId(),
                         .menuType = kCartographyMenu,
                         .title = wire::text::Component::translatable("container.cartography_table")}
            .write(buffer);
    });
    sendInventory();
}

void ContainerMenuController::recomputeCartographyResult() {
    std::optional<storage::StoredItemStack>& result = cartographySlots_[Menu::kCartographyResult];
    const std::optional<storage::StoredItemStack>& map = cartographySlots_[Menu::kCartographyMap];
    const std::optional<storage::StoredItemStack>& additional = cartographySlots_[Menu::kCartographyAdditional];
    MapSystem* maps = owner_->options().maps;
    if (!map.has_value() || !additional.has_value() || maps == nullptr) {
        result.reset();
        return;
    }
    // parity: `setupResultSlot` leaves the slot alone for a map without data.
    if (const std::optional<std::int32_t> id = MapSystem::mapIdOf(map->components);
        !id.has_value() || !maps->summaryOf(*id).has_value()) {
        return;
    }
    result = world::maps::cartographyResult(*map, *additional, *maps, owner_->options().itemTags);
}

void ContainerMenuController::takeCartographyResult() {
    for (const std::int32_t slot : {Menu::kCartographyMap, Menu::kCartographyAdditional}) {
        std::optional<storage::StoredItemStack>& input = cartographySlots_[static_cast<std::size_t>(slot)];
        if (input.has_value() && --input->count <= 0) {
            input.reset();
        }
    }
    if (PlayerSession* session = owner_->playerSession(); session != nullptr) {
        settleCraftedMaps(*session);
    }
    // parity: the take sound plays at most once a game tick.
    world::LoadedWorld* world = owner_->currentWorld();
    if (world != nullptr && openBlock.has_value() && cartographySoundTime_ != world->gameTime()) {
        cartographySoundTime_ = world->gameTime();
        owner_->blockInteraction().broadcastEffects(
            *openBlock, world->state().blockState(openBlock->x, openBlock->y, openBlock->z).value_or(0),
            {world::blockEffect::Sound{.name = "ui.cartography_table.take_result",
                                       .source = SoundSource::Blocks,
                                       .volume = 1.0F,
                                       .pitch = 1.0F}});
    }
    recomputeCartographyResult();
}

void ContainerMenuController::settleCraftedMaps(PlayerSession& session) {
    MapSystem* maps = owner_->options().maps;
    world::LoadedWorld* world = owner_->currentWorld();
    if (maps != nullptr && world != nullptr) {
        static_cast<void>(world::maps::settleCraftedMaps(session, &carriedItem_, *maps, *world));
    }
}

} // namespace basalt::server::runtime
