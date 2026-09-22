#pragma once

#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/RealVillagerTrades.hpp>
#include <basalt/server/runtime/world/Trades.hpp>

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::entity {

/// The villager as a merchant: what it offers, and what taking an offer costs it.
///
/// parity: `Villager` over `VillagerData`, trades read from the jar's data: its profession's up to its
/// level, demand raising the price, and an offer out of stock gone until the villager restocks.
namespace villagers {

[[nodiscard]] const wire::common::Identifier& type();

/// parity: `VillagerData.MIN_VILLAGER_LEVEL`, which a villager starts at, and the five it can reach.
inline constexpr std::int32_t kFirstLevel = 1;
inline constexpr std::int32_t kMaxLevel = 5;

/// parity: `VillagerProfession`, whose names are what the trade files are filed under.
[[nodiscard]] std::span<const std::string_view> professions();

/// What one villager offers, and the level it has rolled offers up to.
struct Offers {
    std::vector<world::trades::Offer> list;
    std::int32_t level = 0;
};

/// The offers a villager shows, rolling any level it has reached but not yet stocked.
///
/// parity: `Villager.updateTrades` over `AbstractVillager.addOffersFromTradeSet`: each level's trade
/// set draws its amount of trades from the level's random sequence, trying another when one gives
/// no offer, and without repeating one unless the set allows it.
[[nodiscard]] std::vector<world::trades::Offer>& offersOf(world::RealVillagerTrades& trades, ServerEntity& villager,
                                                          world::LoadedWorld& world);

/// parity: `AbstractVillager`'s `Offers`, `MerchantOffers.CODEC` over each `MerchantOffer`.
void writeOffers(const ServerEntity& villager, wire::nbt::NbtCompound& tag);

/// Reads them back; a villager saved with offers has rolled every level it has reached.
void readOffers(const wire::nbt::NbtCompound& tag, ServerEntity& villager);

/// parity: one `MerchantOffer` per trade file, with the file's own counts and limits.
[[nodiscard]] world::trades::Offer asOffer(const world::VillagerTrade& trade);

} // namespace villagers

} // namespace basalt::server::runtime::entity
