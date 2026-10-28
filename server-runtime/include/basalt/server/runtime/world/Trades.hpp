#pragma once

#include <basalt/server/world/loot/Loot.hpp>

#include <cstdint>
#include <optional>
#include <utility>

namespace basalt::server::runtime::world {

/// parity: `MerchantOffer`.
namespace trades {

/// parity: `Villager.rewardTradeXp`.
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

/// parity: `MerchantOffer.getModifiedCostCount`.
[[nodiscard]] std::int32_t priceOf(const Offer& offer, const server::world::loot::ItemStack& cost,
                                   std::int32_t maxStackSize = 64);

[[nodiscard]] std::pair<server::world::loot::ItemStack, std::optional<server::world::loot::ItemStack>>
currentCost(const Offer& offer);

void take(Offer& offer);

/// parity: `updateDemand`.
void restock(Offer& offer);

[[nodiscard]] bool satisfies(const std::optional<server::world::loot::ItemStack>& stack,
                             const std::optional<server::world::loot::ItemStack>& cost);

} // namespace trades

} // namespace basalt::server::runtime::world
