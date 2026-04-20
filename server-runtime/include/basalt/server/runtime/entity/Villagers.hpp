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

namespace villagers {

[[nodiscard]] const wire::common::Identifier& type();

inline constexpr std::int32_t kFirstLevel = 1;
inline constexpr std::int32_t kMaxLevel = 5;

[[nodiscard]] std::span<const std::string_view> professions();

struct Offers {
    std::vector<world::trades::Offer> list;
    std::int32_t level = 0;
};

[[nodiscard]] std::vector<world::trades::Offer>& offersOf(world::RealVillagerTrades& trades, ServerEntity& villager,
                                                          world::LoadedWorld& world);

void writeOffers(const ServerEntity& villager, wire::nbt::NbtCompound& tag);

void readOffers(const wire::nbt::NbtCompound& tag, ServerEntity& villager);

[[nodiscard]] world::trades::Offer asOffer(const world::VillagerTrade& trade);

} // namespace villagers

} // namespace basalt::server::runtime::entity
