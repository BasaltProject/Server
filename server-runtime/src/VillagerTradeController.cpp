#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/VillagerTradeController.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/Villagers.hpp>
#include <basalt/server/runtime/entity/villager/VillagerBrain.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealVillagerTrades.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/text/Component.hpp>

#include <algorithm>
#include <cstdint>
#include <expected>
#include <utility>

namespace basalt::server::runtime {
namespace {

using player::PlayerSession;
using wire::common::Identifier;
using wire::protocol::play::MerchantCost;
using wire::protocol::play::MerchantOffer;
using wire::protocol::play::MerchantOffersPacket;
using wire::protocol::play::OpenScreenPacket;

/// parity: the player's `entity_interaction_range` and eye heights.
constexpr double kEntityInteractionRange = 3.0;
constexpr double kStillValidBuffer = 4.0;
constexpr double kStandingEye = 1.62;
constexpr double kSneakingEye = 1.27;
constexpr double kVillagerWidth = 0.6;
constexpr double kVillagerHeight = 1.95;

} // namespace

VillagerTradeController::VillagerTradeController(ClientSession& owner, Hooks hooks)
    : owner_(&owner), hooks_(std::move(hooks)) {}

void VillagerTradeController::openTrades(entity::ServerEntity& villager, bool mainHand) {
    PlayerSession* session = owner_->playerSession();
    world::RealVillagerTrades* trades = owner_->options().villagerTrades;
    world::LoadedWorld* world = owner_->currentWorld();
    if (session == nullptr || trades == nullptr || world == nullptr || owner_->options().mobs == nullptr ||
        villager.removed) {
        return;
    }
    // parity: `Villager.mobInteract`.
    // already trading or asleep does nothing, and the rest open with this buyer's prices.
    entity::villager::VillagerWorld& villagers = owner_->options().mobs->villagersIn(*world);
    bool talked = false;
    const bool trading = entity::villager::villagerBrain::startTrading(villager, villagers, session->entityId(),
                                                                       session->uuid().toString(), mainHand, &talked);
    if (talked) {
        player::statAwards::award(*session, "talked_to_villager");
    }
    if (!trading) {
        return;
    }
    ContainerMenuController& menus = owner_->menus();
    menus.openMerchantMenu(*session);
    openTradingWith_ = villager.entityId();
    openOffers_ = villager.offers;
    slots_ = {};
    selectionHint_ = 0;
    activeOffer_.reset();
    // parity: `getDisplayName`, which for a villager is its profession's name.
    owner_->sendCompressed(OpenScreenPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        OpenScreenPacket{.containerId = menus.openMenu.containerId(),
                         .menuType = wire::protocol::play::menuType::kMerchant,
                         .title =
                             wire::text::Component::translatable("entity.minecraft.villager." + villager.profession)}
            .write(buffer);
    });
    sendOffers();
    owner_->sendInventory();
}

void VillagerTradeController::sendOffers() {
    if (!openTradingWith_.has_value() || openOffers_ == nullptr || openOffers_->list.empty() ||
        owner_->currentWorld() == nullptr) {
        return;
    }
    const entity::ServerEntity* villager = owner_->currentWorld()->state().entity(*openTradingWith_);
    if (villager == nullptr) {
        return;
    }
    std::vector<MerchantOffer> wire;
    const PlayerSession* session = owner_->playerSession();
    for (const world::trades::Offer& offer : openOffers_->list) {
        const server::registry::Item* costA = server::registry::items::registry().byName(offer.costA.item);
        const server::registry::Item* result = server::registry::items::registry().byName(offer.result.item);
        if (costA == nullptr || result == nullptr) {
            continue;
        }
        std::optional<MerchantCost> costB;
        if (offer.costB.has_value()) {
            const server::registry::Item* second = server::registry::items::registry().byName(offer.costB->item);
            if (second != nullptr) {
                costB = MerchantCost{.itemId = second->protocolId, .count = offer.costB->count};
            }
        }
        wire.push_back(MerchantOffer{
            .costA = MerchantCost{.itemId = costA->protocolId, .count = world::trades::currentCost(offer).first.count},
            .result =
                session == nullptr
                    ? wire::protocol::play::WireItemStack{.itemId = result->protocolId, .count = offer.result.count}
                    : session->inventory
                          .toWireStack(storage::StoredItemStack{.id = offer.result.item.toString(),
                                                                .count = offer.result.count,
                                                                .components = offer.result.components})
                          .value_or(wire::protocol::play::WireItemStack{}),
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
    if (session == nullptr || openOffers_ == nullptr) {
        return;
    }
    if (!tradingStillValid()) {
        return;
    }
    if (hooks_.tradeAllowed && index >= 0 && static_cast<std::size_t>(index) < openOffers_->list.size() &&
        !hooks_.tradeAllowed(
            owner_->menus().openMenu.containerId(), index,
            std::optional{openOffers_->list[static_cast<std::size_t>(index)].result.item.toString()})) {
        return;
    }
    selectionHint_ = index;
    updateSellItem();
    // parity: `tryMoveItems`.
    if (index < 0 || static_cast<std::size_t>(index) >= openOffers_->list.size()) {
        return;
    }
    for (const std::int32_t payment : {kPaymentA, kPaymentB}) {
        std::optional<storage::StoredItemStack>& held = slots_[static_cast<std::size_t>(payment)];
        if (!held.has_value()) {
            continue;
        }
        const std::int32_t left = session->inventory.add(*held);
        if (left > 0) {
            held->count = left;
            updateSellItem();
            owner_->sendInventory();
            return;
        }
        held.reset();
        updateSellItem();
    }
    const world::trades::Offer offer = openOffers_->list[static_cast<std::size_t>(index)];
    moveFromInventory(*session, kPaymentA, offer.costA);
    if (offer.costB.has_value()) {
        moveFromInventory(*session, kPaymentB, *offer.costB);
    }
    owner_->sendInventory();
}

void VillagerTradeController::menuClosed() {
    if (!openTradingWith_.has_value()) {
        return;
    }
    PlayerSession* session = owner_->playerSession();
    if (world::LoadedWorld* world = owner_->currentWorld(); world != nullptr) {
        if (entity::ServerEntity* villager = world->state().entity(*openTradingWith_); villager != nullptr) {
            const entity::villager::VillagerState& state = entity::villager::villagerBrain::stateOf(*villager);
            if (session != nullptr && state.tradingPlayer == session->entityId()) {
                entity::villager::villagerBrain::stopTrading(*villager);
            }
        }
    }
    // parity: `MerchantMenu.removed`.
    // player who is dead.
    if (session != nullptr) {
        for (const std::int32_t payment : {kPaymentA, kPaymentB}) {
            std::optional<storage::StoredItemStack>& held = slots_[static_cast<std::size_t>(payment)];
            if (!held.has_value()) {
                continue;
            }
            const std::int32_t left = session->dead ? held->count : session->inventory.add(*held);
            if (left > 0) {
                if (const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
                        wire::common::Identifier::parse(held->id);
                    id.has_value()) {
                    owner_->spawnDroppedItem(*id, left, session->x, session->y, session->z);
                }
            }
            held.reset();
        }
    }
    slots_ = {};
    activeOffer_.reset();
    openTradingWith_.reset();
    openOffers_.reset();
}

} // namespace basalt::server::runtime

namespace basalt::server::runtime {

bool VillagerTradeController::tradingStillValid() {
    const PlayerSession* session = owner_->playerSession();
    world::LoadedWorld* world = owner_->currentWorld();
    if (session == nullptr || world == nullptr || !openTradingWith_.has_value()) {
        return false;
    }
    entity::ServerEntity* villager = world->state().entity(*openTradingWith_);
    if (villager == nullptr || villager->removed || villager->health <= 0.0 ||
        entity::villager::villagerBrain::stateOf(*villager).tradingPlayer != session->entityId()) {
        return false;
    }
    // parity: `isWithinEntityInteractionRange(this, 4.0)`.
    const double eyeY = session->y + (session->sneaking ? kSneakingEye : kStandingEye);
    const double halfWidth = kVillagerWidth / 2.0;
    const double dx = std::max({villager->x - halfWidth - session->x, 0.0, session->x - (villager->x + halfWidth)});
    const double dy = std::max({villager->y - eyeY, 0.0, eyeY - (villager->y + kVillagerHeight)});
    const double dz = std::max({villager->z - halfWidth - session->z, 0.0, session->z - (villager->z + halfWidth)});
    const double range = kEntityInteractionRange + kStillValidBuffer;
    return (dx * dx) + (dy * dy) + (dz * dz) < range * range;
}

void VillagerTradeController::tick() {
    if (openTradingWith_.has_value() && !tradingStillValid()) {
        owner_->menus().closeOpenMenu();
    }
}

} // namespace basalt::server::runtime
