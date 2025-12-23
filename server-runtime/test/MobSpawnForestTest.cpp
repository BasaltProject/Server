#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/MobSpawnForest.hpp>

#include <doctest.h>

using basalt::server::runtime::world::MobSpawnForest;
using basalt::server::runtime::world::MobSpawnForestBuilder;
using basalt::server::runtime::world::MobSpawnNode;
using basalt::server::world::ChunkGeneratedEntity;
using basalt::wire::common::Identifier;

TEST_CASE("a passenger rides its vehicle and the frozen tree carries it") {
    MobSpawnForestBuilder builder;
    MobSpawnNode& chicken = builder.create(Identifier::vanilla("chicken"), 8.0, 64.0, 8.0, 0.0F);
    MobSpawnNode& zombie = builder.create(Identifier::vanilla("zombie"), 8.0, 64.0, 8.0, 0.0F);
    CHECK(builder.mount(zombie, chicken));

    const MobSpawnForest forest = builder.freeze(zombie);
    // One root, the chicken, with the zombie on top: a jockey is one entity as far as a chunk is
    // concerned, and the mob that was asked for is still findable inside it.
    REQUIRE(forest.roots.size() == 1);
    CHECK(forest.roots[0].type == Identifier::vanilla("chicken"));
    REQUIRE(forest.roots[0].passengers.size() == 1);
    CHECK(forest.roots[0].passengers[0].type == Identifier::vanilla("zombie"));
    CHECK(forest.main.type == Identifier::vanilla("zombie"));
}

TEST_CASE("nothing rides itself, or anything already riding it") {
    MobSpawnForestBuilder builder;
    MobSpawnNode& first = builder.create(Identifier::vanilla("chicken"), 0.0, 0.0, 0.0, 0.0F);
    MobSpawnNode& second = builder.create(Identifier::vanilla("zombie"), 0.0, 0.0, 0.0, 0.0F);

    CHECK_FALSE(builder.mount(first, first));
    CHECK(builder.mount(second, first));
    // The chicken cannot climb onto the zombie already riding it: that is a loop, not a stack.
    CHECK_FALSE(builder.mount(first, second));
}

TEST_CASE("mounting again moves a passenger rather than leaving it on two vehicles") {
    MobSpawnForestBuilder builder;
    MobSpawnNode& chicken = builder.create(Identifier::vanilla("chicken"), 0.0, 0.0, 0.0, 0.0F);
    MobSpawnNode& spider = builder.create(Identifier::vanilla("spider"), 0.0, 0.0, 0.0, 0.0F);
    MobSpawnNode& rider = builder.create(Identifier::vanilla("skeleton"), 0.0, 0.0, 0.0, 0.0F);

    CHECK(builder.mount(rider, chicken));
    CHECK(builder.mount(rider, spider));
    CHECK(chicken.passengers().empty());
    CHECK(spider.passengers().size() == 1);

    // An external mount leaves the forest entirely: the rider becomes a root with a link.
    CHECK(builder.mountExternal(rider, 77));
    const MobSpawnForest forest = builder.freeze(rider);
    CHECK(spider.passengers().empty());
    REQUIRE(forest.externalVehicleLinks.size() == 1);
    CHECK(forest.externalVehicleLinks[0].vehicleId == 77);
    CHECK(forest.externalVehicleLinks[0].passenger.type == Identifier::vanilla("skeleton"));
}

TEST_CASE("relocating the main entity leaves whatever it is riding where it stands") {
    MobSpawnForestBuilder builder;
    MobSpawnNode& horse = builder.create(Identifier::vanilla("horse"), 1.0, 64.0, 1.0, 0.0F);
    MobSpawnNode& skeleton = builder.create(Identifier::vanilla("skeleton"), 1.0, 64.0, 1.0, 0.0F);
    CHECK(builder.mount(skeleton, horse));

    const MobSpawnForest moved = relocateMain(builder.freeze(horse), 20.0, 70.0, 30.0, 90.0F);
    CHECK(moved.main.x == 20.0);
    CHECK(moved.main.yaw == 90.0F);
    REQUIRE(moved.roots.size() == 1);
    CHECK(moved.roots[0].x == 20.0);
    // The rider keeps its own position: the caller places the vehicle and the mount does the rest.
    REQUIRE(moved.roots[0].passengers.size() == 1);
    CHECK(moved.roots[0].passengers[0].x == 1.0);
}
