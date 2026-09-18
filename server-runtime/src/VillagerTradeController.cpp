#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/VillagerTradeController.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/Villagers.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealVillagerTrades.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/text/Component.hpp>

#include <cstdint>
#include <random>
#include <utility>

namespace basalt::server::runtime {
namespace {

using player::PlayerSession;
using wire::common::Identifier;
using wire::protocol::play::MerchantCost;
using wire::protocol::play::MerchantOffer;
using wire::protocol::play::MerchantOffersPacket;
using wire::protocol::play::OpenScreenPacket;

} // namespace

VillagerTradeController::VillagerTradeController(ClientSession& owner, Hooks hooks)
    : owner_(&owner), hooks_(std::move(hooks)) {}

void VillagerTradeController::openTrades(entity::ServerEntity& villager) {
    PlayerSession* session = owner_->playerSession();
    world::RealVillagerTrades* trades = owner_->options().villagerTrades;
    if (session == nullptr || trades == nullptr) {
        return;
    }
    // parity: `Villager.mobInteract` over `MerchantMenu`: the offers go out as they are, and what
    // the player then picks is a `select_trade`. The offers themselves come out of the jar's own
    // data, so a farmer offers what a farmer offers.
    std::vector<world::trades::Offer> offers =
        entity::villagers::offersFor(*trades, villager.profession, villager.villagerLevel);
    if (offers.empty()) {
        return;
    }
    ContainerMenuController& menus = owner_->menus();
    menus.returnCraftingGrid(*session);
    menus.nextContainerId = menus.nextContainerId >= ClientSession::kMaxContainerId
                                ? ClientSession::kFirstBlockContainerId
                                : menus.nextContainerId + 1;
    menus.openMenu = player::Menu::merchant(menus.nextContainerId);
    menus.openBlock.reset();
    menus.openBlockSecond.reset();
    openTradingWith_ = villager.entityId();
    openOffers_ = std::move(offers);
    owner_->sendCompressed(OpenScreenPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        OpenScreenPacket{.containerId = menus.openMenu.containerId(),
                         .menuType = wire::protocol::play::menuType::kMerchant,
                         .title = wire::text::Component::translatable("entity.minecraft.villager")}
            .write(buffer);
    });
    sendOffers();
    owner_->sendInventory();
}

void VillagerTradeController::sendOffers() {
    if (!openTradingWith_.has_value() || openOffers_.empty() || owner_->currentWorld() == nullptr) {
        return;
    }
    const entity::ServerEntity* villager = owner_->currentWorld()->state().entity(*openTradingWith_);
    if (villager == nullptr) {
        return;
    }
    std::vector<MerchantOffer> wire;
    for (const world::trades::Offer& offer : openOffers_) {
        const server::registry::Item* costA = server::registry::items::registry().byName(offer.costA.item);
        const server::registry::Item* result = server::registry::items::registry().byName(offer.result.item);
        if (costA == nullptr || result == nullptr) {
            continue;
        }
        std::optional<MerchantCost> costB;
        if (offer.costB.has_value()) {
            const server::registry::Item* second = server::registry::items::registry().byName(offer.costB->item);
            if (second != nullptr) {
                costB =
                    MerchantCost{.itemId = second->protocolId, .count = world::trades::priceOf(offer, *offer.costB)};
            }
        }
        wire.push_back(MerchantOffer{
            .costA = MerchantCost{.itemId = costA->protocolId, .count = world::trades::priceOf(offer, offer.costA)},
            .result = wire::protocol::play::WireItemStack{.itemId = result->protocolId, .count = offer.result.count},
            .costB = costB,
            .outOfStock = offer.outOfStock(),
            .uses = offer.uses,
            .maxUses = offer.maxUses,
            .experience = offer.experience,
            .specialPrice = offer.specialPrice,
            .priceMultiplier = offer.priceMultiplier,
            .demand = offer.demand});
    }
    owner_->sendPlayPacket(MerchantOffersPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        MerchantOffersPacket{.containerId = owner_->menus().openMenu.containerId(),
                             .offers = wire,
                             .level = villager->villagerLevel,
                             .experience = villager->villagerExperience,
                             .showProgress = true,
                             .canRestock = true}
            .write(buffer);
    });
}

void VillagerTradeController::selectTrade(std::int32_t index) {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr || index < 0 || static_cast<std::size_t>(index) >= openOffers_.size()) {
        return;
    }
    if (hooks_.tradeAllowed &&
        !hooks_.tradeAllowed(owner_->menus().openMenu.containerId(), index,
                             std::optional{openOffers_[static_cast<std::size_t>(index)].result.item.toString()})) {
        return;
    }
    // parity: `MerchantMenu.clickMenuButton`: the offer is checked against what they are carrying,
    // the cost comes out of the inventory, the result goes in, and the offer's own use is counted.
    world::trades::Offer& offer = openOffers_[static_cast<std::size_t>(index)];
    if (offer.outOfStock()) {
        return;
    }
    const auto [costA, costB] = world::trades::currentCost(offer);
    if (session->inventory.countOf(costA.item.toString()) < costA.count) {
        return;
    }
    if (costB.has_value() && session->inventory.countOf(costB->item.toString()) < costB->count) {
        return;
    }
    static_cast<void>(session->inventory.take(costA.item.toString(), costA.count));
    if (costB.has_value()) {
        static_cast<void>(session->inventory.take(costB->item.toString(), costB->count));
    }
    const std::int32_t given = session->inventory.add(
        storage::StoredItemStack{.id = offer.result.item.toString(), .count = offer.result.count});
    if (given > 0) {
        owner_->spawnDroppedItem(offer.result.item, given, session->x, session->y, session->z);
    }
    world::trades::take(offer);
    // parity: `AbstractVillager.notifyTrade` ends in `TRADE.trigger`, once per offer taken.
    if (world::AdvancementTriggers* triggers = owner_->options().advancementTriggers; triggers != nullptr) {
        triggers->onVillagerTrade(session->name(), session->uuid().toString());
    }
    if (owner_->currentWorld() != nullptr) {
        entity::ServerEntity* villager =
            openTradingWith_.has_value() ? owner_->currentWorld()->state().entity(*openTradingWith_) : nullptr;
        if (villager != nullptr) {
            villager->villagerExperience += offer.experience;
            // parity: `Villager.rewardTradeXp`, three to six points left at the merchant's feet.
            if (owner_->options().spawnOrb) {
                const std::int32_t points =
                    world::trades::kTradeOrbBase + static_cast<std::int32_t>(std::random_device{}() % 4U);
                owner_->options().spawnOrb(*owner_->currentWorld(), villager->x, villager->y + 0.5, villager->z,
                                           points);
            }
        }
    }
    owner_->sendInventory();
    sendOffers();
}

} // namespace basalt::server::runtime
