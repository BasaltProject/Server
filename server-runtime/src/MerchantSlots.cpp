#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/VillagerTradeController.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/Villagers.hpp>
#include <basalt/server/runtime/entity/villager/VillagerBrain.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>

#include <algorithm>
#include <cstdint>

namespace basalt::server::runtime {
namespace {

using player::PlayerSession;
using server::world::loot::ItemStack;
using storage::StoredItemStack;

/// parity: `ItemCost.test`.
[[nodiscard]] bool costMatches(const ItemStack& cost, const std::optional<StoredItemStack>& stack) {
    if (!stack.has_value() || stack->count <= 0 || stack->id != cost.item.toString()) {
        return false;
    }
    for (const std::string& name : cost.components.names()) {
        const wire::nbt::NbtTag* wanted = cost.components.find(name);
        const wire::nbt::NbtTag* held = stack->components.find(name);
        if (held == nullptr || wanted == nullptr || !(*held == *wanted)) {
            return false;
        }
    }
    return true;
}

/// parity: `MerchantOffer.satisfiedBy`.
[[nodiscard]] bool satisfiedBy(const world::trades::Offer& offer, const std::optional<StoredItemStack>& buyA,
                               const std::optional<StoredItemStack>& buyB) {
    if (!costMatches(offer.costA, buyA) || buyA->count < world::trades::currentCost(offer).first.count) {
        return false;
    }
    if (!offer.costB.has_value()) {
        return !buyB.has_value();
    }
    return costMatches(*offer.costB, buyB) && buyB->count >= offer.costB->count;
}

/// parity: `MerchantOffers.getRecipeFor`.
[[nodiscard]] std::optional<std::size_t> recipeFor(const std::vector<world::trades::Offer>& offers,
                                                   const std::optional<StoredItemStack>& buyA,
                                                   const std::optional<StoredItemStack>& buyB, std::int32_t hint) {
    if (hint > 0 && static_cast<std::size_t>(hint) < offers.size()) {
        return satisfiedBy(offers[static_cast<std::size_t>(hint)], buyA, buyB)
                   ? std::optional{static_cast<std::size_t>(hint)}
                   : std::nullopt;
    }
    for (std::size_t index = 0; index < offers.size(); ++index) {
        if (satisfiedBy(offers[index], buyA, buyB)) {
            return index;
        }
    }
    return std::nullopt;
}

void shrink(std::optional<StoredItemStack>& stack, std::int32_t count) {
    stack->count -= count;
    if (stack->count <= 0) {
        stack.reset();
    }
}

} // namespace

std::vector<world::trades::Offer>* VillagerTradeController::offers() {
    return openOffers_ == nullptr ? nullptr : &openOffers_->list;
}

const std::optional<StoredItemStack>& VillagerTradeController::slot(std::int32_t index) const {
    static const std::optional<StoredItemStack> kNothing;
    return index >= 0 && index < 3 ? slots_[static_cast<std::size_t>(index)] : kNothing;
}

void VillagerTradeController::setSlot(std::int32_t index, std::optional<StoredItemStack> stack) {
    if (index == kResult) {
        if (!stack.has_value() && slots_[kResult].has_value()) {
            takeResult();
        }
        return;
    }
    if (index != kPaymentA && index != kPaymentB) {
        return;
    }
    slots_[static_cast<std::size_t>(index)] = std::move(stack);
    updateSellItem();
}

void VillagerTradeController::updateSellItem() {
    activeOffer_.reset();
    std::optional<StoredItemStack> buyA = slots_[kPaymentA];
    std::optional<StoredItemStack> buyB = slots_[kPaymentB];
    if (!buyA.has_value()) {
        buyA = buyB;
        buyB.reset();
    }
    if (!buyA.has_value()) {
        slots_[kResult].reset();
        return;
    }
    std::vector<world::trades::Offer>* list = offers();
    if (list != nullptr && !list->empty()) {
        std::optional<std::size_t> offer = recipeFor(*list, buyA, buyB, selectionHint_);
        if (!offer.has_value() || (*list)[*offer].outOfStock()) {
            activeOffer_ = offer;
            offer = recipeFor(*list, buyB, buyA, selectionHint_);
        }
        if (offer.has_value() && !(*list)[*offer].outOfStock()) {
            activeOffer_ = offer;
            const ItemStack& result = (*list)[*offer].result;
            slots_[kResult] =
                StoredItemStack{.id = result.item.toString(), .count = result.count, .components = result.components};
        } else {
            slots_[kResult].reset();
        }
    }
    // parity: `AbstractVillager.notifyTradeUpdated`, a yes for a trade and a no for none.
    world::LoadedWorld* world = owner_->currentWorld();
    entity::ServerEntity* villager =
        world != nullptr && openTradingWith_.has_value() ? world->state().entity(*openTradingWith_) : nullptr;
    if (villager != nullptr && owner_->options().mobs != nullptr) {
        entity::villager::VillagerWorld& villagers = owner_->options().mobs->villagersIn(*world);
        entity::villager::villagerBrain::tradeUpdated(*villager, villagers, slots_[kResult].has_value(),
                                                      world->gameTime());
    }
}

bool VillagerTradeController::mayTakeResult() const {
    if (!activeOffer_.has_value() || openOffers_ == nullptr || *activeOffer_ >= openOffers_->list.size()) {
        return false;
    }
    const world::trades::Offer& offer = openOffers_->list[*activeOffer_];
    return satisfiedBy(offer, slots_[kPaymentA], slots_[kPaymentB]) ||
           satisfiedBy(offer, slots_[kPaymentB], slots_[kPaymentA]);
}

void VillagerTradeController::takeResult() {
    const std::optional<StoredItemStack> made = slots_[kResult];
    slots_[kResult].reset();
    PlayerSession* session = owner_->playerSession();
    // parity: `MerchantResultSlot.checkTakeAchievements` over `ItemStack.onCraftedBy`.
    if (session != nullptr && made.has_value()) {
        session->stats.increment(storage::stats::crafted(made->id), made->count);
    }
    if (!activeOffer_.has_value() || openOffers_ == nullptr || *activeOffer_ >= openOffers_->list.size() ||
        session == nullptr) {
        updateSellItem();
        return;
    }
    world::trades::Offer& offer = openOffers_->list[*activeOffer_];
    // parity: `MerchantOffer.take`, tried with the payments either way round.
    std::optional<StoredItemStack>& first = slots_[kPaymentA];
    std::optional<StoredItemStack>& second = slots_[kPaymentB];
    const std::int32_t priceA = world::trades::currentCost(offer).first.count;
    const std::int32_t priceB = offer.costB.has_value() ? offer.costB->count : 0;
    bool taken = false;
    if (satisfiedBy(offer, first, second)) {
        shrink(first, priceA);
        if (priceB > 0) {
            shrink(second, priceB);
        }
        taken = true;
    } else if (satisfiedBy(offer, second, first)) {
        shrink(second, priceA);
        if (priceB > 0) {
            shrink(first, priceB);
        }
        taken = true;
    }
    if (taken) {
        // parity: `AbstractVillager.notifyTrade`: a use, the experience, and the `TRADE` trigger.
        world::trades::take(offer);
        player::statAwards::award(*session, "traded_with_villager");
        if (world::LoadedWorld* world = owner_->currentWorld();
            world != nullptr && owner_->options().mobs != nullptr && openTradingWith_.has_value()) {
            if (entity::ServerEntity* villager = world->state().entity(*openTradingWith_); villager != nullptr) {
                entity::villager::villagerBrain::noteTrade(*villager, world->gameTime());
                random::RandomSource random =
                    random::RandomSource::legacy(owner_->options().nextSeed ? owner_->options().nextSeed() : 0);
                const std::int32_t orb = entity::villager::villagerBrain::rewardTradeXp(
                    *villager, owner_->options().mobs->villagersIn(*world), offer.experience, random);
                if (orb > 0 && owner_->options().spawnOrb) {
                    owner_->options().spawnOrb(*world, villager->x, villager->y + 0.5, villager->z, orb);
                }
            }
        }
        if (world::AdvancementTriggers* triggers = owner_->options().advancementTriggers; triggers != nullptr) {
            // parity: `TradeTrigger`, the trader and what the offer gives.
            world::TriggerEvent traded{.items = {{"item", world::TriggerItem{.item = offer.result.item,
                                                                             .count = offer.result.count,
                                                                             .components = offer.result.components}}}};
            if (world::LoadedWorld* world = owner_->currentWorld(); world != nullptr && openTradingWith_.has_value()) {
                if (const entity::ServerEntity* trader = world->state().entity(*openTradingWith_); trader != nullptr) {
                    traded.entities.emplace("villager", world::triggerFacts::ofEntity(*world, *trader));
                }
            }
            triggers->fire("villager_trade", session->name(), session->uuid().toString(), traded);
        }
    }
    updateSellItem();
}

void VillagerTradeController::quickMoveResult(PlayerSession& session) {
    // parity: `AbstractContainerMenu.doClick`'s quick move keeps taking the result while the same.
    // item comes back and the whole of it fits.
    while (slots_[kResult].has_value() && mayTakeResult()) {
        const StoredItemStack result = *slots_[kResult];
        if (session.inventory.add(result) > 0) {
            break;
        }
        takeResult();
        if (!slots_[kResult].has_value() || slots_[kResult]->id != result.id) {
            break;
        }
    }
}

void VillagerTradeController::moveFromInventory(PlayerSession& session, std::int32_t paymentSlot,
                                                const ItemStack& cost) {
    std::optional<StoredItemStack>& payment = slots_[static_cast<std::size_t>(paymentSlot)];
    // parity: the menu's inventory slots, storage first and the hotbar after.
    std::vector<std::int32_t> order;
    for (std::int32_t index = player::PlayerInventory::kHotbarStart; index < player::PlayerInventory::kMainSlots;
         ++index) {
        order.push_back(index);
    }
    for (std::int32_t index = 0; index < player::PlayerInventory::kHotbarStart; ++index) {
        order.push_back(index);
    }
    for (const std::int32_t index : order) {
        std::optional<StoredItemStack> held = session.inventory.at(index);
        if (!costMatches(cost, held)) {
            continue;
        }
        if (payment.has_value() && (payment->id != held->id || !(payment->components == held->components))) {
            continue;
        }
        const std::int32_t maxStack =
            server::registry::itemStacking::maxStackSize(wire::common::Identifier::parse(held->id).value_or(cost.item));
        const std::int32_t current = payment.has_value() ? payment->count : 0;
        const std::int32_t moved = std::min(maxStack - current, held->count);
        StoredItemStack next = *held;
        next.count = current + moved;
        held->count -= moved;
        session.inventory.set(index, held->count <= 0 ? std::nullopt : held);
        payment = next;
        updateSellItem();
        if (next.count >= maxStack) {
            break;
        }
    }
}

} // namespace basalt::server::runtime
