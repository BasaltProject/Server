#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/loot/LootTableJson.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

using namespace basalt::server::world::loot;
namespace rng = basalt::server::random;

namespace {

/// Tables written the way the real ones are: weighted alternatives, a rolled count, a bonus roll
/// that only luck reaches, and the conditions and functions the evaluator answers by fiat.
[[nodiscard]] const std::map<std::string, std::string, std::less<>>& tables() {
    static const std::map<std::string, std::string, std::less<>> built{
        {"simple", R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:diamond"}]}]})"},
        {"counted", R"({"pools":[{"rolls":{"type":"minecraft:uniform","min":2,"max":5},"entries":[
                  {"type":"minecraft:item","name":"minecraft:coal","functions":[
                    {"function":"minecraft:set_count","count":{"type":"minecraft:uniform","min":1,"max":4}}]}]}]})"},
        {"weighted", R"({"pools":[{"rolls":3,"entries":[
                  {"type":"minecraft:item","name":"minecraft:iron_ingot","weight":10},
                  {"type":"minecraft:item","name":"minecraft:gold_ingot","weight":5,"quality":2},
                  {"type":"minecraft:item","name":"minecraft:emerald","weight":1}]}]})"},
        {"alternatives", R"({"pools":[{"rolls":2,"entries":[
                  {"type":"minecraft:alternatives","children":[
                    {"type":"minecraft:item","name":"minecraft:stone","conditions":[
                      {"condition":"minecraft:match_tool"}]},
                    {"type":"minecraft:item","name":"minecraft:cobblestone"}]},
                  {"type":"minecraft:item","name":"minecraft:gravel","weight":3}]}]})"},
        {"chances", R"({"pools":[{"rolls":4,"entries":[
                  {"type":"minecraft:item","name":"minecraft:bone","conditions":[
                    {"condition":"minecraft:random_chance","chance":0.5}]},
                  {"type":"minecraft:item","name":"minecraft:string","conditions":[
                    {"condition":"minecraft:inverted","term":{"condition":"minecraft:killed_by_player"}}]},
                  {"type":"minecraft:item","name":"minecraft:rotten_flesh","conditions":[
                    {"condition":"minecraft:any_of","terms":[
                      {"condition":"minecraft:entity_properties"},
                      {"condition":"minecraft:survives_explosion"}]}]},
                  {"type":"minecraft:item","name":"minecraft:feather","conditions":[
                    {"condition":"minecraft:all_of","terms":[
                      {"condition":"minecraft:killed_by_player"},
                      {"condition":"minecraft:random_chance_with_enchanted_bonus",
                       "unenchanted_chance":0.25}]}]}]}]})"},
        {"explosive", R"({"pools":[{"rolls":2,"conditions":[{"condition":"minecraft:survives_explosion"}],"entries":[
                  {"type":"minecraft:item","name":"minecraft:sand","functions":[
                    {"function":"minecraft:set_count","count":8}]}]}],
                 "functions":[{"function":"minecraft:explosion_decay"}]})"},
        {"gated", R"({"pools":[{"rolls":1,"conditions":[{"condition":"minecraft:match_tool"}],"entries":[
                  {"type":"minecraft:item","name":"minecraft:glass"}]}]})"},
        // Bounds that meet, a fractional count, a negative quality and an alternatives whose first
        // two children both pass: the four shapes the plain tables never reach.
        {"edges", R"({"pools":[{"rolls":{"type":"minecraft:uniform","min":3,"max":3},"entries":[
                  {"type":"minecraft:item","name":"minecraft:clay_ball","weight":8,"quality":-3,"functions":[
                    {"function":"minecraft:set_count","count":2.7}]},
                  {"type":"minecraft:alternatives","weight":20,"children":[
                    {"type":"minecraft:item","name":"minecraft:brick"},
                    {"type":"minecraft:item","name":"minecraft:flint"},
                    {"type":"minecraft:item","name":"minecraft:quartz"}]},
                  {"type":"minecraft:item","name":"minecraft:clay","weight":4,"conditions":[
                    {"condition":"minecraft:random_chance",
                     "chance":{"type":"minecraft:uniform","min":0.5,"max":0.5}}],"functions":[
                    {"function":"minecraft:set_count",
                     "count":{"type":"minecraft:uniform","min":2.5,"max":2.5}}]}]}]})"},
        // Bounds that meet, a fractional count, a negative quality and an alternatives whose first
        // two children both pass: the four shapes the plain tables never reach.
        {"edges", R"({"pools":[{"rolls":{"type":"minecraft:uniform","min":3,"max":3},"entries":[
                  {"type":"minecraft:item","name":"minecraft:clay_ball","weight":8,"quality":-3,"functions":[
                    {"function":"minecraft:set_count","count":2.7}]},
                  {"type":"minecraft:alternatives","children":[
                    {"type":"minecraft:item","name":"minecraft:brick"},
                    {"type":"minecraft:item","name":"minecraft:flint"},
                    {"type":"minecraft:item","name":"minecraft:quartz"}]},
                  {"type":"minecraft:item","name":"minecraft:clay","weight":4,"functions":[
                    {"function":"minecraft:set_count",
                     "count":{"type":"minecraft:uniform","min":2.5,"max":2.5}}]}]}]})"},
        {"bonus", R"({"pools":[{"rolls":1,"bonus_rolls":2,"entries":[
                  {"type":"minecraft:item","name":"minecraft:lapis_lazuli","functions":[
                    {"function":"minecraft:enchanted_count_increase","count":1,"limit":9},
                    {"function":"minecraft:copy_name"},
                    {"function":"minecraft:furnace_smelt"}]}]}]})"},
    };
    return built;
}

struct Run {
    std::size_t drops = 0;
    std::int32_t total = 0;
    std::int32_t after = 0;
    std::int64_t checksum = 0;
};

[[nodiscard]] Run run(std::string_view table, std::int64_t seed, float luck, std::optional<float> explosionRadius) {
    const auto found = tables().find(table);
    REQUIRE(found != tables().end());
    const std::expected<LootTable, LootTableParseError> parsed = lootTableJson::parse(found->second);
    REQUIRE(parsed.has_value());
    rng::RandomSource random = rng::RandomSource::legacy(seed);
    const std::vector<ItemStack> drops = parsed->generate(random, luck, explosionRadius);
    std::uint64_t checksum = 14695981039346656037ULL;
    Run result;
    for (const ItemStack& drop : drops) {
        for (const char letter : drop.item.toString()) {
            checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<unsigned char>(letter))) * 1099511628211ULL;
        }
        checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(drop.count))) * 1099511628211ULL;
        result.total += drop.count;
    }
    result.drops = drops.size();
    result.after = random.nextInt();
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

/// The message a refused table reports, with spaces joined the way the oracle prints them.
[[nodiscard]] std::string refusal(std::string_view json) {
    const std::expected<LootTable, LootTableParseError> parsed = lootTableJson::parse(json);
    std::string message = parsed.has_value() ? std::string{"accepted"} : parsed.error().message;
    for (char& letter : message) {
        if (letter == ' ') {
            letter = '_';
        }
    }
    return message;
}

void check(const Run& result, std::size_t drops, std::int32_t total, std::int32_t after, std::int64_t checksum) {
    CHECK(result.drops == drops);
    CHECK(result.total == total);
    CHECK(result.after == after);
    CHECK(result.checksum == checksum);
}

} // namespace

TEST_CASE("the loot tables roll what the frozen kotlin rolls") {
    SUBCASE("simple") { check(run("simple", 1234, 0.0F, std::nullopt), 1, 1, -1517918040, -7908905330299729931LL); }

    SUBCASE("counted") { check(run("counted", 1234, 0.0F, std::nullopt), 4, 11, 1362132786, -3999476953389317862LL); }

    SUBCASE("weighted") { check(run("weighted", 1234, 0.0F, std::nullopt), 3, 3, 1019800440, 1321312623607189017LL); }

    // Luck moves an entry's weight by its quality, which is the only thing that reads it.
    SUBCASE("weightedLucky") {
        check(run("weighted", 1234, 3.0F, std::nullopt), 3, 3, 1019800440, -4609031729764813871LL);
    }

    SUBCASE("alternatives") {
        check(run("alternatives", 1234, 0.0F, std::nullopt), 2, 2, -208917030, -1915672080275547317LL);
    }

    SUBCASE("chances") { check(run("chances", 1234, 0.0F, std::nullopt), 4, 4, 64950077, 1545314548558635489LL); }

    SUBCASE("explosive") {
        check(run("explosive", 1234, 0.0F, std::nullopt), 2, 16, -1517918040, 5556686949048168055LL);
    }

    SUBCASE("explosiveBlast") { check(run("explosive", 1234, 0.0F, 4.0F), 2, 5, -228236395, -7514839389085713806LL); }

    SUBCASE("gated") { check(run("gated", 1234, 0.0F, std::nullopt), 0, 0, -1517918040, -3750763034362895579LL); }

    SUBCASE("bonus") { check(run("bonus", 1234, 0.0F, std::nullopt), 1, 1, -1517918040, -1118564091388105360LL); }

    // Only luck reaches a bonus roll, so this is the one case where it fires at all.
    SUBCASE("bonusLucky") { check(run("bonus", 1234, 4.0F, std::nullopt), 9, 9, -1517918040, 2116028971200313784LL); }

    SUBCASE("countedOther") {
        check(run("counted", 77, 0.0F, std::nullopt), 4, 10, 1298288402, -8420051398115059829LL);
    }

    SUBCASE("edges") { check(run("edges", 1234, 0.0F, std::nullopt), 3, 6, 1968097058, 3863140543591701521LL); }

    // Luck three times a negative quality drives that entry's weight below zero.
    SUBCASE("edgesLucky") { check(run("edges", 1234, 3.0F, std::nullopt), 3, 4, 1362132786, 6191627497126115805LL); }

    // A wide enough blast swallows a whole stack, which is then not a drop at all.
    SUBCASE("explosiveWide") { check(run("explosive", 1234, 0.0F, 12.0F), 1, 1, -228236395, -8053036407505117797LL); }

    // A type this build does not implement is named rather than silently producing wrong loot.
    SUBCASE("unknownEntry") {
        CHECK(refusal(R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:tag","name":"minecraft:logs"}]}]})") ==
              "Unsupported_loot_pool_entry_type:_minecraft:tag");
    }

    SUBCASE("unknownCondition") {
        CHECK(refusal(R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:stone",
               "conditions":[{"condition":"minecraft:enchantment_active_check"}]}]}]})") ==
              "Unsupported_loot_condition_type:_minecraft:enchantment_active_check");
    }

    SUBCASE("unknownFunction") {
        CHECK(refusal(R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:stone",
               "functions":[{"function":"minecraft:enchant_randomly"}]}]}]})") ==
              "Unsupported_loot_function_type:_minecraft:enchant_randomly");
    }

    SUBCASE("unknownProvider") {
        CHECK(refusal(R"({"pools":[{"rolls":{"type":"minecraft:binomial","n":3,"p":0.5},"entries":[
               {"type":"minecraft:item","name":"minecraft:stone"}]}]})") ==
              "Unsupported_number_provider_type:_minecraft:binomial");
    }
}

TEST_CASE("a block table rolls by the state of the block that broke") {
    // parity: the shape every door, slab and bed table in the jar has.
    const std::expected<LootTable, LootTableParseError> parsed = lootTableJson::parse(
        R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:oak_door","conditions":[
           {"condition":"minecraft:block_state_property","block":"minecraft:oak_door",
            "properties":{"half":"lower"}}]}]}]})");
    REQUIRE(parsed.has_value());

    rng::RandomSource random = rng::RandomSource::legacy(1234);
    const LootContext lower{.blockState = {{"facing", "east"}, {"half", "lower"}, {"open", "false"}}};
    CHECK(parsed->generate(random, 0.0F, std::nullopt, lower).size() == 1);

    const LootContext upper{.blockState = {{"facing", "east"}, {"half", "upper"}, {"open", "false"}}};
    CHECK(parsed->generate(random, 0.0F, std::nullopt, upper).empty());

    // A roll with no block behind it matches no state, which is what `/loot loot` on a table asks.
    CHECK(parsed->generate(random).empty());
}

TEST_CASE("the rest of what a block table carries: a clamp, a fortune bonus and a chance table") {
    rng::RandomSource random = rng::RandomSource::legacy(1234);

    // parity: `LimitCount` clamps whatever the functions before it left.
    const std::expected<LootTable, LootTableParseError> clamped = lootTableJson::parse(
        R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:wheat","functions":[
           {"function":"minecraft:set_count","count":9},
           {"function":"minecraft:limit_count","limit":{"min":1,"max":3}}]}]}]})");
    REQUIRE(clamped.has_value());
    const std::vector<ItemStack> kept = clamped->generate(random);
    REQUIRE(kept.size() == 1);
    CHECK(kept.front().count == 3);

    // parity: `ApplyBonusCount` at enchantment level zero, which is what a block broken with no tool
    // gets: the count it was already given.
    const std::expected<LootTable, LootTableParseError> bonus = lootTableJson::parse(
        R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:redstone","functions":[
           {"function":"minecraft:set_count","count":4},
           {"function":"minecraft:apply_bonus","enchantment":"minecraft:fortune",
            "formula":"minecraft:uniform_bonus_count","parameters":{"bonusMultiplier":1}}]}]}]})");
    REQUIRE(bonus.has_value());
    const std::vector<ItemStack> unchanged = bonus->generate(random);
    REQUIRE(unchanged.size() == 1);
    CHECK(unchanged.front().count == 4);

    // parity: `BonusLevelTableCondition` reads its table at the level of the tool, which is zero.
    const std::expected<LootTable, LootTableParseError> never = lootTableJson::parse(
        R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:apple","conditions":[
           {"condition":"minecraft:table_bonus","enchantment":"minecraft:fortune",
            "chances":[0.0,1.0,1.0]}]}]}]})");
    REQUIRE(never.has_value());
    CHECK(never->generate(random).empty());
}

TEST_CASE("a function only runs when its own conditions pass") {
    // parity: the candle table, three `set_count`s each gated on how many candles the block had.
    const std::expected<LootTable, LootTableParseError> parsed = lootTableJson::parse(
        R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:candle","functions":[
           {"function":"minecraft:set_count","count":2,"conditions":[
             {"condition":"minecraft:block_state_property","block":"minecraft:candle",
              "properties":{"candles":"2"}}]},
           {"function":"minecraft:set_count","count":3,"conditions":[
             {"condition":"minecraft:block_state_property","block":"minecraft:candle",
              "properties":{"candles":"3"}}]}]}]}]})");
    REQUIRE(parsed.has_value());

    rng::RandomSource random = rng::RandomSource::legacy(1234);
    const LootContext three{.blockState = {{"candles", "3"}, {"lit", "false"}}};
    const std::vector<ItemStack> lit = parsed->generate(random, 0.0F, std::nullopt, three);
    REQUIRE(lit.size() == 1);
    CHECK(lit.front().count == 3);

    // A single candle matches neither, so the stack keeps the one it was made with.
    const LootContext one{.blockState = {{"candles", "1"}, {"lit", "false"}}};
    const std::vector<ItemStack> alone = parsed->generate(random, 0.0F, std::nullopt, one);
    REQUIRE(alone.size() == 1);
    CHECK(alone.front().count == 1);
}
