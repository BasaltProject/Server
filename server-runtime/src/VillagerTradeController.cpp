#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/VillagerTradeController.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/Villagers.hpp>
#include <basalt/server/runtime/entity/villager/VillagerBrain.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealVillagerTrades.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/text/Component.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime {
namespace {

using player::PlayerSession;
using wire::common::Identifier;
using wire::protocol::play::MerchantCost;
using wire::protocol::play::MerchantOffer;
using wire::protocol::play::MerchantOffersPacket;
using wire::protocol::play::OpenScreenPacket;

/// parity: the player's `entity_interaction_range` and eye heights, and the villager's box.
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
    // parity: `Villager.mobInteract`: a baby or a villager with nothing to offer shakes its head, one
    // already trading or asleep does nothing, and the rest open with this buyer's prices.
    entity::villager::VillagerWorld& villagers = owner_->options().mobs->villagersIn(*world);
    if (!entity::villager::villagerBrain::startTrading(villager, villagers, session->entityId(),
                                                       session->uuid().toString(), mainHand)) {
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
    openOffers_ = villager.offers;
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
    if (session == nullptr || openOffers_ == nullptr || index < 0 ||
        static_cast<std::size_t>(index) >= openOffers_->list.size()) {
        return;
    }
    if (hooks_.tradeAllowed &&
        !hooks_.tradeAllowed(
            owner_->menus().openMenu.containerId(), index,
            std::optional{openOffers_->list[static_cast<std::size_t>(index)].result.item.toString()})) {
        return;
    }
    // parity: `MerchantMenu.clickMenuButton`: the offer is checked against what they are carrying,
    // the cost comes out of the inventory, the result goes in, and the offer's own use is counted.
    if (!tradingStillValid()) {
        owner_->menus().closeOpenMenu();
        return;
    }
    world::trades::Offer& offer = openOffers_->list[static_cast<std::size_t>(index)];
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
    const std::int32_t given = session->inventory.add(storage::StoredItemStack{
        .id = offer.result.item.toString(), .count = offer.result.count, .components = offer.result.components});
    if (given > 0) {
        owner_->spawnDroppedItem(offer.result.item, given, session->x, session->y, session->z);
    }
    world::trades::take(offer);
    // parity: `AbstractVillager.notifyTrade` ends in `TRADE.trigger`, once per offer taken.
    if (world::AdvancementTriggers* triggers = owner_->options().advancementTriggers; triggers != nullptr) {
        triggers->onVillagerTrade(session->name(), session->uuid().toString());
    }
    // parity: `AbstractVillager.notifyTrade` over `Villager.rewardTradeXp`.
    if (world::LoadedWorld* world = owner_->currentWorld(); world != nullptr && owner_->options().mobs != nullptr) {
        entity::ServerEntity* villager =
            openTradingWith_.has_value() ? world->state().entity(*openTradingWith_) : nullptr;
        if (villager != nullptr) {
            random::RandomSource random =
                random::RandomSource::legacy(owner_->options().nextSeed ? owner_->options().nextSeed() : 0);
            const std::int32_t orb = entity::villager::villagerBrain::rewardTradeXp(
                *villager, owner_->options().mobs->villagersIn(*world), offer.experience, random);
            if (orb > 0 && owner_->options().spawnOrb) {
                owner_->options().spawnOrb(*world, villager->x, villager->y + 0.5, villager->z, orb);
            }
        }
    }
    owner_->sendInventory();
    sendOffers();
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
    // parity: `isWithinEntityInteractionRange(this, 4.0)`, from the eyes to the villager's box.
    const double eyeY = session->y + (session->sneaking ? kSneakingEye : kStandingEye);
    const double halfWidth = kVillagerWidth / 2.0;
    const double dx = std::max({villager->x - halfWidth - session->x, 0.0, session->x - (villager->x + halfWidth)});
    const double dy = std::max({villager->y - eyeY, 0.0, eyeY - (villager->y + kVillagerHeight)});
    const double dz = std::max({villager->z - halfWidth - session->z, 0.0, session->z - (villager->z + halfWidth)});
    const double range = kEntityInteractionRange + kStillValidBuffer;
    return (dx * dx) + (dy * dy) + (dz * dz) < range * range;
}

void VillagerTradeController::menuClosed() {
    if (!openTradingWith_.has_value()) {
        return;
    }
    if (world::LoadedWorld* world = owner_->currentWorld(); world != nullptr) {
        if (entity::ServerEntity* villager = world->state().entity(*openTradingWith_); villager != nullptr) {
            entity::villager::VillagerState& state = entity::villager::villagerBrain::stateOf(*villager);
            const PlayerSession* session = owner_->playerSession();
            if (session != nullptr && state.tradingPlayer == session->entityId()) {
                entity::villager::villagerBrain::stopTrading(*villager);
            }
        }
    }
    openTradingWith_.reset();
    openOffers_.reset();
}

void VillagerTradeController::tick() {
    if (openTradingWith_.has_value() && !tradingStillValid()) {
        owner_->menus().closeOpenMenu();
    }
}

} // namespace basalt::server::runtime
