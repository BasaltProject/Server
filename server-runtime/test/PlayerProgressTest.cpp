#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/PlayerProgress.hpp>

#include <doctest.h>

#include <map>
#include <string>

using basalt::server::runtime::world::PlayerProgress;
using basalt::server::runtime::world::RealAdvancements;
using basalt::server::world::advancement::Advancement;
using basalt::wire::common::Identifier;

namespace {

[[nodiscard]] Advancement childOf(const std::optional<Identifier>& parent) {
    Advancement advancement;
    advancement.parent = parent;
    return advancement;
}

/// A little tree: root -> stone -> iron, and root -> wood.
[[nodiscard]] RealAdvancements tree() {
    std::map<Identifier, Advancement> definitions;
    definitions.emplace(Identifier::vanilla("story/root"), childOf(std::nullopt));
    definitions.emplace(Identifier::vanilla("story/stone"), childOf(Identifier::vanilla("story/root")));
    definitions.emplace(Identifier::vanilla("story/iron"), childOf(Identifier::vanilla("story/stone")));
    definitions.emplace(Identifier::vanilla("story/wood"), childOf(Identifier::vanilla("story/root")));
    return RealAdvancements{std::move(definitions)};
}

} // namespace

TEST_CASE("an advancement is found by its own id") {
    const RealAdvancements advancements = tree();

    CHECK(advancements.size() == 4);
    CHECK(advancements.find(Identifier::vanilla("story/stone")) != nullptr);
    CHECK(advancements.find(Identifier::vanilla("story/nothing")) == nullptr);
    CHECK(advancements.ids().size() == 4);
}

TEST_CASE("everything below one advancement is what a grant from it reaches") {
    const RealAdvancements advancements = tree();

    const std::vector<Identifier> below = advancements.descendantsOf(Identifier::vanilla("story/root"));

    CHECK(below.size() == 4);
    CHECK(advancements.descendantsOf(Identifier::vanilla("story/stone")).size() == 2);
    CHECK(advancements.descendantsOf(Identifier::vanilla("story/iron")).size() == 1);
}

TEST_CASE("everything above one advancement is what a grant until it reaches") {
    const RealAdvancements advancements = tree();

    const std::vector<Identifier> above = advancements.ancestorsOf(Identifier::vanilla("story/iron"));

    REQUIRE(above.size() == 3);
    CHECK(above[0] == Identifier::vanilla("story/iron"));
    CHECK(above[1] == Identifier::vanilla("story/stone"));
    CHECK(above[2] == Identifier::vanilla("story/root"));

    CHECK(advancements.ancestorsOf(Identifier::vanilla("story/root")).size() == 1);
    // One nobody declared is still itself and nothing more.
    CHECK(advancements.ancestorsOf(Identifier::vanilla("story/nothing")).size() == 1);
}

TEST_CASE("an advancement is read out of the jar by its path under the directory") {
    const RealAdvancements::Jar jar{
        .entriesUnder =
            [](std::string_view) {
                return std::vector<std::string>{"data/minecraft/advancement/story/root.json",
                                                "data/minecraft/advancement/notes.txt",
                                                "data/minecraft/advancement/story/broken.json"};
            },
        .readText = [](std::string_view name) -> std::optional<std::string> {
            if (name == "data/minecraft/advancement/story/root.json") {
                return std::string{R"({"criteria": {"tick": {"trigger": "minecraft:tick"}}})"};
            }
            return std::string{"{not json"};
        }};

    const RealAdvancements advancements{jar};

    CHECK(advancements.size() == 1);
    CHECK(advancements.find(Identifier::vanilla("story/root")) != nullptr);
}

TEST_CASE("without a jar there are no advancements at all") {
    const RealAdvancements advancements{RealAdvancements::Jar{}};

    CHECK(advancements.size() == 0);
}

TEST_CASE("a criterion is granted once and only once") {
    PlayerProgress progress;
    const Identifier story = Identifier::vanilla("story/root");

    CHECK(progress.grantCriterion("uuid-1", story, "tick"));
    CHECK_FALSE(progress.grantCriterion("uuid-1", story, "tick"));
    CHECK(progress.grantedCriteria("uuid-1", story).size() == 1);
    CHECK(progress.grantedCriteria("uuid-1", story).contains("tick"));
    // Another player's progress is their own.
    CHECK(progress.grantedCriteria("uuid-2", story).empty());
}

TEST_CASE("a criterion nobody granted cannot be revoked") {
    PlayerProgress progress;
    const Identifier story = Identifier::vanilla("story/root");

    CHECK_FALSE(progress.revokeCriterion("uuid-1", story, "tick"));

    REQUIRE(progress.grantCriterion("uuid-1", story, "tick"));
    CHECK(progress.revokeCriterion("uuid-1", story, "tick"));
    CHECK_FALSE(progress.revokeCriterion("uuid-1", story, "tick"));
    CHECK(progress.grantedCriteria("uuid-1", story).empty());
}

TEST_CASE("revoking a whole advancement takes every criterion with it") {
    PlayerProgress progress;
    const Identifier story = Identifier::vanilla("story/root");
    REQUIRE(progress.grantCriterion("uuid-1", story, "tick"));
    REQUIRE(progress.grantCriterion("uuid-1", story, "mine"));

    CHECK(progress.revokeAll("uuid-1", story));

    CHECK(progress.grantedCriteria("uuid-1", story).empty());
    CHECK_FALSE(progress.revokeAll("uuid-1", story));
    CHECK_FALSE(progress.revokeAll("uuid-2", story));
}

TEST_CASE("a recipe is learnt once and forgotten once") {
    PlayerProgress progress;
    const Identifier torch = Identifier::vanilla("torch");

    CHECK(progress.learn("uuid-1", torch));
    CHECK_FALSE(progress.learn("uuid-1", torch));
    CHECK(progress.knownRecipes("uuid-1").contains(torch));

    CHECK(progress.forget("uuid-1", torch));
    CHECK_FALSE(progress.forget("uuid-1", torch));
    CHECK(progress.knownRecipes("uuid-1").empty());
    CHECK_FALSE(progress.forget("uuid-2", torch));
}

TEST_CASE("unloading a player forgets what was held for them and nobody else") {
    PlayerProgress progress;
    const Identifier story = Identifier::vanilla("story/root");
    REQUIRE(progress.grantCriterion("uuid-1", story, "tick"));
    REQUIRE(progress.learn("uuid-1", Identifier::vanilla("torch")));
    REQUIRE(progress.grantCriterion("uuid-2", story, "tick"));

    progress.unload("uuid-1");

    CHECK(progress.grantedCriteria("uuid-1", story).empty());
    CHECK(progress.knownRecipes("uuid-1").empty());
    CHECK(progress.grantedCriteria("uuid-2", story).size() == 1);
    // Unloading somebody who was never loaded is not an error.
    progress.unload("uuid-3");
}

TEST_CASE("with no database behind it a load changes nothing") {
    PlayerProgress progress;
    REQUIRE(progress.grantCriterion("uuid-1", Identifier::vanilla("story/root"), "tick"));

    progress.load("uuid-1");

    CHECK(progress.grantedCriteria("uuid-1", Identifier::vanilla("story/root")).size() == 1);
}
