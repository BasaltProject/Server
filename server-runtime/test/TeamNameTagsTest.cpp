#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/scoreboard/TeamNameTags.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <doctest.h>

#include <string>

using basalt::server::runtime::scoreboard::nameTagFor;
using basalt::server::runtime::scoreboard::Team;
using basalt::server::runtime::scoreboard::Teams;
using basalt::wire::protocol::play::NameTagVisibility;
using basalt::wire::protocol::play::TeamColor;
using basalt::wire::text::Component;

namespace {

std::string tag(const Teams& teams, const char* member, const char* viewer) {
    return basalt::wire::text::toLegacyText(nameTagFor(teams, member, viewer));
}

} // namespace

TEST_CASE("a name with no team behind it is drawn as it is") {
    const Teams teams;
    CHECK(tag(teams, "Steve", "Alex") == "Steve");
}

TEST_CASE("the team's colour and affixes go into the name itself") {
    Teams teams;
    Team* red = teams.add("red");
    REQUIRE(red != nullptr);
    red->color = TeamColor::Red;
    red->prefix = Component::text("[R] ");
    red->suffix = Component::text("!");
    teams.join(*red, "Steve");

    // parity: prefix, then the name in the team's colour, then suffix.
    CHECK(tag(teams, "Steve", "Alex") == "[R] §c"
                                         "Steve!");
}

TEST_CASE("who is looking decides whether the name is drawn at all") {
    Teams teams;
    Team* red = teams.add("red");
    REQUIRE(red != nullptr);
    teams.join(*red, "Steve");
    teams.join(*red, "Alex");

    red->nameTagVisibility = NameTagVisibility::Never;
    CHECK(tag(teams, "Steve", "Alex").empty());

    red->nameTagVisibility = NameTagVisibility::HideForOtherTeams;
    CHECK(tag(teams, "Steve", "Alex") == "Steve");
    CHECK(tag(teams, "Steve", "Nobody").empty());

    red->nameTagVisibility = NameTagVisibility::HideForOwnTeam;
    CHECK(tag(teams, "Steve", "Alex").empty());
    CHECK(tag(teams, "Steve", "Nobody") == "Steve");
}
