#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/Trades.hpp>

#include <doctest.h>

using basalt::server::runtime::world::trades::Offer;
using basalt::server::world::loot::ItemStack;
using basalt::wire::common::Identifier;
namespace trades = basalt::server::runtime::world::trades;

namespace {

[[nodiscard]] ItemStack stack(std::string_view item, std::int32_t count) {
    return {.item = Identifier::vanilla(std::string{item}), .count = count};
}

} // namespace

TEST_CASE("an offer costs its base until demand or a discount moves it") {
    Offer offer{.costA = stack("emerald", 3), .result = stack("bread", 6)};
    CHECK(trades::priceOf(offer, offer.costA) == 3);

    // parity: base times demand times the multiplier, floored: 3 * 10 * 0.05 is 1.5, so one more.
    offer.demand = 10;
    CHECK(trades::priceOf(offer, offer.costA) == 4);

    // A discount takes it down and never below one, however deep it goes.
    offer.specialPrice = -100;
    CHECK(trades::priceOf(offer, offer.costA) == 1);

    // And it never asks for more than a stack, however high demand climbs.
    offer.specialPrice = 0;
    offer.demand = 100000;
    CHECK(trades::priceOf(offer, offer.costA) == 64);

    // Negative demand does not make a trade cheaper: vanilla floors that half at zero.
    offer.demand = -100;
    CHECK(trades::priceOf(offer, offer.costA) == 3);
}

TEST_CASE("an offer runs out after its uses and a restock moves demand by how wanted it was") {
    Offer offer{.costA = stack("emerald", 1), .result = stack("bread", 6), .maxUses = 2};
    CHECK_FALSE(offer.outOfStock());
    trades::take(offer);
    trades::take(offer);
    CHECK(offer.outOfStock());

    // Used twice out of two: demand climbs by the full two, which is what makes a popular trade
    // cost more after the villager restocks.
    trades::restock(offer);
    CHECK(offer.demand == 2);
    CHECK(offer.uses == 0);
    CHECK_FALSE(offer.outOfStock());

    // Used none out of two: demand falls back by the same two.
    trades::restock(offer);
    CHECK(offer.demand == 0);
}

TEST_CASE("both cost slots are priced, and only what really pays counts as payment") {
    Offer offer{
        .costA = stack("emerald", 2), .costB = stack("book", 1), .result = stack("enchanted_book", 1), .demand = 20};
    const auto [first, second] = trades::currentCost(offer);
    CHECK(first.count == 4);
    REQUIRE(second.has_value());
    CHECK(second->count == 2);

    CHECK(trades::satisfies(stack("emerald", 4), first));
    CHECK_FALSE(trades::satisfies(stack("emerald", 3), first));
    CHECK_FALSE(trades::satisfies(stack("diamond", 64), first));
    // A slot the offer does not ask for is paid by anything, including nothing.
    CHECK(trades::satisfies(std::nullopt, std::nullopt));
    CHECK_FALSE(trades::satisfies(std::nullopt, first));
}
