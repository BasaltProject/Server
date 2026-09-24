#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/entity/FlyingMovement.hpp>
#include <basalt/server/runtime/entity/PathTypes.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <doctest.h>

#include <cstdint>

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

[[nodiscard]] ServerEntity mob(const char* type, double x, double y, double z) {
    return ServerEntity{1, basalt::wire::common::Uuid::random(), Identifier::vanilla(type), 1, x, y, z};
}

[[nodiscard]] ServerEntity ghast(double x, double y, double z) { return mob("ghast", x, y, z); }

basalt::server::random::RandomSource rolls = basalt::server::random::RandomSource::legacy(1);

} // namespace

TEST_CASE("a ghast with nowhere to go drifts, and the air keeps 0.91 of it") {
    Loaded loaded;
    ServerEntity flier = ghast(8.0, 64.0, 8.0);
    flier.velocityX = 1.0;
    flier.velocityY = 0.5;
    flier.velocityZ = -0.25;

    flyingMovement::tick(loaded.state, flier, rolls);

    CHECK(flier.x == doctest::Approx(9.0));
    CHECK(flier.y == doctest::Approx(64.5));
    CHECK(flier.z == doctest::Approx(7.75));
    CHECK(flier.velocityX == doctest::Approx(0.91));
    CHECK(flier.velocityY == doctest::Approx(0.455));
    CHECK(flier.velocityZ == doctest::Approx(-0.2275));
    CHECK_FALSE(flier.horizontallyBlocked);
}

TEST_CASE("a flier that hits a wall loses that axis and says it was blocked") {
    Loaded loaded;
    REQUIRE(loaded.state.setBlockState(11, 64, 8, stateOf("stone")));
    ServerEntity flier = ghast(8.0, 64.0, 8.0);
    flier.velocityX = 1.5;
    flier.velocityZ = 0.1;

    flyingMovement::tick(loaded.state, flier, rolls);

    CHECK(flier.velocityX == doctest::Approx(0.0));
    CHECK(flier.horizontallyBlocked);
    CHECK(flier.x == doctest::Approx(9.0));
    CHECK(flier.velocityZ == doctest::Approx(0.091));
}

TEST_CASE("a ghast pushes a tenth of a block at a point it can reach and gives up on one behind a wall") {
    Loaded loaded;
    ServerEntity flier = ghast(8.0, 64.0, 8.0);
    flier.wantedX = 14.0;
    flier.wantedY = 64.0;
    flier.wantedZ = 8.0;
    flier.moving = true;
    flyingMovement::tick(loaded.state, flier, rolls);
    // parity: flying speed 0.06 times five thirds, then the air's 0.91.
    CHECK(flier.velocityX == doctest::Approx(0.1 * 0.91));
    CHECK(flier.moving);

    for (std::int32_t y = 60; y < 72; ++y) {
        for (std::int32_t z = 2; z < 14; ++z) {
            REQUIRE(loaded.state.setBlockState(12, y, z, stateOf("stone")));
        }
    }
    ServerEntity walled = ghast(6.0, 64.0, 8.0);
    walled.wantedX = 15.0;
    walled.wantedY = 64.0;
    walled.wantedZ = 8.0;
    walled.moving = true;
    flyingMovement::tick(loaded.state, walled, rolls);
    CHECK_FALSE(walled.moving);
    CHECK(walled.velocityX == doctest::Approx(0.0));
}

TEST_CASE("a flying move control climbs towards what it wants and floats while it does") {
    Loaded loaded;
    ServerEntity bee = mob("bee", 8.0, 64.0, 8.0);
    bee.wantedX = 8.0;
    bee.wantedY = 70.0;
    bee.wantedZ = 12.0;
    bee.moveSpeed = 1.0;
    bee.moving = true;
    flyingMovement::tick(loaded.state, bee, rolls);
    CHECK(bee.noGravity);
    CHECK_FALSE(bee.moving);
    CHECK(bee.velocityY > 0.0);
    CHECK(bee.velocityZ > 0.0);
}

TEST_CASE("a vex flies straight through stone") {
    Loaded loaded;
    REQUIRE(loaded.state.setBlockState(9, 64, 8, stateOf("stone")));
    ServerEntity vex = mob("vex", 8.5, 64.0, 8.5);
    vex.velocityX = 1.0;
    flyingMovement::tick(loaded.state, vex, rolls);
    CHECK(vex.x == doctest::Approx(9.5));
    CHECK_FALSE(vex.horizontallyBlocked);
}

TEST_CASE("the mobs that fly rather than walk") {
    for (const char* name : {"ghast", "happy_ghast", "ghastling", "phantom", "vex", "bat", "allay", "wither",
                             "ender_dragon", "breeze", "bee", "parrot"}) {
        CHECK(flyers::flies(Identifier::vanilla(name)));
    }
    CHECK_FALSE(flyers::flies(Identifier::vanilla("zombie")));
    // parity: a blaze walks, with its own slow fall.
    CHECK_FALSE(flyers::flies(Identifier::vanilla("blaze")));
    CHECK(flyers::flies(Identifier{"mypack", "ghast"}));
}

TEST_CASE("a square is priced by what it is and what lies under and around it") {
    namespace types = pathTypes;
    Loaded loaded;
    REQUIRE(loaded.state.setBlockState(2, 63, 2, stateOf("stone")));
    REQUIRE(loaded.state.setBlockState(6, 63, 6, stateOf("oak_fence")));
    REQUIRE(loaded.state.setBlockState(10, 63, 10, stateOf("lava")));
    REQUIRE(loaded.state.setBlockState(12, 64, 12, stateOf("cocoa")));
    CHECK(types::flying(loaded.state, 2, 64, 2, -64, std::nullopt) == types::PathType::Walkable);
    CHECK(types::flying(loaded.state, 2, 70, 2, -64, std::nullopt) == types::PathType::Open);
    CHECK(types::flying(loaded.state, 6, 64, 6, -64, std::nullopt) == types::PathType::Fence);
    // parity: the fence the mob stands on is not held against it.
    CHECK(types::flying(loaded.state, 6, 64, 6, -64, std::array<std::int32_t, 3>{6, 63, 6}) != types::PathType::Fence);
    CHECK(types::flying(loaded.state, 10, 64, 10, -64, std::nullopt) == types::PathType::Fire);
    CHECK(types::flying(loaded.state, 11, 63, 10, -64, std::nullopt) == types::PathType::FireInNeighbor);
    CHECK(types::flying(loaded.state, 12, 65, 12, -64, std::nullopt) == types::PathType::Cocoa);
    CHECK(types::malusOf(types::of(Identifier::vanilla("parrot")), types::PathType::Fire) < 0.0F);
    CHECK(types::malusOf(types::of(Identifier::vanilla("wither")), types::PathType::Fire) == doctest::Approx(16.0F));
    CHECK(types::malusOf(types::of(Identifier::vanilla("bee")), types::PathType::Fence) < 0.0F);
}
