#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/entity/FlyingMovement.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <doctest.h>

using namespace basalt::server::runtime::entity;
using basalt::server::runtime::world::WorldState;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

struct Loaded {
    WorldState state = emptyWorld();

    Loaded() { state.getOrGenerateChunk(0, 0); }
};

[[nodiscard]] ServerEntity ghast(double x, double y, double z) {
    return ServerEntity{1, basalt::wire::common::Uuid::random(), Identifier::vanilla("bat"), 1, x, y, z};
}

} // namespace

TEST_CASE("leaning towards a point adds a step of speed along the way there") {
    ServerEntity flier = ghast(8.0, 64.0, 8.0);

    CHECK(flyingMovement::approach(flier, 18.0, 64.0, 8.0, 0.1));

    CHECK(flier.velocityX == doctest::Approx(0.1));
    CHECK(flier.velocityY == doctest::Approx(0.0));
    CHECK(flier.velocityZ == doctest::Approx(0.0));

    // The step is the length of the nudge, however far away the point is.
    ServerEntity diagonal = ghast(0.0, 0.0, 0.0);
    CHECK(flyingMovement::approach(diagonal, 3.0, 0.0, 4.0, 0.5));
    CHECK(diagonal.velocityX == doctest::Approx(0.3));
    CHECK(diagonal.velocityZ == doctest::Approx(0.4));

    // And it adds to what was already there rather than replacing it.
    CHECK(flyingMovement::approach(flier, 18.0, 64.0, 8.0, 0.1));
    CHECK(flier.velocityX == doctest::Approx(0.2));
}

TEST_CASE("a flier that has arrived says so and is left alone") {
    ServerEntity flier = ghast(8.0, 64.0, 8.0);

    CHECK_FALSE(flyingMovement::approach(flier, 8.2, 64.0, 8.0, 0.1));
    CHECK(flier.velocityX == doctest::Approx(0.0));
    // Exactly three tenths away is not yet arrived, and neither is anything further.
    CHECK(flyingMovement::approach(flier, 8.3, 64.0, 8.0, 0.1));
    CHECK(flier.velocityX == doctest::Approx(0.1));
    ServerEntity onTheLine = ghast(8.0, 64.0, 8.0);
    CHECK(flyingMovement::approach(onTheLine, 8.0, 64.0, 8.3, 0.1));
    CHECK(onTheLine.velocityZ == doctest::Approx(0.1));
}

TEST_CASE("a tick in the open moves the flier and the air keeps most of the speed") {
    Loaded loaded;
    ServerEntity flier = ghast(8.0, 64.0, 8.0);
    flier.velocityX = 1.0;
    flier.velocityY = 0.5;
    flier.velocityZ = -0.25;

    flyingMovement::tick(loaded.state, flier);

    CHECK(flier.x == doctest::Approx(9.0));
    CHECK(flier.y == doctest::Approx(64.5));
    CHECK(flier.z == doctest::Approx(7.75));
    CHECK(flier.velocityX == doctest::Approx(0.91));
    CHECK(flier.velocityY == doctest::Approx(0.455));
    CHECK(flier.velocityZ == doctest::Approx(-0.2275));
    // Nothing that flies is ever standing on anything.
    CHECK_FALSE(flier.onGround);
    CHECK_FALSE(flier.horizontallyBlocked);
}

TEST_CASE("a flier that hits a wall loses that axis and says it was blocked") {
    Loaded loaded;
    REQUIRE(loaded.state.setBlockState(10, 64, 8, stateOf("stone")));
    ServerEntity flier = ghast(8.0, 64.0, 8.0);
    flier.velocityX = 5.0;
    flier.velocityZ = 0.1;

    flyingMovement::tick(loaded.state, flier);

    CHECK(flier.velocityX == doctest::Approx(0.0));
    CHECK(flier.horizontallyBlocked);
    CHECK(flier.x < 10.0);
    CHECK(flier.velocityZ == doctest::Approx(0.091));
    CHECK_FALSE(flier.onGround);
}

TEST_CASE("a flier stopped from below loses only its fall") {
    Loaded loaded;
    REQUIRE(loaded.state.setBlockState(8, 63, 8, stateOf("stone")));
    ServerEntity flier = ghast(8.0, 64.0, 8.0);
    flier.velocityY = -1.0;
    flier.velocityX = 0.2;

    flyingMovement::tick(loaded.state, flier);

    CHECK(flier.velocityY == doctest::Approx(0.0));
    CHECK(flier.y == doctest::Approx(64.0));
    // Being stopped below is not being blocked sideways, and it is still not on the ground.
    CHECK_FALSE(flier.horizontallyBlocked);
    CHECK_FALSE(flier.onGround);
    CHECK(flier.velocityX == doctest::Approx(0.182));
}

TEST_CASE("the eleven mobs that fly rather than walk") {
    for (const char* name : {"ghast", "happy_ghast", "ghastling", "blaze", "phantom", "vex", "bat", "allay", "wither",
                             "ender_dragon", "breeze"}) {
        CHECK(flyers::flies(Identifier::vanilla(name)));
    }
    CHECK_FALSE(flyers::flies(Identifier::vanilla("zombie")));
    CHECK_FALSE(flyers::flies(Identifier::vanilla("bee")));
    CHECK_FALSE(flyers::flies(Identifier::vanilla("parrot")));
    // The name is read without its namespace, so a datapack ghast flies like the real one.
    CHECK(flyers::flies(Identifier{"mypack", "ghast"}));
}
