#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/player/PlayerPersistence.hpp>

#include <doctest.h>

using namespace basalt::server::runtime;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;

TEST_CASE("the effects a player logs out with come back with it") {
    player::PlayerSession before{Uuid{1, 1}, "Steve", 1, player::PlayerConnection{}};
    const Identifier speed = Identifier::vanilla("speed");
    before.effects.insert_or_assign(speed, world::ActiveEffect{.effect = speed,
                                                               .amplifier = 1,
                                                               .remainingTicks = 600,
                                                               .ambient = true,
                                                               .visible = false,
                                                               .showIcon = true});

    const auto stored = player::playerPersistence::toStored(before, "world", 0, 0, 0);
    player::PlayerSession after{Uuid{1, 1}, "Steve", 2, player::PlayerConnection{}};
    player::playerPersistence::restore(after, stored);

    REQUIRE(after.effects.size() == 1);
    const world::ActiveEffect& restored = after.effects.at(speed);
    CHECK(restored.amplifier == 1);
    CHECK(restored.remainingTicks == 600);
    CHECK(restored.ambient);
    CHECK_FALSE(restored.visible);
}

TEST_CASE("air, fire, fall distance and absorption come back with the player") {
    player::PlayerSession before{Uuid{2, 1}, "Alex", 1, player::PlayerConnection{}};
    before.airSupply = 120;
    before.fireTicks = 40;
    before.fallDistance = 3.5;
    before.absorption = 4.0F;

    const auto stored = player::playerPersistence::toStored(before, "world", 0, 0, 0);
    player::PlayerSession after{Uuid{2, 1}, "Alex", 2, player::PlayerConnection{}};
    player::playerPersistence::restore(after, stored);

    CHECK(after.airSupply == 120);
    CHECK(after.fireTicks == 40);
    CHECK(after.fallDistance == 3.5);
    CHECK(after.absorption == 4.0F);
}

TEST_CASE("the place a player died is remembered") {
    player::PlayerSession before{Uuid{3, 1}, "Kim", 1, player::PlayerConnection{}};
    before.lastDeath =
        basalt::wire::protocol::play::GlobalPos{.dimensionName = "minecraft:the_nether", .location = {10, 64, -20}};

    const auto stored = player::playerPersistence::toStored(before, "world", 0, 0, 0);
    player::PlayerSession after{Uuid{3, 1}, "Kim", 2, player::PlayerConnection{}};
    player::playerPersistence::restore(after, stored);

    REQUIRE(after.lastDeath.has_value());
    CHECK(after.lastDeath->dimensionName == "minecraft:the_nether");
    CHECK(after.lastDeath->location.z == -20);
}
