#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/scoreboard/Scoreboard.hpp>
#include <basalt/server/runtime/scoreboard/ScoreboardStorage.hpp>
#include <basalt/server/runtime/scoreboard/Teams.hpp>

#include <doctest.h>

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

using namespace basalt::server::runtime::scoreboard;
namespace play = basalt::wire::protocol::play;
using basalt::wire::text::Component;

namespace {

[[nodiscard]] Objective dummy(std::string name) {
    return Objective{.name = name, .criterion = "dummy", .displayName = Component::text(std::move(name))};
}

} // namespace

TEST_CASE("the criteria a `/scoreboard objectives add` accepts, and what they mean") {
    // parity: ObjectiveCriteria's own registrations, in the order the class declares them.
    const std::vector<std::string> expected = {"dummy",          "trigger", "deathCount", "playerKillCount",
                                               "totalKillCount", "health",  "food",       "air",
                                               "armor",          "xp",      "level"};
    const std::vector<std::string>& criteria = Objective::criteria();
    REQUIRE(criteria.size() == expected.size() + 32);
    CHECK(std::vector<std::string>(criteria.begin(), criteria.begin() + 11) == expected);
    // parity: `registerForEveryTeamColor`, every team kill before every killed-by-team.
    CHECK(criteria[11] == "teamkill.black");
    CHECK(criteria[26] == "teamkill.white");
    CHECK(criteria[27] == "killedByTeam.black");
    CHECK(criteria.back() == "killedByTeam.white");

    // `trigger` is writable and gated by the score's lock, so it is not one of the six the server writes.
    for (const std::string& criterion : {"health", "food", "air", "armor", "xp", "level"}) {
        CAPTURE(criterion);
        CHECK(Objective::isReadOnly(criterion));
    }
    for (const std::string& criterion : {"dummy", "trigger", "deathCount", "playerKillCount", "totalKillCount"}) {
        CAPTURE(criterion);
        CHECK_FALSE(Objective::isReadOnly(criterion));
    }

    CHECK(Objective::defaultRenderType("health") == play::ObjectiveRenderType::Hearts);
    for (const std::string& criterion : Objective::criteria()) {
        CAPTURE(criterion);
        if (criterion != "health") {
            CHECK(Objective::defaultRenderType(criterion) == play::ObjectiveRenderType::Integer);
        }
    }

    const Objective fresh = dummy("x");
    CHECK(fresh.renderType == play::ObjectiveRenderType::Integer);
    CHECK(fresh.displayAutoUpdate);
    CHECK_FALSE(fresh.numberFormat.has_value());

    CHECK(dummy("x").isTrigger() == false);
    CHECK(Objective{.name = "t", .criterion = "trigger", .displayName = Component::empty()}.isTrigger());
}

TEST_CASE("objectives keep the order they were added, and a taken name is refused") {
    Scoreboard scoreboard;
    CHECK(scoreboard.add(dummy("kills")));
    CHECK(scoreboard.add(dummy("deaths")));
    CHECK_FALSE(scoreboard.add(dummy("kills")));

    REQUIRE(scoreboard.allObjectives().size() == 2);
    CHECK(scoreboard.allObjectives()[0]->name == "kills");
    CHECK(scoreboard.allObjectives()[1]->name == "deaths");
    CHECK(scoreboard.get("kills") != nullptr);
    CHECK(scoreboard.get("nothing") == nullptr);
}

TEST_CASE("removing an objective takes its scores and its slot with it") {
    Scoreboard scoreboard;
    scoreboard.add(dummy("kills"));
    scoreboard.add(dummy("deaths"));
    scoreboard.setScore("Steve", "kills", 3);
    scoreboard.setScore("Steve", "deaths", 1);
    scoreboard.setScore("Alex", "kills", 9);
    scoreboard.display(play::DisplaySlot::Sidebar, "kills");
    scoreboard.display(play::DisplaySlot::List, "deaths");

    const std::unique_ptr<Objective> removed = scoreboard.remove("kills");
    REQUIRE(removed != nullptr);
    CHECK(removed->name == "kills");
    CHECK(scoreboard.get("kills") == nullptr);
    CHECK_FALSE(scoreboard.score("Steve", "kills").has_value());
    CHECK(scoreboard.score("Steve", "deaths") == 1);
    CHECK(scoreboard.displayed(play::DisplaySlot::Sidebar) == nullptr);
    REQUIRE(scoreboard.displayed(play::DisplaySlot::List) != nullptr);
    CHECK(*scoreboard.displayed(play::DisplaySlot::List) == "deaths");

    // parity: removeObjective leaves a holder with no scores left on the books, unlike a reset.
    CHECK(scoreboard.holders() == std::vector<std::string>{"Steve", "Alex"});
    CHECK(scoreboard.scoresOf("Alex").empty());

    CHECK(scoreboard.remove("kills") == nullptr);
}

TEST_CASE("a slot holds one objective at a time and is cleared by name-less display") {
    Scoreboard scoreboard;
    CHECK(scoreboard.displayed(play::DisplaySlot::Sidebar) == nullptr);
    scoreboard.display(play::DisplaySlot::Sidebar, "kills");
    scoreboard.display(play::DisplaySlot::Sidebar, "deaths");
    REQUIRE(scoreboard.displayed(play::DisplaySlot::Sidebar) != nullptr);
    CHECK(*scoreboard.displayed(play::DisplaySlot::Sidebar) == "deaths");
    scoreboard.display(play::DisplaySlot::Sidebar, std::nullopt);
    CHECK(scoreboard.displayed(play::DisplaySlot::Sidebar) == nullptr);
    scoreboard.display(play::DisplaySlot::BelowName, std::nullopt);
    CHECK(scoreboard.displayed(play::DisplaySlot::BelowName) == nullptr);
}

TEST_CASE("scores are per holder and per objective, and adding saturates at the ends") {
    Scoreboard scoreboard;
    CHECK_FALSE(scoreboard.score("Steve", "kills").has_value());
    scoreboard.setScore("Steve", "kills", 5);
    scoreboard.setScore("Steve", "deaths", 2);
    scoreboard.setScore("Alex", "kills", 7);
    CHECK(scoreboard.score("Steve", "kills") == 5);
    CHECK(scoreboard.score("Alex", "kills") == 7);
    CHECK_FALSE(scoreboard.score("Alex", "deaths").has_value());

    CHECK(scoreboard.addScore("Herobrine", "kills", 4) == 4);
    CHECK(scoreboard.addScore("Steve", "kills", -2) == 3);
    CHECK(scoreboard.score("Steve", "kills") == 3);

    // parity: the wire carries an int, so vanilla's own add clamps rather than wrapping round.
    scoreboard.setScore("Steve", "kills", std::numeric_limits<std::int32_t>::max());
    CHECK(scoreboard.addScore("Steve", "kills", 100) == std::numeric_limits<std::int32_t>::max());
    scoreboard.setScore("Steve", "kills", std::numeric_limits<std::int32_t>::min());
    CHECK(scoreboard.addScore("Steve", "kills", -100) == std::numeric_limits<std::int32_t>::min());

    CHECK(scoreboard.holders() == std::vector<std::string>{"Steve", "Alex", "Herobrine"});
    CHECK(scoreboard.holdersIn("deaths") == std::vector<std::string>{"Steve"});
    CHECK(scoreboard.holdersIn("kills") == std::vector<std::string>{"Steve", "Alex", "Herobrine"});
    CHECK(scoreboard.holdersIn("nothing").empty());

    const std::vector<std::pair<std::string, std::int32_t>> steve = {
        {"kills", std::numeric_limits<std::int32_t>::min()}, {"deaths", 2}};
    CHECK(scoreboard.scoresOf("Steve") == steve);
    CHECK(scoreboard.scoresOf("nobody").empty());
}

TEST_CASE("a reset drops the holder once nothing is left, along with what hung off the score") {
    Scoreboard scoreboard;
    scoreboard.setScore("Steve", "kills", 5);
    scoreboard.setScore("Steve", "deaths", 2);
    scoreboard.setDisplayName("Steve", "kills", Component::text("The Steve"));
    scoreboard.setNumberFormat("Steve", "kills", play::NumberFormat{play::NumberFormat::Blank{}});
    CHECK(scoreboard.displayNameOf("Steve", "kills") != nullptr);
    CHECK(scoreboard.numberFormatOf("Steve", "kills") != nullptr);

    scoreboard.resetScore("Steve", "kills");
    CHECK_FALSE(scoreboard.score("Steve", "kills").has_value());
    CHECK(scoreboard.displayNameOf("Steve", "kills") == nullptr);
    CHECK(scoreboard.numberFormatOf("Steve", "kills") == nullptr);
    CHECK(scoreboard.holders() == std::vector<std::string>{"Steve"});

    scoreboard.resetScore("Steve", "deaths");
    CHECK(scoreboard.holders().empty());

    scoreboard.setScore("Alex", "kills", 1);
    scoreboard.setScore("Alex", "deaths", 1);
    scoreboard.resetScore("Alex", std::nullopt);
    CHECK(scoreboard.holders().empty());
    scoreboard.resetScore("Nobody", std::nullopt);
    scoreboard.resetScore("Nobody", "kills");
    CHECK(scoreboard.holders().empty());
}

TEST_CASE("per-holder overrides are set and cleared without disturbing the number") {
    Scoreboard scoreboard;
    CHECK(scoreboard.displayNameOf("Steve", "kills") == nullptr);
    scoreboard.setDisplayName("Steve", "kills", Component::text("The Steve"));
    CHECK(scoreboard.score("Steve", "kills") == 0);
    REQUIRE(scoreboard.displayNameOf("Steve", "kills") != nullptr);
    CHECK(*scoreboard.displayNameOf("Steve", "kills") == Component::text("The Steve"));

    scoreboard.setScore("Steve", "kills", 4);
    CHECK(scoreboard.displayNameOf("Steve", "kills") != nullptr);
    scoreboard.setDisplayName("Steve", "kills", std::nullopt);
    CHECK(scoreboard.displayNameOf("Steve", "kills") == nullptr);
    CHECK(scoreboard.score("Steve", "kills") == 4);

    scoreboard.setNumberFormat("Steve", "kills", play::NumberFormat{play::NumberFormat::Blank{}});
    REQUIRE(scoreboard.numberFormatOf("Steve", "kills") != nullptr);
    CHECK(scoreboard.numberFormatOf("Steve", "kills")->typeId() == 0);
    scoreboard.setNumberFormat("Steve", "kills", std::nullopt);
    CHECK(scoreboard.numberFormatOf("Steve", "kills") == nullptr);
    CHECK(scoreboard.score("Steve", "kills") == 4);
}

TEST_CASE("a trigger is locked until enabled, and one enable buys one use") {
    Scoreboard scoreboard;
    // parity: a score is born locked, so `/trigger` on a fresh objective is unprimed.
    scoreboard.setScore("Steve", "menu", 0);
    CHECK_FALSE(scoreboard.isTriggerEnabled("Steve", "menu"));

    scoreboard.enableTrigger("Steve", "menu");
    CHECK(scoreboard.isTriggerEnabled("Steve", "menu"));
    CHECK_FALSE(scoreboard.isTriggerEnabled("Alex", "menu"));
    CHECK_FALSE(scoreboard.isTriggerEnabled("Steve", "other"));

    scoreboard.disableTrigger("Steve", "menu");
    CHECK_FALSE(scoreboard.isTriggerEnabled("Steve", "menu"));
    CHECK(scoreboard.score("Steve", "menu") == 0);

    scoreboard.enableTrigger("Alex", "menu");
    CHECK(scoreboard.score("Alex", "menu") == 0);
    CHECK(scoreboard.isTriggerEnabled("Alex", "menu"));

    // parity: a reset unprimes it, because the lock lives on the score that was just thrown away.
    scoreboard.resetScore("Alex", "menu");
    CHECK_FALSE(scoreboard.isTriggerEnabled("Alex", "menu"));

    scoreboard.disableTrigger("Nobody", "menu");
    CHECK(scoreboard.holders() == std::vector<std::string>{"Steve"});
}

TEST_CASE("a team starts with vanilla's own defaults and answers them as packet parameters") {
    Teams teams;
    Team* red = teams.add("red");
    REQUIRE(red != nullptr);
    CHECK(red->name == "red");
    CHECK(red->displayName == Component::text("red"));
    CHECK(red->prefix == Component::empty());
    CHECK(red->suffix == Component::empty());
    CHECK_FALSE(red->color.has_value());
    CHECK(red->friendlyFire);
    CHECK(red->seeFriendlyInvisibles);
    CHECK(red->nameTagVisibility == play::NameTagVisibility::Always);
    CHECK(red->deathMessageVisibility == play::NameTagVisibility::Always);
    CHECK(red->collisionRule == play::CollisionRule::Always);
    CHECK(red->members.empty());

    const play::SetPlayerTeamPacket::Parameters parameters = red->parameters();
    CHECK(parameters.displayName == Component::text("red"));
    // parity: PlayerTeam.packOptions, bit 0 friendly fire and bit 1 seeing friendly invisibles.
    CHECK(parameters.options() == 3);
    red->friendlyFire = false;
    CHECK(red->parameters().options() == 2);
    red->seeFriendlyInvisibles = false;
    CHECK(red->parameters().options() == 0);

    // The visibility on the wire is the name tag's, not the death message's, which never leaves the server.
    red->prefix = Component::text("[");
    red->suffix = Component::text("]");
    red->nameTagVisibility = play::NameTagVisibility::HideForOtherTeams;
    red->deathMessageVisibility = play::NameTagVisibility::Never;
    red->color = play::TeamColor::Red;
    const play::SetPlayerTeamPacket::Parameters changed = red->parameters();
    CHECK(changed.prefix == Component::text("["));
    CHECK(changed.suffix == Component::text("]"));
    CHECK(changed.nameTagVisibility == play::NameTagVisibility::HideForOtherTeams);
    CHECK(changed.color == play::TeamColor::Red);

    CHECK(teams.add("red") == nullptr);
    CHECK(teams.all().size() == 1);
}

TEST_CASE("a member belongs to one team, and joining another takes them out of the first") {
    Teams teams;
    Team* red = teams.add("red");
    Team* blue = teams.add("blue");
    REQUIRE(red != nullptr);
    REQUIRE(blue != nullptr);

    CHECK(teams.join(*red, "Steve") == nullptr);
    CHECK(teams.join(*red, "Alex") == nullptr);
    CHECK(red->members == std::vector<std::string>{"Steve", "Alex"});
    CHECK(teams.teamOf("Steve") == red);

    CHECK(teams.join(*blue, "Steve") == red);
    CHECK(red->members == std::vector<std::string>{"Alex"});
    CHECK(blue->members == std::vector<std::string>{"Steve"});
    CHECK(teams.teamOf("Steve") == blue);

    // Joining the team already on, or adding twice, is not a duplicate: vanilla's members are a set.
    CHECK(teams.join(*blue, "Steve") == blue);
    CHECK(blue->members == std::vector<std::string>{"Steve"});
    CHECK_FALSE(blue->addMember("Steve"));
    CHECK(blue->members == std::vector<std::string>{"Steve"});
    CHECK(blue->addMember("Herobrine"));
    CHECK(blue->members == std::vector<std::string>{"Steve", "Herobrine"});
    CHECK(blue->hasMember("Steve"));
    CHECK_FALSE(blue->hasMember("Alex"));
    CHECK(blue->removeMember("Herobrine"));
    CHECK_FALSE(blue->removeMember("Herobrine"));

    CHECK(teams.leave("Steve") == blue);
    CHECK(blue->members.empty());
    CHECK(teams.teamOf("Steve") == nullptr);
    CHECK(teams.leave("Steve") == nullptr);
}

TEST_CASE("a removed team is handed back, members and all") {
    Teams teams;
    Team* red = teams.add("red");
    REQUIRE(red != nullptr);
    teams.join(*red, "Steve");
    teams.add("blue");

    const std::unique_ptr<Team> removed = teams.remove("red");
    REQUIRE(removed != nullptr);
    CHECK(removed->name == "red");
    CHECK(removed->members == std::vector<std::string>{"Steve"});
    CHECK(teams.get("red") == nullptr);
    CHECK(teams.teamOf("Steve") == nullptr);
    REQUIRE(teams.all().size() == 1);
    CHECK(teams.all()[0]->name == "blue");
    CHECK(teams.remove("red") == nullptr);
}

TEST_CASE("a criterion reaches every objective that counts it, and only those") {
    Scoreboard board;
    REQUIRE(board.add(Objective{.name = "deaths", .criterion = "deathCount", .displayName = Component::text("d")}));
    REQUIRE(board.add(Objective{.name = "more", .criterion = "deathCount", .displayName = Component::text("m")}));
    REQUIRE(board.add(dummy("plain")));
    const auto touched = board.forAllObjectives("deathCount", "Steve", [](std::int32_t value) { return value + 1; });
    CHECK(touched.size() == 2);
    CHECK(board.score("Steve", "deaths") == std::optional<std::int32_t>{1});
    CHECK(board.score("Steve", "more") == std::optional<std::int32_t>{1});
    CHECK_FALSE(board.score("Steve", "plain").has_value());
}

TEST_CASE("a statistic criterion is resolved the way the jar reads it") {
    // parity: `ObjectiveCriteria.byName` over `Identifier.bySeparator(name, '.')`.
    CHECK(Objective::canonical("deathCount") == std::optional<std::string>{"deathCount"});
    CHECK(Objective::canonical("custom:jump") == std::optional<std::string>{"minecraft.custom:minecraft.jump"});
    CHECK(Objective::canonical("minecraft.mined:minecraft.stone") ==
          std::optional<std::string>{"minecraft.mined:minecraft.stone"});
    CHECK_FALSE(Objective::canonical("nonsense").has_value());
    CHECK_FALSE(Objective::canonical("minecraft.nothing:minecraft.stone").has_value());
    CHECK_FALSE(Objective::canonical("minecraft.mined:minecraft.diamond").has_value());
    CHECK_FALSE(Objective::canonical("custom:nonsense").has_value());
    CHECK(Objective::canonical("killed:zombie") == std::optional<std::string>{"minecraft.killed:minecraft.zombie"});
    CHECK(Objective::canonical("crafted:minecraft.diamond") ==
          std::optional<std::string>{"minecraft.crafted:minecraft.diamond"});
    CHECK(Objective::criterionOfStat("minecraft:custom/minecraft:jump") == "minecraft.custom:minecraft.jump");
}

TEST_CASE("a saved scoreboard comes back with its scores, display slot and teams") {
    Scoreboard scoreboard;
    Teams teams;
    scoreboard.add(dummy("kills"));
    scoreboard.setScore("Alice", "kills", 7);
    scoreboard.display(play::DisplaySlot::Sidebar, "kills");
    Team* red = teams.add("red");
    red->color = play::TeamColor::Red;
    red->friendlyFire = false;
    red->addMember("Alice");

    Scoreboard loadedScoreboard;
    Teams loadedTeams;
    unpack(pack(scoreboard, teams), loadedScoreboard, loadedTeams);

    CHECK(loadedScoreboard.score("Alice", "kills") == 7);
    REQUIRE(loadedScoreboard.displayed(play::DisplaySlot::Sidebar) != nullptr);
    CHECK(*loadedScoreboard.displayed(play::DisplaySlot::Sidebar) == "kills");
    const Team* loaded = loadedTeams.get("red");
    REQUIRE(loaded != nullptr);
    CHECK(loaded->color == play::TeamColor::Red);
    CHECK_FALSE(loaded->friendlyFire);
    CHECK(loaded->hasMember("Alice"));
}
