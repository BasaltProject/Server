#pragma once

#include <basalt/server/world/loot/Loot.hpp>

#include <cstdint>
#include <optional>
#include <utility>

namespace basalt::server::runtime::world {

/// What a merchant offers and what it charges for it.
///
/// parity: `MerchantOffer`. The price is not fixed: it is the base cost plus what demand has done to
/// it plus whatever discount the buyer has earned, clamped to at least one and at most a stack. An
/// offer runs out after its uses and comes back when the merchant restocks.
///
/// Who does the offering is a villager, which does not exist yet. This is the half of trading that
/// is items: when a merchant exists it has an offer list and this prices it.
namespace trades {

/// One offer: what it takes, what it gives, and how its price moves.
/// parity: `Villager.rewardTradeXp`'s `3 + random.nextInt(4)`.
inline constexpr std::int32_t kTradeOrbBase = 3;

struct Offer {
    server::world::loot::ItemStack costA;
    std::optional<server::world::loot::ItemStack> costB;
    server::world::loot::ItemStack result;
    std::int32_t maxUses = 12;
    std::int32_t experience = 0;
    float priceMultiplier = 0.05F;
    std::int32_t uses = 0;
    std::int32_t demand = 0;
    std::int32_t specialPrice = 0;

    /// parity: `MerchantOffer.isOutOfStock`.
    [[nodiscard]] bool outOfStock() const noexcept { return uses >= maxUses; }
};

/// parity: `MerchantOffer.getModifiedCostCount`: the base, plus the demand times the multiplier
/// floored, plus the special price, never under one and never over a stack.
[[nodiscard]] std::int32_t priceOf(const Offer& offer, const server::world::loot::ItemStack& cost,
                                   std::int32_t maxStackSize = 64);

/// What this offer asks for right now, both slots.
[[nodiscard]] std::pair<server::world::loot::ItemStack, std::optional<server::world::loot::ItemStack>>
currentCost(const Offer& offer);

/// Takes the trade: one use, and the demand moves with it on the next restock.
void take(Offer& offer);

/// parity: `updateDemand`, which a merchant runs when it restocks rather than on each trade.
void restock(Offer& offer);

/// Whether `stack` pays for `cost`, which is what the menu checks before it hands anything over.
[[nodiscard]] bool satisfies(const std::optional<server::world::loot::ItemStack>& stack,
                             const std::optional<server::world::loot::ItemStack>& cost);

} // namespace trades

} // namespace basalt::server::runtime::world
