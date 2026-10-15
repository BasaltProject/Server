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
        CHECK(gameRules::byId(rule.id) == &rule);
    }

    CHECK(gameRules::byId("minecraft:keep_inventory") == gameRules::byId("keep_inventory"));
    CHECK(gameRules::byId("keepInventory") == nullptr);
    CHECK(gameRules::byId("") == nullptr);
    CHECK(gameRules::byId("keep_") == nullptr);
    CHECK(gameRules::byId("keep_inventory2") == nullptr);
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
    CHECK(read.get(gameRules::kAdvanceTime));
    CHECK(read.get(gameRules::kKeepInventory));

    NbtCompound written;
    read.writeTo(written);
    CHECK(written.getCompound("GameRules")->size() == 1);
}
