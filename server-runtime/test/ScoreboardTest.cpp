#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/scoreboard/Scoreboard.hpp>
#include <basalt/server/runtime/scoreboard/Teams.hpp>

#include <doctest.h>

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
    CHECK(Objective::criteria() == expected);

    // The six the server writes itself, which a command may not set. `trigger` is not one of them:
    // it is writable, and gated by the score's lock instead.
    for (const std::string& criterion : {"health", "food", "air", "armor", "xp", "level"}) {
        CAPTURE(criterion);
        CHECK(Objective::isReadOnly(criterion));
    }
    for (const std::string& criterion : {"dummy", "trigger", "deathCount", "playerKillCount", "totalKillCount"}) {
        CAPTURE(criterion);
        CHECK_FALSE(Objective::isReadOnly(criterion));
    }

    // Only `health` draws as hearts.
    CHECK(Objective::defaultRenderType("health") == play::ObjectiveRenderType::Hearts);
    for (const std::string& criterion : Objective::criteria()) {
        CAPTURE(criterion);
        if (criterion != "health") {
            CHECK(Objective::defaultRenderType(criterion) == play::ObjectiveRenderType::Integer);
        }
    }

    // A fresh objective draws as a number and keeps its display name up to date, which is what
    // `/scoreboard objectives add` leaves behind before `displayautoupdate` is touched.
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
    // The slot showing it is cleared; the one showing something else is left alone.
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
    // Clearing an empty slot is not an error, and does not disturb the others.
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

    // A missing score counts as zero, which is what `/scoreboard players add` does.
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
    // parity: the override lives on the score object, so it goes when the score does rather than
    // coming back attached to the next score of the same name.
    CHECK(scoreboard.displayNameOf("Steve", "kills") == nullptr);
    CHECK(scoreboard.numberFormatOf("Steve", "kills") == nullptr);
    // Still tracked, because a score is left.
    CHECK(scoreboard.holders() == std::vector<std::string>{"Steve"});

    scoreboard.resetScore("Steve", "deaths");
    CHECK(scoreboard.holders().empty());

    // Resetting everything, and resetting what was never there.
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
    // Setting an override creates the score at zero, as `getOrCreatePlayerScore` does.
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
    // Locking again keeps the number, which is what the client is still being shown.
    CHECK(scoreboard.score("Steve", "menu") == 0);

    // Enabling a holder nobody has scored creates the score at zero, as vanilla's own enable does.
    scoreboard.enableTrigger("Alex", "menu");
    CHECK(scoreboard.score("Alex", "menu") == 0);
    CHECK(scoreboard.isTriggerEnabled("Alex", "menu"));

    // parity: a reset unprimes it, because the lock lives on the score that was just thrown away.
    scoreboard.resetScore("Alex", "menu");
    CHECK_FALSE(scoreboard.isTriggerEnabled("Alex", "menu"));

    // Locking something that was never scored is not an error and creates nothing.
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

    // Each field is carried by the one the client reads it from: the prefix and the suffix are not
    // interchangeable, and the visibility on the wire is the name tag's, not the death message's,
    // which is the team's own business and never leaves the server.
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

    // Joining the team they are already on is not a duplicate, and neither is adding them twice
    // directly: vanilla's members are a set.
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
