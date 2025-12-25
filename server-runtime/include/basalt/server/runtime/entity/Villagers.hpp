#pragma once

#include <basalt/server/runtime/world/RealVillagerTrades.hpp>
#include <basalt/server/runtime/world/Trades.hpp>

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::entity {

/// The villager as a merchant: what it offers, and what taking an offer costs it.
///
/// parity: `Villager` over `VillagerData`, with the trades themselves read out of the jar's own
/// data. A villager offers the trades of its profession up to its level, each one priced by the
/// trade rules: demand raises what it asks for, and running out of stock takes it off the list
/// until the villager restocks.
namespace villagers {

[[nodiscard]] const wire::common::Identifier& type();

/// parity: `VillagerData.MIN_VILLAGER_LEVEL`, which a villager starts at, and the five it can reach.
inline constexpr std::int32_t kFirstLevel = 1;
inline constexpr std::int32_t kMaxLevel = 5;

/// parity: `VillagerProfession`, whose names are what the trade files are filed under.
[[nodiscard]] std::span<const std::string_view> professions();

/// Turns the jar's trades into the offers a merchant shows, at this level and every one below.
[[nodiscard]] std::vector<world::trades::Offer> offersFor(world::RealVillagerTrades& trades,
                                                          std::string_view profession, std::int32_t level);

/// parity: one `MerchantOffer` per trade file, with the file's own counts and limits.
[[nodiscard]] world::trades::Offer asOffer(const world::VillagerTrade& trade);

} // namespace villagers

} // namespace basalt::server::runtime::entity
