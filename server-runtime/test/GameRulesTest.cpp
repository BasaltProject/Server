#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/GameRules.hpp>

#include <doctest.h>

#include <algorithm>
#include <string>

using namespace basalt::server::runtime::world;
using basalt::wire::nbt::NbtCompound;
using basalt::wire::nbt::NbtTag;

TEST_CASE("every rule vanilla has, once, in the order a list is read in") {
    const std::span<const GameRule> all = gameRules::all();
    CHECK(all.size() == 59);
    CHECK(std::ranges::is_sorted(all, {}, &GameRule::id));
    for (const GameRule& rule : all) {
        CAPTURE(rule.id);
        CHECK_FALSE(rule.id.empty());
        // The registry answers with the very rule it holds, not a copy of it.
        CHECK(gameRules::byId(rule.id) == &rule);
    }

    // A command carries the namespace, and the rules are not stored under it.
    CHECK(gameRules::byId("minecraft:keep_inventory") == gameRules::byId("keep_inventory"));
    CHECK(gameRules::byId("keepInventory") == nullptr);
    CHECK(gameRules::byId("") == nullptr);
    // A prefix of a real id is not a rule, and neither is a rule with something after it.
    CHECK(gameRules::byId("keep_") == nullptr);
    CHECK(gameRules::byId("keep_inventory2") == nullptr);
}

TEST_CASE("the defaults are vanilla's, including the ones that read backwards") {
    const GameRuleValues values;
    CHECK(values.get(gameRules::kAdvanceTime));
    CHECK(values.get(gameRules::kSpawnMobs));
    CHECK_FALSE(values.get(gameRules::kKeepInventory));
    // parity: off in vanilla, which is why lava has no infinite pools while water does.
    CHECK_FALSE(values.get(gameRules::kLavaSourceConversion));
    CHECK(values.get(gameRules::kWaterSourceConversion));
    CHECK(values.number(gameRules::kRandomTickSpeed) == 3);
    CHECK(values.number(gameRules::kPlayersSleepingPercentage) == 100);
    CHECK(values.number(gameRules::kPlayersNetherPortalDefaultDelay) == 80);
    CHECK(values.number(gameRules::kMaxBlockModifications) == 32768);

    // The six that are off while everything around them is on: a block explosion decays its drops
    // and a tnt one does not, and the rest are the switches vanilla ships turned off.
    CHECK_FALSE(values.get(*gameRules::byId("tnt_explosion_drop_decay")));
    CHECK(values.get(*gameRules::byId("block_explosion_drop_decay")));
    CHECK(values.get(*gameRules::byId("mob_explosion_drop_decay")));
    CHECK_FALSE(values.get(*gameRules::byId("universal_anger")));
    CHECK_FALSE(values.get(*gameRules::byId("reduced_debug_info")));
    CHECK_FALSE(values.get(gameRules::kImmediateRespawn));
    CHECK_FALSE(values.get(gameRules::kLimitedCrafting));

    // The bounds an integer rule accepts, which is what `/gamerule` refuses a value against.
    CHECK(gameRules::kRandomTickSpeed.min == 0);
    CHECK(gameRules::byId("max_minecart_speed")->min == 1);
    CHECK(gameRules::byId("max_minecart_speed")->max == 1000);
    CHECK(gameRules::byId("fire_spread_radius_around_player")->min == -1);
    CHECK(gameRules::byId("max_snow_accumulation_height")->max == 8);
}

TEST_CASE("what was set is what is answered, and what was not is the default") {
    GameRuleValues values;
    values.set(gameRules::kKeepInventory, "true");
    values.set(gameRules::kRandomTickSpeed, "0");
    CHECK(values.get(gameRules::kKeepInventory));
    CHECK(values.number(gameRules::kRandomTickSpeed) == 0);
    CHECK(values.serialized(gameRules::kKeepInventory) == "true");
    CHECK(values.serialized(gameRules::kRandomTickSpeed) == "0");
    // Unset rules print their default rather than an empty string.
    CHECK(values.serialized(gameRules::kAdvanceTime) == "true");
    CHECK(values.serialized(gameRules::kLavaSourceConversion) == "false");
    CHECK(values.serialized(gameRules::kPlayersSleepingPercentage) == "100");

    // Nonsense stored by an older build or an edited file reads as the default, not as false or
    // zero: `TRUE` is not `true` and half a number is not a number.
    values.set(gameRules::kAdvanceTime, "TRUE");
    values.set(gameRules::kPlayersSleepingPercentage, "50%");
    CHECK(values.get(gameRules::kAdvanceTime));
    CHECK(values.number(gameRules::kPlayersSleepingPercentage) == 100);
    // What it prints is still what was stored: the value survives a round trip it cannot parse.
    CHECK(values.serialized(gameRules::kAdvanceTime) == "TRUE");

    values.set(gameRules::kAdvanceTime, "false");
    CHECK_FALSE(values.get(gameRules::kAdvanceTime));
    values.set(gameRules::kPlayersSleepingPercentage, "-7");
    CHECK(values.number(gameRules::kPlayersSleepingPercentage) == -7);
}

TEST_CASE("a world that changed nothing stores nothing") {
    NbtCompound root;
    GameRuleValues{}.writeTo(root);
    CHECK_FALSE(root.contains("GameRules"));
    // And reads back as the same thing it was, so a fresh world equals a saved empty one.
    CHECK(GameRuleValues::readFrom(root) == GameRuleValues{});
}

TEST_CASE("the values a world keeps survive a trip through the region file") {
    GameRuleValues values;
    values.set(gameRules::kKeepInventory, "true");
    values.set(gameRules::kRandomTickSpeed, "10");
    values.set(*gameRules::byId("pvp"), "false");

    NbtCompound root;
    values.writeTo(root);
    REQUIRE(root.getCompound("GameRules") != nullptr);
    CHECK(root.getCompound("GameRules")->size() == 3);
    CHECK(root.getCompound("GameRules")->getString("keep_inventory") == "true");

    const GameRuleValues read = GameRuleValues::readFrom(root);
    CHECK(read == values);
    CHECK(read.get(gameRules::kKeepInventory));
    CHECK(read.number(gameRules::kRandomTickSpeed) == 10);
    CHECK_FALSE(read.get(*gameRules::byId("pvp")));
}

TEST_CASE("a rule this build does not know is dropped rather than kept") {
    NbtCompound rules;
    rules.putString("doDaylightCycle", "false");
    rules.putString("keep_inventory", "true");
    NbtCompound root;
    root.set("GameRules", NbtTag::ofCompound(std::move(rules)));

    const GameRuleValues read = GameRuleValues::readFrom(root);
    // The 26.2 name is `advance_time`, and a value stored under the old one is not it.
    CHECK(read.get(gameRules::kAdvanceTime));
    CHECK(read.get(gameRules::kKeepInventory));

    NbtCompound written;
    read.writeTo(written);
    CHECK(written.getCompound("GameRules")->size() == 1);
}
