#pragma once

#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/RealVillagerTrades.hpp>
#include <basalt/server/runtime/world/Trades.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::entity {

namespace villagers {

[[nodiscard]] const wire::common::Identifier& type();

inline constexpr std::int32_t kFirstLevel = 1;
inline constexpr std::int32_t kMaxLevel = 5;

[[nodiscard]] std::span<const std::string_view> professions();

struct Offers {
    std::vector<world::trades::Offer> list;
    std::int32_t level = 0;
};

/// Rolls the trade set of a profession and level; a seed makes it repeatable and skips the world's sequence.
[[nodiscard]] std::vector<world::trades::Offer>
rollOffers(world::RealVillagerTrades& trades, std::string_view profession, std::int32_t level,
           world::LoadedWorld& world, const std::array<double, 3>& origin, std::string_view variant,
           std::optional<std::int32_t> fixedAmount, std::optional<std::int64_t> seed);

/// Draws offers from the listed trades without repeats, as a villager does for `addOffersFromItemListings`.
[[nodiscard]] std::vector<world::trades::Offer>
rollListed(world::RealVillagerTrades& trades, std::span<const wire::common::Identifier> listed, std::int32_t amount,
           world::LoadedWorld& world, const std::array<double, 3>& origin, std::string_view variant, std::int64_t seed);

[[nodiscard]] std::vector<world::trades::Offer>& offersOf(world::RealVillagerTrades& trades, ServerEntity& villager,
                                                          world::LoadedWorld& world);

/// Rolls more trades for the level it has reached; how many it gave.
std::int32_t addTrades(world::RealVillagerTrades& trades, ServerEntity& villager, world::LoadedWorld& world,
                       std::int32_t amount);

/// Forgets every offer, so the next ask rolls them again.
void resetOffers(ServerEntity& villager);

void writeOffers(const ServerEntity& villager, wire::nbt::NbtCompound& tag);

void readOffers(const wire::nbt::NbtCompound& tag, ServerEntity& villager);

[[nodiscard]] world::trades::Offer asOffer(const world::VillagerTrade& trade);

} // namespace villagers

} // namespace basalt::server::runtime::entity
