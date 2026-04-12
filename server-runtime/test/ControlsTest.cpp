#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <doctest.h>

#include <cstdint>
#include <memory>

using basalt::server::runtime::entity::ServerEntity;
using basalt::server::runtime::world::WorldState;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
namespace ai = basalt::server::runtime::entity::ai;

namespace {

[[nodiscard]] std::unique_ptr<ServerEntity> zombieAt(double x, double y, double z) {
    const basalt::server::registry::EntityTypeEntry* type =
        basalt::server::registry::entityTypes::of(Identifier::vanilla("zombie"));
    REQUIRE(type != nullptr);
    return std::make_unique<ServerEntity>(1, Uuid{1, 1}, type->id, type->protocolId, x, y, z);
}

} // namespace

TEST_CASE("a turn takes the shortest way round and never more than it was allowed") {
    // Straight across the wrap: from 170 to -170 is twenty degrees the short way, not 340.
    CHECK(ai::moveControl::rotlerp(170.0F, -170.0F, 90.0F) == doctest::Approx(190.0F));
    CHECK(ai::moveControl::rotlerp(0.0F, 45.0F, 90.0F) == doctest::Approx(45.0F));
    // Capped: ninety degrees a tick is all a mob turns, however far it wants to.
    CHECK(ai::moveControl::rotlerp(0.0F, 120.0F, 90.0F) == doctest::Approx(90.0F));
    // parity: `Mth.wrapDegrees` folds exactly 180 to -180, so a half turn goes the negative way.
    CHECK(ai::moveControl::rotlerp(0.0F, 180.0F, 90.0F) == doctest::Approx(-90.0F));
    CHECK(ai::moveControl::rotlerp(0.0F, -180.0F, 90.0F) == doctest::Approx(-90.0F));

    CHECK(ai::moveControl::wrapDegrees(370.0F) == doctest::Approx(10.0F));
    CHECK(ai::moveControl::wrapDegrees(-370.0F) == doctest::Approx(-10.0F));
    CHECK(ai::moveControl::wrapDegrees(180.0F) == doctest::Approx(-180.0F));
}

TEST_CASE("a mob with no path is done, and stopping one clears what it was walking to") {
    WorldState world;
    const std::unique_ptr<ServerEntity> mob = zombieAt(0.5, 64.0, 0.5);
    CHECK(ai::navigation::isDone(*mob));

    mob->path = std::vector<std::array<std::int32_t, 3>>{{0, 64, 0}, {1, 64, 0}};
    mob->pathIndex = 0;
    CHECK_FALSE(ai::navigation::isDone(*mob));

    ai::navigation::stop(*mob);
    CHECK(ai::navigation::isDone(*mob));
    CHECK_FALSE(mob->walkTarget.has_value());
    CHECK(mob->pathIndex == 0);
}

TEST_CASE("walking a path steps to the next node once the mob is close enough to this one") {
    WorldState world;
    const std::unique_ptr<ServerEntity> mob = zombieAt(0.5, 64.0, 0.5);
    mob->path = std::vector<std::array<std::int32_t, 3>>{{0, 64, 0}, {3, 64, 0}};
    mob->pathIndex = 0;
    mob->moveSpeed = 1.0;

    ai::navigation::tick(world, *mob);
    // Standing on the first node, so it moves on and aims at the second.
    CHECK(mob->pathIndex == 1);
    CHECK(mob->moving);
    CHECK(mob->wantedX == doctest::Approx(3.5));

    // Arriving at the last node ends the path.
    mob->x = 3.5;
    mob->z = 0.5;
    ai::navigation::tick(world, *mob);
    CHECK(ai::navigation::isDone(*mob));
}

TEST_CASE("looking at something turns the head towards it, and it drifts back when nothing asks") {
    const std::unique_ptr<ServerEntity> mob = zombieAt(0.0, 64.0, 0.0);
    ai::lookControl::lookAt(*mob, 0.0, 64.0, 10.0, 90.0F, 90.0F);
    ai::lookControl::tick(*mob);
    // Due south is yaw zero in Minecraft's own convention, which is what the arctangent gives.
    CHECK(mob->headYaw == doctest::Approx(0.0F));

    mob->headYaw = 100.0F;
    mob->lookTicks = 0;
    ai::lookControl::tick(*mob);
    // Ten degrees a tick back towards the body, not the whole way at once.
    CHECK(mob->headYaw == doctest::Approx(90.0F));
}

TEST_CASE("standing in water is what the block at the mob's feet says") {
    WorldState world;
    world.getOrGenerateChunk(0, 0);
    const std::unique_ptr<ServerEntity> mob = zombieAt(8.5, 64.0, 8.5);
    CHECK_FALSE(ai::inWater(world, *mob));

    REQUIRE(world.setBlockState(8, 64, 8, basalt::server::registry::blocks::named("water")->stateId));
    CHECK(ai::inWater(world, *mob));
    // A block with no collision box is one a mob can stand in; a full one is not.
    CHECK(ai::isPassable(world, 8, 70, 8));
    REQUIRE(world.setBlockState(8, 70, 8, basalt::server::registry::blocks::named("stone")->stateId));
    CHECK_FALSE(ai::isPassable(world, 8, 70, 8));
}

TEST_CASE("a mob crossing flat ground keeps its feet on it") {
    WorldState world;
    world.getOrGenerateChunk(0, 0);
    world.getOrGenerateChunk(1, 0);
    const std::int32_t stone = basalt::server::registry::blocks::named("stone")->stateId;
    for (std::int32_t x = 0; x < 32; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            REQUIRE(world.setBlockState(x, 64, z, stone));
        }
    }
    const std::unique_ptr<ServerEntity> mob = zombieAt(8.5, 65.0, 8.5);
    mob->onGround = true;
    std::int32_t airborne = 0;
    for (std::int32_t tick = 0; tick < 40; ++tick) {
        ai::moveControl::setWanted(*mob, 20.0, 65.0, 8.5, 1.0);
        ai::moveControl::tick(world, *mob, 0.25);
        airborne += mob->onGround ? 0 : 1;
    }
    // parity: `Mth.equal`'s slack; comparing exactly calls a rounding error a wall and hops it.
    CHECK(airborne <= 1);
    CHECK(mob->x > 12.0);
}
