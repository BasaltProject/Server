#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/EntitySelector.hpp>
#include <basalt/server/runtime/command/ExecutionSource.hpp>

#include <doctest.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using basalt::server::runtime::command::CommandEnvironment;
using basalt::server::runtime::command::CommandSource;
using basalt::server::runtime::command::CommandTarget;
using basalt::server::runtime::command::ConsoleCommandSource;
using basalt::server::runtime::command::EntitySelector;
using basalt::server::runtime::command::ExecutionSource;
using basalt::server::runtime::command::Parsed;
using basalt::server::runtime::command::StringReader;
using basalt::server::runtime::player::PlayerConnection;
using basalt::server::runtime::player::PlayerRegistry;
using basalt::server::runtime::player::PlayerSession;
using basalt::wire::common::Uuid;
using basalt::wire::protocol::play::Gamemode;

namespace {

/// A registry of players plus the environment around them, which is all a selector reads.
struct World {
    PlayerRegistry registry;
    std::vector<std::unique_ptr<PlayerSession>> players;
    CommandEnvironment environment{registry, {}};
    ConsoleCommandSource console;

    PlayerSession& add(const std::string& name, double x = 0.0, double y = 0.0, double z = 0.0) {
        players.push_back(std::make_unique<PlayerSession>(Uuid{static_cast<std::uint64_t>(players.size() + 1), 1}, name,
                                                          static_cast<std::int32_t>(players.size() + 1),
                                                          PlayerConnection{}));
        PlayerSession& session = *players.back();
        session.x = x;
        session.y = y;
        session.z = z;
        registry.add(&session);
        return session;
    }

    [[nodiscard]] std::vector<CommandTarget> select(const std::string& text, bool playersOnly = false) {
        StringReader reader{text};
        const Parsed<EntitySelector> selector = EntitySelector::parse(reader, console, environment);
        REQUIRE(selector.has_value());
        return selector->select(console, environment, playersOnly);
    }

    [[nodiscard]] bool refuses(const std::string& text) {
        StringReader reader{text};
        return !EntitySelector::parse(reader, console, environment).has_value();
    }
};

} // namespace

TEST_CASE("a bare name is one player, matched however it was capitalised") {
    World world;
    world.add("Steve");
    world.add("Alex");

    const std::vector<CommandTarget> found = world.select("steve");

    REQUIRE(found.size() == 1);
    CHECK(found[0].name() == "Steve");
    CHECK(world.select("Nobody").empty());
}

TEST_CASE("a name too long or empty is not a name at all") {
    World world;

    CHECK(world.refuses("ThisNameIsFarTooLongForMinecraft"));
}

TEST_CASE("a uuid picks the player that has it") {
    World world;
    const PlayerSession& steve = world.add("Steve");

    const std::vector<CommandTarget> found = world.select(steve.uuid().toString());

    REQUIRE(found.size() == 1);
    CHECK(found[0].name() == "Steve");
}

TEST_CASE("@a is everyone and @p is the nearest") {
    World world;
    world.add("Far", 100.0, 0.0, 0.0);
    world.add("Near", 1.0, 0.0, 0.0);

    CHECK(world.select("@a").size() == 2);

    const std::vector<CommandTarget> nearest = world.select("@p");
    REQUIRE(nearest.size() == 1);
    CHECK(nearest[0].name() == "Near");
}

TEST_CASE("@s is whoever is running the command, and nobody at the console") {
    World world;
    PlayerSession& steve = world.add("Steve");
    StringReader reader{"@s"};
    const Parsed<EntitySelector> selector = EntitySelector::parse(reader, world.console, world.environment);
    REQUIRE(selector.has_value());

    CHECK(selector->select(world.console, world.environment, false).empty());

    const ExecutionSource asSteve{world.console, ExecutionSource::Changes{.player = &steve}};
    const std::vector<CommandTarget> found = selector->select(asSteve, world.environment, false);
    REQUIRE(found.size() == 1);
    CHECK(found[0].name() == "Steve");
}

TEST_CASE("a limit takes the first few and sort decides which they are") {
    World world;
    world.add("One", 1.0, 0.0, 0.0);
    world.add("Two", 2.0, 0.0, 0.0);
    world.add("Three", 3.0, 0.0, 0.0);

    CHECK(world.select("@a[limit=2]").size() == 2);

    const std::vector<CommandTarget> furthest = world.select("@a[limit=1,sort=furthest]");
    REQUIRE(furthest.size() == 1);
    CHECK(furthest[0].name() == "Three");

    const std::vector<CommandTarget> nearest = world.select("@a[limit=1,sort=nearest]");
    REQUIRE(nearest.size() == 1);
    CHECK(nearest[0].name() == "One");
}

TEST_CASE("a limit of nothing at all is refused") {
    World world;

    CHECK(world.refuses("@a[limit=0]"));
    CHECK(world.refuses("@s[limit=2]"));
    CHECK(world.refuses("@a[limit=1,limit=2]"));
}

TEST_CASE("a distance is measured from wherever the command is run") {
    World world;
    world.add("Close", 3.0, 0.0, 0.0);
    world.add("Far", 30.0, 0.0, 0.0);

    CHECK(world.select("@a[distance=..10]").size() == 1);
    CHECK(world.select("@a[distance=10..]").size() == 1);
    CHECK(world.select("@a[distance=..100]").size() == 2);
    CHECK(world.select("@a[distance=3]").size() == 1);
}

TEST_CASE("a negative or swapped distance is a mistake rather than an empty answer") {
    World world;

    CHECK(world.refuses("@a[distance=-5]"));
    CHECK(world.refuses("@a[distance=10..1]"));
    CHECK(world.refuses("@a[distance=]"));
}

TEST_CASE("a name filter can be inverted, and only once the other way") {
    World world;
    world.add("Steve");
    world.add("Alex");

    CHECK(world.select("@a[name=Steve]").size() == 1);
    CHECK(world.select("@a[name=!Steve]").size() == 1);
    CHECK(world.select("@a[name=!Steve,name=!Alex]").empty());
    // Two positives can never both match, so vanilla refuses the pair outright.
    CHECK(world.refuses("@a[name=Steve,name=Alex]"));
}

TEST_CASE("a game mode filter reads the player's own mode") {
    World world;
    PlayerSession& steve = world.add("Steve");
    world.add("Alex");
    steve.gamemode = Gamemode::Creative;

    CHECK(world.select("@a[gamemode=creative]").size() == 1);
    CHECK(world.select("@a[gamemode=!creative]").size() == 1);
    CHECK(world.refuses("@a[gamemode=nonsense]"));
}

TEST_CASE("a tag filter asks about the scoreboard tags a player carries") {
    World world;
    PlayerSession& steve = world.add("Steve");
    world.add("Alex");
    steve.tags.emplace("chosen");

    CHECK(world.select("@a[tag=chosen]").size() == 1);
    CHECK(world.select("@a[tag=!chosen]").size() == 1);
    // An empty tag asks whether they carry none at all.
    CHECK(world.select("@a[tag=]").size() == 1);
}

TEST_CASE("a level filter is a player's experience and never an entity's") {
    World world;
    PlayerSession& steve = world.add("Steve");
    world.add("Alex");
    steve.experience.restore(30, 0.0F, 1000);

    CHECK(world.select("@a[level=30]").size() == 1);
    CHECK(world.select("@a[level=10..40]").size() == 1);
    CHECK(world.select("@a[level=0]").size() == 1);
    CHECK(world.refuses("@a[level=-1]"));
}

TEST_CASE("a rotation range wraps, so an arc through south is not a swapped range") {
    World world;
    PlayerSession& steve = world.add("Steve");
    steve.yaw = 175.0F;
    world.add("Alex").yaw = 0.0F;

    // parity: `MinMaxBounds.FloatDegrees` does not reject a swapped range.
    CHECK(world.select("@a[y_rotation=170..-170]").size() == 1);
    CHECK(world.select("@a[y_rotation=-10..10]").size() == 1);
}

TEST_CASE("a box filter is anchored where the command was run") {
    World world;
    world.add("Inside", 2.0, 0.0, 2.0);
    world.add("Outside", 20.0, 0.0, 20.0);

    CHECK(world.select("@a[dx=5,dy=5,dz=5]").size() == 1);
    CHECK(world.select("@a[x=18,y=-1,z=18,dx=5,dy=5,dz=5]").size() == 1);
}

TEST_CASE("a type filter takes a real type and refuses one nobody registered") {
    World world;
    world.add("Steve");

    CHECK(world.select("@e[type=player]").size() == 1);
    CHECK(world.select("@e[type=cow]").empty());
    CHECK(world.refuses("@e[type=minecraft:not_a_thing]"));
    // Two positive types can never both match.
    CHECK(world.refuses("@e[type=cow,type=pig]"));
    CHECK_FALSE(world.refuses("@e[type=!cow,type=!pig]"));
}

TEST_CASE("an unknown option is a syntax error rather than an empty answer") {
    World world;

    CHECK(world.refuses("@a[nonsense=1]"));
    CHECK(world.refuses("@a[name]"));
    CHECK(world.refuses("@a[name=Steve"));
    CHECK(world.refuses("@x"));
    CHECK(world.refuses("@"));
}

TEST_CASE("an nbt filter compares against the view the data command answers with") {
    World world;
    PlayerSession& steve = world.add("Steve");
    steve.setHealth(7.0F);
    world.add("Alex");

    CHECK(world.select("@a[nbt={Health:7.0f}]").size() == 1);
    CHECK(world.select("@a[nbt=!{Health:7.0f}]").size() == 1);
    CHECK(world.select("@a[nbt={Name:\"Steve\"}]").size() == 1);
}

TEST_CASE("a players-only argument sees players even through @e") {
    World world;
    world.add("Steve");

    CHECK(world.select("@e", true).size() == 1);
    CHECK(world.select("@e", false).size() == 1);
}

TEST_CASE("a scores filter with no scoreboard behind it matches nobody") {
    World world;
    world.add("Steve");

    CHECK(world.select("@a[scores={kills=1..}]").empty());
    // An empty set is not a filter at all.
    CHECK(world.select("@a[scores={}]").size() == 1);
    CHECK(world.refuses("@a[scores={kills=1},scores={deaths=1}]"));
}

TEST_CASE("a predicate nobody loaded matches nobody, negated or not") {
    World world;
    world.add("Steve");

    // parity: an unknown predicate is false in vanilla too, even when negated.
    CHECK(world.select("@a[predicate=minecraft:nothing]").empty());
    CHECK(world.select("@a[predicate=!minecraft:nothing]").empty());
}

TEST_CASE("whitespace inside the brackets is skipped the way vanilla skips it") {
    World world;
    world.add("Steve");

    CHECK(world.select("@a[ name = Steve ]").size() == 1);
}
