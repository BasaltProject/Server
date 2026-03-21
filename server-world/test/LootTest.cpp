#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/loot/LootTableJson.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <doctest.h>

#include <bit>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace basalt::server::world::loot;
namespace rng = basalt::server::random;

namespace {

/// Tables written the way the real ones are: weighted alternatives, a rolled count, a bonus roll
/// that only luck reaches, composites, holder sets and the conditions the evaluator answers by fiat.
[[nodiscard]] const std::map<std::string, std::string, std::less<>>& tables() {
    static const std::map<std::string, std::string, std::less<>> built{
        {"simple", R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:diamond"}]}]})"},
        {"counted", R"({"pools":[{"rolls":{"type":"minecraft:uniform","min":2,"max":5},"entries":[
                  {"type":"minecraft:item","name":"minecraft:coal",
                   "modifier":{"type":"minecraft:set_count","count":{"type":"minecraft:uniform","min":1,"max":4}}}]}]})"},
        {"weighted", R"({"pools":[{"rolls":3,"entries":[
                  {"type":"minecraft:item","name":"minecraft:iron_ingot","weight":10},
                  {"type":"minecraft:item","name":"minecraft:gold_ingot","weight":5,"quality":2},
                  {"type":"minecraft:item","name":"minecraft:emerald","weight":1}]}]})"},
        {"alternatives", R"({"pools":[{"rolls":2,"entries":[
                  {"type":"minecraft:alternatives","children":[
                    {"type":"minecraft:item","name":"minecraft:stone","condition":{"type":"minecraft:match_tool"}},
                    {"type":"minecraft:item","name":"minecraft:cobblestone"}]},
                  {"type":"minecraft:item","name":"minecraft:gravel","weight":3}]}]})"},
        {"chances", R"({"pools":[{"rolls":4,"entries":[
                  {"type":"minecraft:item","name":"minecraft:bone",
                   "condition":{"type":"minecraft:random_chance","chance":0.5}},
                  {"type":"minecraft:item","name":"minecraft:string",
                   "condition":{"type":"minecraft:inverted","term":{"type":"minecraft:killed_by_player"}}},
                  {"type":"minecraft:item","name":"minecraft:rotten_flesh","condition":{"type":"minecraft:any_of",
                   "terms":[{"type":"minecraft:entity_properties","entity":"this"},
                            {"type":"minecraft:survives_explosion"}]}},
                  {"type":"minecraft:item","name":"minecraft:feather","condition":{"type":"minecraft:all_of",
                   "terms":[{"type":"minecraft:killed_by_player"},
                            {"type":"minecraft:random_chance","chance":0.25}]}}]}]})"},
        {"explosive", R"({"pools":[{"rolls":2,"condition":{"type":"minecraft:survives_explosion"},"entries":[
                  {"type":"minecraft:item","name":"minecraft:sand",
                   "modifier":{"type":"minecraft:set_count","count":8}}]}],
                 "modifier":{"type":"minecraft:explosion_decay"}})"},
        {"gated", R"({"pools":[{"rolls":1,"condition":{"type":"minecraft:match_tool"},"entries":[
                  {"type":"minecraft:item","name":"minecraft:glass"}]}]})"},
        // A fractional count, bounds that meet, a negative quality and an alternatives whose first two
        // children both pass: the shapes the plain tables never reach. `Codec.INT` truncates 2.7.
        {"edges", R"({"pools":[{"rolls":{"type":"minecraft:uniform","min":3,"max":3},"entries":[
                  {"type":"minecraft:item","name":"minecraft:clay_ball","weight":8,"quality":-3,
                   "modifier":{"type":"minecraft:set_count","count":2.7}},
                  {"type":"minecraft:alternatives","children":[
                    {"type":"minecraft:item","name":"minecraft:brick"},
                    {"type":"minecraft:item","name":"minecraft:flint"},
                    {"type":"minecraft:item","name":"minecraft:quartz"}]},
                  {"type":"minecraft:item","name":"minecraft:clay","weight":4,
                   "condition":{"type":"minecraft:random_chance",
                                "chance":{"type":"minecraft:uniform","min":0.5,"max":0.5}},
                   "modifier":{"type":"minecraft:set_count",
                               "count":{"type":"minecraft:uniform","min":2.5,"max":2.5}}}]}]})"},
        {"bonus", R"({"pools":[{"rolls":1,"bonus_rolls":2,"entries":[
                  {"type":"minecraft:item","name":"minecraft:lapis_lazuli","modifier":[
                    {"type":"minecraft:enchanted_count_increase","enchantment":"minecraft:looting","count":1,
                     "limit":9},
                    {"type":"minecraft:copy_name","source":"block_entity"},
                    {"type":"minecraft:furnace_smelt"}]}]}]})"},
        // parity: 26.3's `group` and `sequence` composites, with a modifier on the group.
        {"composed", R"({"pools":[{"rolls":4,"entries":[
                  {"type":"minecraft:group","children":[
                    {"type":"minecraft:item","name":"minecraft:stick",
                     "condition":{"type":"minecraft:random_chance","chance":0.5}},
                    {"type":"minecraft:item","name":"minecraft:bone"}],
                   "modifier":{"type":"minecraft:set_count","count":{"type":"minecraft:uniform","min":1,"max":3}}},
                  {"type":"minecraft:sequence","children":[
                    {"type":"minecraft:item","name":"minecraft:dirt",
                     "condition":{"type":"minecraft:random_chance","chance":0.3}},
                    {"type":"minecraft:item","name":"minecraft:stone"}]}]}]})"},
        // An int rolls provider, a composed bonus, a pool modifier, an expanded item list, an inline
        // nested table under an entry modifier, and a weighted count.
        {"layered", R"({"pools":[{"rolls":{"type":"minecraft:binomial","n":5,"p":0.6},
                  "bonus_rolls":{"type":"minecraft:mul","inputs":[0.5,{"type":"minecraft:uniform","min":1,"max":3}]},
                  "modifier":[{"type":"minecraft:limit_count",
                               "limit":{"max":{"type":"minecraft:uniform","min":1,"max":2}}}],
                  "entries":[
                    {"type":"minecraft:tag","items":["minecraft:apple","minecraft:carrot","minecraft:potato"],
                     "expand":true,"weight":2,"modifier":{"type":"minecraft:set_count",
                     "count":{"type":"minecraft:weighted_list","distribution":[{"data":1,"weight":1},
                       {"data":{"type":"minecraft:uniform","min":3,"max":6},"weight":3}]}}},
                    {"type":"minecraft:loot_table","value":{"pools":[{"rolls":2,"entries":[
                       {"type":"minecraft:item","name":"minecraft:bread","modifier":{"type":"minecraft:set_count",
                        "count":{"type":"minecraft:add","inputs":[1,{"type":"minecraft:uniform","min":0,"max":2}]},
                        "condition":{"type":"minecraft:random_chance","chance":0.7}}}]}]},
                     "modifier":{"type":"minecraft:set_count","count":2,"add":true}},
                    {"type":"minecraft:tag","items":["minecraft:beef","minecraft:mutton"]}]}]})"},
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
    // The frozen oracle rolled every table as a kill by a player.
    const std::vector<ItemStack> drops =
        parsed->generate(random, luck, explosionRadius, LootContext{.killedByPlayer = true});
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

// Every golden below is what the 26.3 server rolls for the same table, seed, luck and blast, killed
// by a player. The two whose blast draws differ from 26.2 are `explosiveBlast` and `explosiveWide`.
TEST_CASE("the loot tables roll what the official server rolls") {
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

    // parity: `ExplosionCondition` draws once per pool roll, and seed 1234 loses both draws.
    SUBCASE("explosiveMissed") { check(run("explosive", 1234, 0.0F, 4.0F), 0, 0, 1115789266, -3750763034362895579LL); }

    SUBCASE("explosiveBlast") { check(run("explosive", 5555, 0.0F, 4.0F), 1, 4, -570839267, -8053033108970233164LL); }

    // A wide enough blast swallows a whole stack, which is then not a drop at all.
    SUBCASE("explosiveWide") { check(run("explosive", 31337, 0.0F, 12.0F), 1, 1, 140908551, -8053036407505117797LL); }

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

    SUBCASE("composed") { check(run("composed", 1234, 0.0F, std::nullopt), 4, 7, -1643623517, -6817681030838137439LL); }

    SUBCASE("layered") { check(run("layered", 1234, 0.0F, std::nullopt), 2, 2, 1957992572, 5901802531632482912LL); }

    SUBCASE("layeredLucky") { check(run("layered", 99, 2.5F, std::nullopt), 5, 6, -992928519, 6165598494076272865LL); }

    // A type this build does not implement is named rather than silently producing wrong loot.
    SUBCASE("unknownEntry") {
        CHECK(refusal(R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:slots","slot_source":"x"}]}]})") ==
              "Unsupported_loot_pool_entry_type:_minecraft:slots");
    }

    SUBCASE("unknownFunction") {
        CHECK(refusal(R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:stone",
               "modifier":{"type":"minecraft:set_lore","lore":[]}}]}]})") ==
              "Unsupported_loot_function_type:_minecraft:set_lore");
    }

    SUBCASE("unknownProvider") {
        CHECK(refusal(R"({"pools":[{"rolls":{"type":"minecraft:sum","inputs":[1]},"entries":[
               {"type":"minecraft:item","name":"minecraft:stone"}]}]})") ==
              "Unsupported_number_provider_type:_minecraft:sum");
    }

    // parity: 26.3 made a condition a holder, so a name needs the registry to read it from.
    SUBCASE("unresolvedReference") {
        CHECK(refusal(R"({"pools":[{"rolls":1,"condition":"minecraft:tool/can_shear","entries":[
               {"type":"minecraft:item","name":"minecraft:stone"}]}]})") ==
              "Unresolved_predicate_reference:_minecraft:tool/can_shear");
    }
}

namespace {

/// A provider rolled `times` over one random, then the next int that random gives.
[[nodiscard]] std::vector<std::int32_t> rollInts(std::string_view json, std::int64_t seed, std::int32_t times,
                                                 const LootContext& context = {}) {
    const std::expected<basalt::wire::common::JsonValue, basalt::wire::common::JsonError> value =
        basalt::wire::common::parseJson(json);
    REQUIRE(value.has_value());
    const std::expected<IntProvider, LootTableParseError> provider = lootTableJson::parseIntProvider(*value);
    REQUIRE(provider.has_value());
    rng::RandomSource random = rng::RandomSource::legacy(seed);
    std::vector<std::int32_t> rolled;
    for (std::int32_t index = 0; index < times; ++index) {
        rolled.push_back(provider->get(random, context));
    }
    rolled.push_back(random.nextInt());
    return rolled;
}

/// The same for a fraction, as raw bits so a one-ulp difference fails.
[[nodiscard]] std::vector<std::uint32_t> rollFloats(std::string_view json, std::int64_t seed, std::int32_t times) {
    const std::expected<basalt::wire::common::JsonValue, basalt::wire::common::JsonError> value =
        basalt::wire::common::parseJson(json);
    REQUIRE(value.has_value());
    const std::expected<FloatProvider, LootTableParseError> provider = lootTableJson::parseFloatProvider(*value);
    REQUIRE(provider.has_value());
    rng::RandomSource random = rng::RandomSource::legacy(seed);
    std::vector<std::uint32_t> rolled;
    for (std::int32_t index = 0; index < times; ++index) {
        rolled.push_back(std::bit_cast<std::uint32_t>(provider->get(random, {})));
    }
    rolled.push_back(static_cast<std::uint32_t>(random.nextInt()));
    return rolled;
}

/// A condition tested `times` over one random, then the next int.
[[nodiscard]] std::pair<std::vector<bool>, std::int32_t> testTimes(std::string_view json, std::int64_t seed,
                                                                   std::int32_t times) {
    const std::expected<basalt::wire::common::JsonValue, basalt::wire::common::JsonError> value =
        basalt::wire::common::parseJson(json);
    REQUIRE(value.has_value());
    const std::expected<LootCondition, LootTableParseError> condition = lootTableJson::parseCondition(*value);
    REQUIRE(condition.has_value());
    rng::RandomSource random = rng::RandomSource::legacy(seed);
    std::vector<bool> results;
    for (std::int32_t index = 0; index < times; ++index) {
        results.push_back(condition->test(random, {}));
    }
    return {results, random.nextInt()};
}

/// A compost chance shaped as 26.3 writes one: one layer into an empty composter, else a weighed roll.
constexpr std::string_view kCompostLow =
    R"({"type":"minecraft:number_dispatcher","cases":[{"condition":{"type":"minecraft:match_block",
        "blocks":"minecraft:composter","state":{"level":"0"}},"value":1}],
        "default":{"type":"minecraft:weighted_list","distribution":[{"data":1,"weight":30},{"data":0,"weight":70}]}})";

} // namespace

// Every golden below is what the 26.3 server's own providers answer for the same JSON and seed.
TEST_CASE("the context providers roll what the official server rolls") {
    using basalt::wire::common::Identifier;
    // parity: a composter at level zero takes a layer without a draw; above it, one `nextInt(100)`.
    const LootContext filling{.blockState = {{"level", "3"}}, .block = Identifier::vanilla("composter")};
    CHECK(rollInts(kCompostLow, 99, 6, filling) == std::vector<std::int32_t>{0, 0, 1, 1, 1, 0, -571055948});
    const LootContext empty{.blockState = {{"level", "0"}}, .block = Identifier::vanilla("composter")};
    CHECK(rollInts(kCompostLow, 99, 3, empty) == std::vector<std::int32_t>{1, 1, 1, -1192035722});

    CHECK(rollInts(R"({"type":"minecraft:weighted_list","distribution":[{"data":1,"weight":3},
                      {"data":{"type":"minecraft:uniform","min":5,"max":9},"weight":2}]})",
                   42, 5) == std::vector<std::int32_t>{1, 8, 5, 1, 1, -1255373459});
    CHECK(rollInts(R"({"type":"minecraft:binomial","n":{"type":"minecraft:uniform","min":3,"max":6},"p":0.4})", 7, 3) ==
          std::vector<std::int32_t>{2, 2, 1, -306024682});
    CHECK(rollInts(R"({"type":"minecraft:number_dispatcher","cases":[{"condition":{"type":"minecraft:random_chance",
                      "chance":0.5},"value":{"type":"minecraft:uniform","min":10,"max":20}}],
                      "default":{"type":"minecraft:negate","input":{"type":"minecraft:pow","base":2,
                      "exponent":{"type":"minecraft:uniform","min":0,"max":5}}}})",
                   3, 4) == std::vector<std::int32_t>{-4, 12, 12, -16, 976394918});
    CHECK(rollInts(R"({"type":"minecraft:from_float","input":{"type":"minecraft:mul","inputs":[
                      {"type":"minecraft:uniform","min":-2,"max":2},1000]}})",
                   12, 3) == std::vector<std::int32_t>{919, -1760, -899, 942660266});
    CHECK(rollInts(R"({"type":"minecraft:add","inputs":[{"type":"minecraft:floor_div","left":-7,"right":2},
                      {"type":"minecraft:mul","inputs":[100,{"type":"minecraft:floor_mod","left":-7,"right":3}]},
                      {"type":"minecraft:mul","inputs":[10000,{"type":"minecraft:mod","left":-7,"right":3}]}]})",
                   1, 1) == std::vector<std::int32_t>{-9804, -1155869325});

    // parity: `getInt` answers 0 for an overflow and for a zero divisor, having drawn what came first.
    CHECK(rollInts(R"({"type":"minecraft:add","inputs":[{"type":"minecraft:mul","inputs":[65536,65536]},
                      {"type":"minecraft:uniform","min":1,"max":6}]})",
                   1, 1) == std::vector<std::int32_t>{0, -1155869325});
    CHECK(rollInts(R"({"type":"minecraft:add","inputs":[{"type":"minecraft:uniform","min":1,"max":6},
                      {"type":"minecraft:div","left":{"type":"minecraft:uniform","min":1,"max":6},"right":0},
                      {"type":"minecraft:uniform","min":1,"max":6}]})",
                   1, 1) == std::vector<std::int32_t>{0, 1761283695});

    CHECK(rollFloats(R"({"type":"minecraft:add","inputs":[{"type":"minecraft:uniform","min":0,"max":1},
                        {"type":"minecraft:mul","inputs":[2.5,{"type":"minecraft:sin","input":1.0}]}]})",
                     42, 4) == std::vector<std::uint32_t>{0x40353241, 0x400a217c, 0x40325bc9, 0x4009b34a,
                                                          static_cast<std::uint32_t>(1325939940)});
    CHECK(rollFloats(R"({"type":"minecraft:round","input":{"type":"minecraft:div",
                        "left":{"type":"minecraft:uniform","min":-10,"max":10},"right":3}})",
                     11, 3) ==
          std::vector<std::uint32_t>{0x40000000, 0x3f800000, 0x00000000, static_cast<std::uint32_t>(919154311)});
    CHECK(rollFloats(R"({"type":"minecraft:add","inputs":[{"type":"minecraft:ceil","input":{"type":"minecraft:uniform",
                        "min":-3,"max":3}},{"type":"minecraft:mul","inputs":[10,{"type":"minecraft:truncate",
                        "input":-2.5}]},{"type":"minecraft:length","inputs":[3,4]},{"type":"minecraft:cos",
                        "input":{"type":"minecraft:from_int","input":{"type":"minecraft:uniform","min":0,"max":3}}},
                        {"type":"minecraft:mod","left":7.5,"right":{"type":"minecraft:max","inputs":[-2,2]}}]})",
                     3,
                     2) == std::vector<std::uint32_t>{0xc13ea844, 0xc183eb80, static_cast<std::uint32_t>(288278256)});
}

TEST_CASE("the value checks and the chance read their providers in vanilla's order") {
    CHECK(testTimes(R"({"type":"minecraft:int_value_check","value":{"type":"minecraft:uniform","min":0,"max":10},
                       "test":{"min":3,"max":{"type":"minecraft:uniform","min":4,"max":8}}})",
                    8, 5) == std::pair{std::vector<bool>{true, false, false, false, false}, 924168001});
    CHECK(testTimes(R"({"type":"minecraft:float_value_check","value":{"type":"minecraft:uniform","min":0,"max":1},
                       "test":{"type":"minecraft:uniform","min":0,"max":1}})",
                    8, 4) == std::pair{std::vector<bool>{false, false, false, false}, -1893091589});
    // parity: `getFloat` turns a NaN chance into 0, and the draw still happens.
    CHECK(testTimes(R"({"type":"minecraft:random_chance","chance":{"type":"minecraft:sqrt","input":-1}})", 5, 2) ==
          std::pair{std::vector<bool>{false, false}, 379066948});
}

TEST_CASE("named predicates, modifiers and providers are read once from their registries") {
    using basalt::wire::common::Identifier;
    std::int32_t reads = 0;
    LootReferences references{[&reads](std::string_view registry, const Identifier& id) -> std::optional<std::string> {
        ++reads;
        const std::string key = std::string{registry} + "/" + id.path();
        if (key == "predicate/cold") {
            return R"({"type":"minecraft:match_block","blocks":["minecraft:snow","minecraft:ice"]})";
        }
        if (key == "item_modifier/double") {
            return R"([{"type":"minecraft:set_count","count":"minecraft:two"},
                      {"type":"minecraft:set_count","count":{"type":"minecraft:mul","inputs":[2,"minecraft:two"]},
                       "add":true,"condition":"minecraft:cold"}])";
        }
        if (key == "context_int_provider/two") {
            return "2";
        }
        if (key == "predicate/loop") {
            return R"({"type":"minecraft:inverted","term":"minecraft:loop"})";
        }
        return std::nullopt;
    }};
    const std::expected<LootTable, LootTableParseError> table = lootTableJson::parse(
        R"({"pools":[{"rolls":"minecraft:two","entries":[{"type":"minecraft:item","name":"minecraft:snowball",
           "modifier":"minecraft:double"}]}]})",
        &references);
    REQUIRE(table.has_value());
    const std::int32_t firstReads = reads;
    REQUIRE(lootTableJson::parse(R"({"pools":[{"rolls":1,"condition":"minecraft:cold","entries":[]}]})", &references)
                .has_value());
    CHECK(reads == firstReads);

    rng::RandomSource random = rng::RandomSource::legacy(1);
    const std::vector<ItemStack> warm = table->generate(
        random, 0.0F, std::nullopt, LootContext{.blockState = {{"a", "b"}}, .block = Identifier::vanilla("dirt")});
    REQUIRE(warm.size() == 2);
    CHECK(warm.front().count == 2);
    const std::vector<ItemStack> cold = table->generate(
        random, 0.0F, std::nullopt, LootContext{.blockState = {{"a", "b"}}, .block = Identifier::vanilla("ice")});
    REQUIRE(cold.size() == 2);
    CHECK(cold.front().count == 6);

    CHECK(refusal(R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:stone",
           "condition":"minecraft:nowhere"}]}]})") == "Unresolved_predicate_reference:_minecraft:nowhere");
    const std::expected<LootTable, LootTableParseError> cyclic =
        lootTableJson::parse(R"({"pools":[{"rolls":1,"condition":"minecraft:loop","entries":[]}]})", &references);
    REQUIRE_FALSE(cyclic.has_value());
    CHECK(cyclic.error().message == "Cyclic predicate reference: minecraft:loop");
}

TEST_CASE("a block predicate reads the block, exact values and ranges") {
    using basalt::wire::common::Identifier;
    const std::expected<basalt::wire::common::JsonValue, basalt::wire::common::JsonError> json =
        basalt::wire::common::parseJson(R"({"type":"minecraft:match_block","blocks":"minecraft:composter",
            "state":{"level":{"min":"2","max":"6"}}})");
    REQUIRE(json.has_value());
    const std::expected<LootCondition, LootTableParseError> condition = lootTableJson::parseCondition(*json);
    REQUIRE(condition.has_value());
    rng::RandomSource random = rng::RandomSource::legacy(1);
    const auto at = [&](std::string_view block, std::string_view level) {
        return condition->test(random, LootContext{.blockState = {{"level", std::string{level}}},
                                                   .block = Identifier::vanilla(std::string{block})});
    };
    CHECK(at("composter", "2"));
    CHECK(at("composter", "6"));
    CHECK_FALSE(at("composter", "1"));
    CHECK_FALSE(at("composter", "7"));
    CHECK_FALSE(at("barrel", "4"));
}

TEST_CASE("a block table rolls by the state of the block that broke") {
    // parity: the shape every door, slab and bed table in the jar has.
    const std::expected<LootTable, LootTableParseError> parsed = lootTableJson::parse(
        R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:oak_door","condition":
           {"type":"minecraft:match_block","blocks":"minecraft:oak_door","state":{"half":"lower"}}}]}]})");
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
        R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:wheat","modifier":[
           {"type":"minecraft:set_count","count":9},
           {"type":"minecraft:limit_count","limit":{"min":1,"max":3}}]}]}]})");
    REQUIRE(clamped.has_value());
    const std::vector<ItemStack> kept = clamped->generate(random);
    REQUIRE(kept.size() == 1);
    CHECK(kept.front().count == 3);

    // parity: `ApplyBonusCount` without a tool leaves the count alone and draws nothing.
    const std::expected<LootTable, LootTableParseError> bonus = lootTableJson::parse(
        R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:redstone","modifier":[
           {"type":"minecraft:set_count","count":4},
           {"type":"minecraft:apply_bonus","enchantment":"minecraft:fortune",
            "formula":"minecraft:uniform_bonus_count","parameters":{"bonusMultiplier":1}}]}]}]})");
    REQUIRE(bonus.has_value());
    const std::vector<ItemStack> unchanged = bonus->generate(random);
    REQUIRE(unchanged.size() == 1);
    CHECK(unchanged.front().count == 4);

    // parity: `BonusLevelTableCondition` reads its table at the level of the tool, zero without one.
    const std::expected<LootTable, LootTableParseError> never = lootTableJson::parse(
        R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:apple","condition":
           {"type":"minecraft:table_bonus","enchantment":"minecraft:fortune","chances":[0.0,1.0,1.0]}}]}]})");
    REQUIRE(never.has_value());
    CHECK(never->generate(random).empty());
}

TEST_CASE("a function only runs when its own condition passes") {
    // parity: the candle table, `set_count`s each gated on how many candles the block had.
    const std::expected<LootTable, LootTableParseError> parsed = lootTableJson::parse(
        R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:candle","modifier":[
           {"type":"minecraft:set_count","count":2,"condition":
             {"type":"minecraft:match_block","blocks":"minecraft:candle","state":{"candles":"2"}}},
           {"type":"minecraft:set_count","count":3,"condition":
             {"type":"minecraft:match_block","blocks":"minecraft:candle","state":{"candles":"3"}}}]}]}]})");
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

TEST_CASE("an empty entity predicate asks only whether somebody is behind the break") {
    // parity: the snow and chorus flower tables, the two that carry one.
    const std::expected<LootTable, LootTableParseError> parsed = lootTableJson::parse(
        R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:snowball"}],
           "condition":{"type":"minecraft:entity_properties","entity":"this","predicate":{}}}]})");
    REQUIRE(parsed.has_value());

    rng::RandomSource random = rng::RandomSource::legacy(1234);
    const LootContext byHand{.hasEntity = true};
    CHECK(parsed->generate(random, 0.0F, std::nullopt, byHand).size() == 1);
    // A block a neighbour knocked down has nobody behind it, and snow broken that way leaves nothing.
    CHECK(parsed->generate(random).empty());

    // A predicate with anything in it is a question this evaluator cannot answer, and stays a no.
    const std::expected<LootTable, LootTableParseError> fussy = lootTableJson::parse(
        R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:snowball"}],
           "condition":{"type":"minecraft:entity_properties","entity":"this",
                        "predicate":{"type":"minecraft:player"}}}]})");
    REQUIRE(fussy.has_value());
    CHECK(fussy->generate(random, 0.0F, std::nullopt, byHand).empty());
}

TEST_CASE("a nested table rolls on the same random, and the open-water question gates the treasure") {
    // parity: `gameplay/fishing`, whose entries are other tables and whose treasure needs open water.
    const std::expected<LootTable, LootTableParseError> fishing = lootTableJson::parse(
        R"({"pools":[{"rolls":1,"entries":[
             {"type":"minecraft:loot_table","value":"minecraft:gameplay/fishing/fish","weight":85},
             {"type":"minecraft:loot_table","value":"minecraft:gameplay/fishing/treasure","weight":5,"quality":2,
              "condition":{"type":"minecraft:entity_properties","entity":"this",
                           "predicate":{"minecraft:type_specific/fishing_hook":{"in_open_water":true}}}}]}]})");
    const std::expected<LootTable, LootTableParseError> fish =
        lootTableJson::parse(R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:cod"}]}]})");
    const std::expected<LootTable, LootTableParseError> treasure = lootTableJson::parse(
        R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:nautilus_shell"}]}]})");
    REQUIRE(fishing.has_value());
    REQUIRE(fish.has_value());
    REQUIRE(treasure.has_value());

    LootContext context;
    context.tables = [&](const basalt::wire::common::Identifier& id) -> const LootTable* {
        if (id.path() == "gameplay/fishing/fish") {
            return &*fish;
        }
        return id.path() == "gameplay/fishing/treasure" ? &*treasure : nullptr;
    };
    rng::RandomSource random = rng::RandomSource::legacy(77);
    context.inOpenWater = false;
    for (std::int32_t roll = 0; roll < 50; ++roll) {
        const std::vector<ItemStack> caught = fishing->generate(random, 0.0F, std::nullopt, context);
        REQUIRE(caught.size() == 1);
        CHECK(caught.front().item.path() == "cod");
    }
    // Luck makes the quality-two treasure heavy enough to come up among a few hundred rolls.
    context.inOpenWater = true;
    std::int32_t shells = 0;
    for (std::int32_t roll = 0; roll < 300; ++roll) {
        for (const ItemStack& caught : fishing->generate(random, 20.0F, std::nullopt, context)) {
            shells += caught.item.path() == "nautilus_shell" ? 1 : 0;
        }
    }
    CHECK(shells > 50);
}

TEST_CASE("the tool decides silk touch, shears and fortune") {
    // parity: the shape of every ore table: silk touch keeps the block, otherwise the ore drops. The
    // silk touch test is a named predicate, as 26.3 writes it.
    LootReferences references{
        [](std::string_view registry, const basalt::wire::common::Identifier& id) -> std::optional<std::string> {
            if (registry != "predicate" || id.path() != "tool/can_silk_touch") {
                return std::nullopt;
            }
            return R"({"type":"minecraft:match_tool","predicate":{"predicates":{"minecraft:enchantments":[
                  {"enchantments":"minecraft:silk_touch","levels":{"min":1}}]}}})";
        }};
    const std::expected<LootTable, LootTableParseError> ore = lootTableJson::parse(
        R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:alternatives","children":[
           {"type":"minecraft:item","name":"minecraft:diamond_ore","condition":"minecraft:tool/can_silk_touch"},
           {"type":"minecraft:item","name":"minecraft:diamond","modifier":{"type":"minecraft:apply_bonus",
            "enchantment":"minecraft:fortune","formula":"minecraft:ore_drops"}}]}]}]})",
        &references);
    REQUIRE(ore.has_value());
    rng::RandomSource random = rng::RandomSource::legacy(9);
    LootContext silk{.tool = basalt::wire::common::Identifier::vanilla("diamond_pickaxe")};
    silk.toolEnchantments.emplace("minecraft:silk_touch", 1);
    const std::vector<ItemStack> kept = ore->generate(random, 0.0F, std::nullopt, silk);
    REQUIRE(kept.size() == 1);
    CHECK(kept.front().item.path() == "diamond_ore");

    LootContext plain{.tool = basalt::wire::common::Identifier::vanilla("diamond_pickaxe")};
    const std::vector<ItemStack> mined = ore->generate(random, 0.0F, std::nullopt, plain);
    REQUIRE(mined.size() == 1);
    CHECK(mined.front().item.path() == "diamond");
    CHECK(mined.front().count == 1);

    // Fortune three multiplies by one to four, never less than the base.
    LootContext lucky{.tool = basalt::wire::common::Identifier::vanilla("diamond_pickaxe")};
    lucky.toolEnchantments.emplace("minecraft:fortune", 3);
    std::int32_t most = 0;
    for (std::int32_t roll = 0; roll < 200; ++roll) {
        const std::vector<ItemStack> fortunate = ore->generate(random, 0.0F, std::nullopt, lucky);
        REQUIRE(fortunate.size() == 1);
        CHECK(fortunate.front().count >= 1);
        CHECK(fortunate.front().count <= 4);
        most = std::max(most, fortunate.front().count);
    }
    CHECK(most == 4);

    const std::expected<LootTable, LootTableParseError> leaves = lootTableJson::parse(
        R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:oak_leaves","condition":
           {"type":"minecraft:match_tool","predicate":{"items":"minecraft:shears"}}}]}]})");
    REQUIRE(leaves.has_value());
    CHECK(leaves->generate(random, 0.0F, std::nullopt, plain).empty());
    const LootContext shears{.tool = basalt::wire::common::Identifier::vanilla("shears")};
    CHECK(leaves->generate(random, 0.0F, std::nullopt, shears).size() == 1);
}

TEST_CASE("a death roll reads the entity that died and the one that killed it") {
    // parity: the sheep table picks its wool by colour, and only while it has not been sheared.
    const std::expected<LootTable, LootTableParseError> sheep = lootTableJson::parse(
        R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:red_wool","condition":
           {"type":"minecraft:entity_properties","entity":"this","predicate":{
            "minecraft:components":{"minecraft:sheep/color":"red"},
            "minecraft:type_specific/sheep":{"sheared":false}}}}]},
           {"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:mutton","modifier":[
            {"type":"minecraft:furnace_smelt","condition":{"type":"minecraft:entity_properties",
             "entity":"this","predicate":{"minecraft:flags":{"is_on_fire":true}}}},
            {"type":"minecraft:enchanted_count_increase","enchantment":"minecraft:looting",
             "count":{"type":"minecraft:constant","value":1}}]}]}]})");
    REQUIRE(sheep.has_value());
    rng::RandomSource random = rng::RandomSource::legacy(4);
    EntityFacts red{.type = basalt::wire::common::Identifier::vanilla("sheep"), .onFire = true};
    red.components.emplace("minecraft:sheep/color", "red");
    LootContext context{.thisEntity = red};
    context.smelt = [](const ItemStack& input) -> std::optional<ItemStack> {
        if (input.item.path() != "mutton") {
            return std::nullopt;
        }
        return ItemStack{.item = basalt::wire::common::Identifier::vanilla("cooked_mutton"), .count = 1};
    };
    context.attacker = EntityFacts{.type = basalt::wire::common::Identifier::vanilla("player")};
    context.attackerEnchantments.emplace("minecraft:looting", 2);
    const std::vector<ItemStack> drops = sheep->generate(random, 0.0F, std::nullopt, context);
    REQUIRE(drops.size() == 2);
    CHECK(drops[0].item.path() == "red_wool");
    CHECK(drops[1].item.path() == "cooked_mutton");
    CHECK(drops[1].count == 3);

    red.sheared = true;
    red.onFire = false;
    const std::vector<ItemStack> shorn = sheep->generate(random, 0.0F, std::nullopt, LootContext{.thisEntity = red});
    REQUIRE(shorn.size() == 1);
    CHECK(shorn.front().item.path() == "mutton");
    CHECK(shorn.front().count == 1);
}

TEST_CASE("a chest roll enchants, wears and names what it makes") {
    using basalt::wire::common::Identifier;
    const std::expected<LootTable, LootTableParseError> chest = lootTableJson::parse(
        R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:book","modifier":
            {"type":"minecraft:enchant_randomly","options":"#minecraft:on_random_loot"}}]},
           {"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:iron_sword","modifier":[
            {"type":"minecraft:set_damage","damage":0.25},
            {"type":"minecraft:set_enchantments","enchantments":{"minecraft:knockback":2}},
            {"type":"minecraft:set_name","name":{"translate":"item.sword"},"target":"item_name"}]}]},
           {"rolls":3,"entries":[{"type":"minecraft:empty","weight":1},
            {"type":"minecraft:tag","items":"#minecraft:discs","expand":true,"weight":1}]}]})");
    REQUIRE(chest.has_value());
    const EnchantmentEntry sharpness{.id = Identifier::vanilla("sharpness"), .maxLevel = 5};
    const Enchanting enchanting{.candidates = [&sharpness](const std::vector<std::string>&) {
        return std::vector<const EnchantmentEntry*>{&sharpness};
    }};
    LootContext context{.enchanting = &enchanting};
    context.tagMembers = [](std::string_view, const Identifier&) {
        return std::vector<Identifier>{Identifier::vanilla("music_disc_cat"), Identifier::vanilla("music_disc_13")};
    };
    rng::RandomSource random = rng::RandomSource::legacy(11);
    const std::vector<ItemStack> items = chest->generate(random, 0.0F, std::nullopt, context);
    REQUIRE(items.size() >= 2);
    // parity: a book is swapped for an enchanted one, and the level lands in the stored set.
    CHECK(items[0].item.path() == "enchanted_book");
    const auto* stored = items[0].components.getCompound("minecraft:stored_enchantments");
    REQUIRE(stored != nullptr);
    CHECK(stored->getInt("minecraft:sharpness") >= 1);
    // parity: a quarter of the durability left is three quarters of it worn off.
    CHECK(items[1].components.getInt("minecraft:damage") == 187);
    CHECK(items[1].components.getCompound("minecraft:enchantments")->getInt("minecraft:knockback") == 2);
    CHECK(items[1].components.contains("minecraft:item_name"));
    // The empty choice makes nothing, so the rest are only ever the tag's discs.
    for (std::size_t index = 2; index < items.size(); ++index) {
        CHECK(items[index].item.path().starts_with("music_disc_"));
    }
}

TEST_CASE("a container roll spreads its stacks over the empty slots") {
    // parity: `LootTable.fill` splits stacks until the roll covers the free slots, and never
    // touches a slot that already holds something.
    const std::expected<LootTable, LootTableParseError> chest = lootTableJson::parse(
        R"({"pools":[{"rolls":1,"entries":[{"type":"minecraft:item","name":"minecraft:bread","modifier":
            {"type":"minecraft:set_count","count":20}}]}]})");
    REQUIRE(chest.has_value());
    std::vector<std::optional<ItemStack>> slots(27);
    slots[4] = ItemStack{.item = basalt::wire::common::Identifier::vanilla("stone"), .count = 1};
    rng::RandomSource random = rng::RandomSource::legacy(42);
    chest->fill(slots, random);
    std::int32_t bread = 0;
    std::int32_t used = 0;
    for (const std::optional<ItemStack>& slot : slots) {
        if (slot.has_value() && slot->item.path() == "bread") {
            bread += slot->count;
            ++used;
        }
    }
    CHECK(bread == 20);
    CHECK(used > 1);
    REQUIRE(slots[4].has_value());
    CHECK(slots[4]->item.path() == "stone");
}

TEST_CASE("a location check reads the square the offsets point at, and fails without an origin") {
    using basalt::wire::common::Identifier;
    const std::expected<LootTable, LootTableParseError> grass = lootTableJson::parse(
        R"({"pools":[{"rolls":1,"condition":{"type":"minecraft:location_check","offsetY":1,
            "predicate":{"block":{"blocks":"minecraft:tall_grass","state":{"half":"upper"}}}},
            "entries":[{"type":"minecraft:item","name":"minecraft:short_grass"}]},
           {"rolls":1,"condition":{"type":"minecraft:location_check",
            "predicate":{"biomes":["minecraft:jungle"]}},
            "entries":[{"type":"minecraft:item","name":"minecraft:bamboo"}]}]})");
    REQUIRE(grass.has_value());
    rng::RandomSource random = rng::RandomSource::legacy(1);
    CHECK(grass->generate(random).empty());
    LootContext context{.origin = std::array<double, 3>{0.5, 64.5, 0.5}};
    context.placeAt = [](std::int32_t, std::int32_t y, std::int32_t) {
        PlaceFacts place{.biome = Identifier::vanilla("plains")};
        if (y == 65) {
            place.block = Identifier::vanilla("tall_grass");
            place.properties = {{"half", "upper"}};
        }
        return place;
    };
    const std::vector<ItemStack> drops = grass->generate(random, 0.0F, std::nullopt, context);
    REQUIRE(drops.size() == 1);
    CHECK(drops.front().item.path() == "short_grass");
}
