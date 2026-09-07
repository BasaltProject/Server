#include <basalt/server/runtime/world/Trades.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace basalt::server::runtime::world::trades {

std::int32_t priceOf(const Offer& offer, const server::world::loot::ItemStack& cost, std::int32_t maxStackSize) {
    const std::int32_t base = cost.count;
    const auto scaled = static_cast<std::int32_t>(
        std::floor(static_cast<double>(base) * offer.demand * static_cast<double>(offer.priceMultiplier)));
    const std::int32_t fromDemand = std::max(0, scaled);
    return std::clamp(base + fromDemand + offer.specialPrice, 1, maxStackSize);
}

std::pair<server::world::loot::ItemStack, std::optional<server::world::loot::ItemStack>>
currentCost(const Offer& offer) {
    server::world::loot::ItemStack first = offer.costA;
    first.count = priceOf(offer, offer.costA);
    std::optional<server::world::loot::ItemStack> second;
    if (offer.costB.has_value()) {
        second = *offer.costB;
        second->count = priceOf(offer, *offer.costB);
    }
    return {std::move(first), std::move(second)};
}

void take(Offer& offer) { offer.uses++; }

void restock(Offer& offer) {
    offer.demand = offer.demand + offer.uses - (offer.maxUses - offer.uses);
    offer.uses = 0;
}

bool satisfies(const std::optional<server::world::loot::ItemStack>& stack,
               const std::optional<server::world::loot::ItemStack>& cost) {
    if (!cost.has_value()) {
        return true;
    }
    if (!stack.has_value()) {
        return false;
    }
    return stack->item == cost->item && stack->count >= cost->count;
}

} // namespace basalt::server::runtime::world::trades
