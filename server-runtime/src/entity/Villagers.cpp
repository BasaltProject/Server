#include <basalt/server/runtime/entity/Villagers.hpp>

#include <algorithm>
#include <array>

namespace basalt::server::runtime::entity::villagers {
namespace {

constexpr std::array<std::string_view, 13> kProfessions{
    "armorer",       "butcher",   "cartographer", "cleric",   "farmer",    "fisherman",  "fletcher",
    "leatherworker", "librarian", "mason",        "shepherd", "toolsmith", "weaponsmith"};

} // namespace

const wire::common::Identifier& type() {
    static const wire::common::Identifier kType = wire::common::Identifier::vanilla("villager");
    return kType;
}

std::span<const std::string_view> professions() { return kProfessions; }

world::trades::Offer asOffer(const world::VillagerTrade& trade) {
    world::trades::Offer offer{.costA = server::world::loot::ItemStack{.item = trade.wants, .count = trade.wantsCount},
                               .result = server::world::loot::ItemStack{.item = trade.gives, .count = trade.givesCount},
                               .maxUses = trade.maxUses,
                               .experience = trade.experience,
                               .priceMultiplier = trade.priceMultiplier};
    if (trade.alsoWants.has_value()) {
        offer.costB = server::world::loot::ItemStack{.item = *trade.alsoWants, .count = trade.alsoWantsCount};
    }
    return offer;
}

std::vector<world::trades::Offer> offersFor(world::RealVillagerTrades& trades, std::string_view profession,
                                            std::int32_t level) {
    std::vector<world::trades::Offer> offers;
    for (std::int32_t at = kFirstLevel; at <= std::clamp(level, kFirstLevel, kMaxLevel); at++) {
        for (const world::VillagerTrade& trade : trades.forProfession(profession, at)) {
            offers.push_back(asOffer(trade));
        }
    }
    return offers;
}

} // namespace basalt::server::runtime::entity::villagers
