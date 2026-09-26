#include <basalt/server/runtime/BlockInteractionHandler.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/MapSystem.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

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
constexpr std::string_view kPostProcessing = "minecraft:map_post_processing";

[[nodiscard]] storage::StoredItemStack oneOf(const storage::StoredItemStack& stack, std::int32_t count) {
    storage::StoredItemStack copy = stack;
    copy.count = count;
    return copy;
}

} // namespace

void ContainerMenuController::openCartographyTable(PlayerSession& session, const BlockPos& pos) {
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
    const std::optional<std::int32_t> id = MapSystem::mapIdOf(map->components);
    const std::optional<MapSystem::Summary> data = id.has_value() ? maps->summaryOf(*id) : std::nullopt;
    if (!data.has_value()) {
        return;
    }
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> mapItem =
        wire::common::Identifier::parse(map->id);
    const bool extendable =
        mapItem.has_value() && owner_->options().itemTags != nullptr &&
        world::realItemTags::contains(*owner_->options().itemTags, "minecraft:extendable_maps", *mapItem);
    if (additional->id == "minecraft:paper" && extendable && !data->locked && data->scale < 4) {
        storage::StoredItemStack made = oneOf(*map, 1);
        made.components.putString(kPostProcessing, "scale");
        result = std::move(made);
    } else if (additional->id == "minecraft:glass_pane" && !data->locked) {
        storage::StoredItemStack made = oneOf(*map, 1);
        made.components.putString(kPostProcessing, "lock");
        result = std::move(made);
    } else if (additional->id == "minecraft:map") {
        result = oneOf(*map, 2);
    } else {
        result.reset();
    }
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
    if (maps == nullptr || world == nullptr) {
        return;
    }
    if (carriedItem_.has_value() && carriedItem_->components.contains(kPostProcessing)) {
        maps->postProcessCrafted(carriedItem_->components, *world);
    }
    for (std::int32_t slot = 0; slot < storage::StoredInventory::kPlayerSlots; ++slot) {
        const std::optional<storage::StoredItemStack>& stack = session.inventory.at(slot);
        if (stack.has_value() && stack->components.contains(kPostProcessing)) {
            storage::StoredItemStack settled = *stack;
            maps->postProcessCrafted(settled.components, *world);
            session.inventory.set(slot, std::move(settled));
        }
    }
}

} // namespace basalt::server::runtime
