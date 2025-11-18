#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/RealVillagerTrades.hpp>

#include <doctest.h>

#include <map>
#include <string>

using basalt::server::runtime::world::RealVillagerTrades;
using basalt::server::runtime::world::VillagerTrade;
using basalt::wire::common::Identifier;

namespace {

struct FakeJar {
    std::map<std::string, std::string> files;
    std::int32_t reads = 0;

    [[nodiscard]] RealVillagerTrades::Jar handle() {
        return RealVillagerTrades::Jar{.readText = [this](std::string_view name) -> std::optional<std::string> {
            ++reads;
            const auto found = files.find(std::string{name});
            return found == files.end() ? std::nullopt : std::optional{found->second};
        }};
    }
};

[[nodiscard]] std::string tagFor(const std::string& profession, std::int32_t level) {
    return "data/minecraft/tags/villager_trade/" + profession + "/level_" + std::to_string(level) + ".json";
}

[[nodiscard]] std::string tradeFor(const std::string& path) {
    return "data/minecraft/villager_trade/" + path + ".json";
}

constexpr std::string_view kWheatTrade = R"({
    "wants": {"id": "minecraft:wheat", "count": 20},
    "gives": {"id": "minecraft:emerald", "count": 1},
    "max_uses": 16,
    "xp": 2,
    "reputation_discount": 0.05
})";

} // namespace

TEST_CASE("a trade file reads as what it wants, what it gives and its three numbers") {
    const std::optional<VillagerTrade> trade =
        RealVillagerTrades::parse(Identifier::vanilla("farmer/1/wheat"), kWheatTrade);

    REQUIRE(trade.has_value());
    CHECK(trade->id == Identifier::vanilla("farmer/1/wheat"));
    CHECK(trade->wants == Identifier::vanilla("wheat"));
    CHECK(trade->wantsCount == 20);
    CHECK_FALSE(trade->alsoWants.has_value());
    CHECK(trade->alsoWantsCount == 0);
    CHECK(trade->gives == Identifier::vanilla("emerald"));
    CHECK(trade->givesCount == 1);
    CHECK(trade->maxUses == 16);
    CHECK(trade->experience == 2);
    CHECK(trade->priceMultiplier == doctest::Approx(0.05F));
}

TEST_CASE("a trade that wants two things reads the second one too") {
    const std::optional<VillagerTrade> trade = RealVillagerTrades::parse(Identifier::vanilla("armorer/3/plate"), R"({
        "wants": {"id": "minecraft:emerald", "count": 4},
        "additional_wants": {"id": "minecraft:iron_ingot", "count": 3},
        "gives": {"id": "minecraft:iron_chestplate"}
    })");

    REQUIRE(trade.has_value());
    REQUIRE(trade->alsoWants.has_value());
    CHECK(*trade->alsoWants == Identifier::vanilla("iron_ingot"));
    CHECK(trade->alsoWantsCount == 3);

    // A second want with no count of its own is one of it, like the first.
    const std::optional<VillagerTrade> bare = RealVillagerTrades::parse(Identifier::vanilla("armorer/3/bare"), R"({
        "wants": {"id": "minecraft:emerald"},
        "additional_wants": {"id": "minecraft:iron_ingot"},
        "gives": {"id": "minecraft:iron_chestplate"}
    })");
    REQUIRE(bare.has_value());
    CHECK(bare->alsoWantsCount == 1);
}

TEST_CASE("the numbers a trade file leaves out take their own defaults") {
    const std::optional<VillagerTrade> trade = RealVillagerTrades::parse(Identifier::vanilla("farmer/1/bare"), R"({
        "wants": {"id": "minecraft:wheat"},
        "gives": {"id": "minecraft:emerald"}
    })");

    REQUIRE(trade.has_value());
    CHECK(trade->wantsCount == 1);
    CHECK(trade->givesCount == 1);
    // parity: twelve uses, no experience and no discount.
    CHECK(trade->maxUses == RealVillagerTrades::kDefaultMaxUses);
    CHECK(trade->maxUses == 12);
    CHECK(trade->experience == 0);
    CHECK(trade->priceMultiplier == doctest::Approx(0.0F));
    // An additional_wants that is not there is not a count of one.
    CHECK(trade->alsoWantsCount == 0);
}

TEST_CASE("a trade file with nothing to want or nothing to give is not a trade") {
    const Identifier id = Identifier::vanilla("farmer/1/broken");

    CHECK_FALSE(RealVillagerTrades::parse(id, R"({"gives": {"id": "minecraft:emerald"}})").has_value());
    CHECK_FALSE(RealVillagerTrades::parse(id, R"({"wants": {"id": "minecraft:wheat"}})").has_value());
    CHECK_FALSE(RealVillagerTrades::parse(id, R"({"wants": {}, "gives": {"id": "minecraft:emerald"}})").has_value());
    CHECK_FALSE(RealVillagerTrades::parse(id, R"({"wants": {"id": "a:b:c"}, "gives": {"id": "minecraft:emerald"}})")
                    .has_value());
    CHECK_FALSE(RealVillagerTrades::parse(id, "{not json").has_value());
    CHECK_FALSE(RealVillagerTrades::parse(id, "[]").has_value());
}

TEST_CASE("a level's trades come back in the order its tag lists them") {
    FakeJar jar{.files = {
                    {tagFor("farmer", 1), R"({"values": ["minecraft:farmer/1/wheat", "minecraft:farmer/1/potato"]})"},
                    {tradeFor("farmer/1/wheat"), std::string{kWheatTrade}},
                    {tradeFor("farmer/1/potato"), R"({
                         "wants": {"id": "minecraft:potato", "count": 26},
                         "gives": {"id": "minecraft:emerald"}
                     })"},
                }};
    RealVillagerTrades trades{jar.handle()};

    const std::vector<VillagerTrade>& level = trades.forProfession("farmer", 1);

    REQUIRE(level.size() == 2);
    CHECK(level[0].wants == Identifier::vanilla("wheat"));
    CHECK(level[1].wants == Identifier::vanilla("potato"));
    CHECK(level[1].wantsCount == 26);
}

TEST_CASE("a trade the tag names but the jar does not ship is left out") {
    FakeJar jar{.files = {{tagFor("farmer", 1),
                           R"({"values": ["minecraft:farmer/1/missing", "minecraft:farmer/1/wheat",
                                          "minecraft:farmer/1/broken", "a:b:c", 5]})"},
                          {tradeFor("farmer/1/wheat"), std::string{kWheatTrade}},
                          {tradeFor("farmer/1/broken"), "{not json"}}};
    RealVillagerTrades trades{jar.handle()};

    const std::vector<VillagerTrade>& level = trades.forProfession("farmer", 1);

    REQUIRE(level.size() == 1);
    CHECK(level[0].wants == Identifier::vanilla("wheat"));
}

TEST_CASE("each profession and level is read once and then remembered") {
    FakeJar jar{.files = {{tagFor("farmer", 1), R"({"values": ["minecraft:farmer/1/wheat"]})"},
                          {tradeFor("farmer/1/wheat"), std::string{kWheatTrade}}}};
    RealVillagerTrades trades{jar.handle()};

    CHECK(trades.forProfession("farmer", 1).size() == 1);
    CHECK(jar.reads == 2);
    CHECK(trades.forProfession("farmer", 1).size() == 1);
    CHECK(jar.reads == 2);

    // A level with no tag is an empty answer, and it is remembered too.
    CHECK(trades.forProfession("farmer", 2).empty());
    CHECK(jar.reads == 3);
    CHECK(trades.forProfession("farmer", 2).empty());
    CHECK(jar.reads == 3);
    // And so is another profession at the same level, which is a key of its own.
    CHECK(trades.forProfession("armorer", 1).empty());
    CHECK(jar.reads == 4);
}

TEST_CASE("a tag that is not a list of names yields no trades") {
    FakeJar jar{.files = {{tagFor("farmer", 1), R"({"values": {"not": "a list"}})"},
                          {tagFor("armorer", 1), "{not json"},
                          {tagFor("butcher", 1), R"({"replace": false})"}}};
    RealVillagerTrades trades{jar.handle()};

    CHECK(trades.forProfession("farmer", 1).empty());
    CHECK(trades.forProfession("armorer", 1).empty());
    CHECK(trades.forProfession("butcher", 1).empty());
}

TEST_CASE("no jar at all answers nothing rather than falling over") {
    RealVillagerTrades trades{RealVillagerTrades::Jar{}};

    CHECK(trades.forProfession("farmer", 1).empty());
}
