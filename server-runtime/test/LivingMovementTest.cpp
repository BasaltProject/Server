#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/entity/LivingMovement.hpp>
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

    /// A mob standing on a floor of `ground` at (8.5, 64, 8.5).
    [[nodiscard]] ServerEntity standing(const std::string& ground) {
        REQUIRE(state.setBlockState(8, 63, 8, stateOf(ground)));
        ServerEntity mob{1, basalt::wire::common::Uuid::random(), Identifier::vanilla("zombie"), 1, 8.5, 64.0, 8.5};
        mob.onGround = true;
        return mob;
    }
};

/// parity: `Attributes.MOVEMENT_SPEED`'s own default for a walking mob.
constexpr double kWalkSpeed = 0.23;

} // namespace

TEST_CASE("walking forward moves along the yaw the mob is facing") {
    Loaded loaded;
    ServerEntity mob = loaded.standing("stone");
    mob.yaw = 0.0F;

    livingMovement::travel(loaded.state, mob, 1.0, 0.0, kWalkSpeed);

    // parity: yaw zero is south, so forward is +z and nothing else.
    CHECK(mob.z > 8.5);
    CHECK(mob.x == doctest::Approx(8.5));

    ServerEntity west = loaded.standing("stone");
    west.yaw = 90.0F;
    livingMovement::travel(loaded.state, west, 1.0, 0.0, kWalkSpeed);
    CHECK(west.x < 8.5);
    CHECK(west.z == doctest::Approx(8.5));
}

TEST_CASE("strafing goes across the facing, and both together are not faster than one") {
    Loaded loaded;
    ServerEntity strafing = loaded.standing("stone");
    strafing.yaw = 0.0F;
    livingMovement::travel(loaded.state, strafing, 0.0, 1.0, kWalkSpeed);
    const double sideways = strafing.x - 8.5;
    CHECK(sideways > 0.0);
    CHECK(strafing.z == doctest::Approx(8.5));

    // parity: `getInputVector` scales an input longer than one back to one.
    ServerEntity both = loaded.standing("stone");
    both.yaw = 0.0F;
    livingMovement::travel(loaded.state, both, 1.0, 1.0, kWalkSpeed);
    const double dx = both.x - 8.5;
    const double dz = both.z - 8.5;
    CHECK(std::sqrt((dx * dx) + (dz * dz)) == doctest::Approx(sideways));
}

TEST_CASE("no input at all leaves the mob where it stands") {
    Loaded loaded;
    ServerEntity mob = loaded.standing("stone");

    livingMovement::travel(loaded.state, mob, 0.0, 0.0, kWalkSpeed);

    CHECK(mob.x == doctest::Approx(8.5));
    CHECK(mob.z == doctest::Approx(8.5));
    CHECK(mob.velocityX == doctest::Approx(0.0));
    CHECK(mob.velocityZ == doctest::Approx(0.0));
}

TEST_CASE("a mob only counts as on the ground on the tick it lands") {
    Loaded loaded;
    ServerEntity mob = loaded.standing("stone");

    // parity: gravity comes off after the move, so a mob starting from rest has nothing pushing it
    // down this tick and is not standing on anything by the end of it.
    livingMovement::travel(loaded.state, mob, 0.0, 0.0, kWalkSpeed);
    CHECK_FALSE(mob.onGround);

    // The next tick falls into the floor, and that is the one that lands.
    livingMovement::travel(loaded.state, mob, 0.0, 0.0, kWalkSpeed);
    CHECK(mob.onGround);
    CHECK(mob.y == doctest::Approx(64.0));
}

TEST_CASE("ice takes back far less of the speed than stone, which is what makes it slide") {
    // The first step on ice is the shorter one: the input is divided by the cube of the friction.
    Loaded stony;
    ServerEntity onStone = stony.standing("stone");
    onStone.yaw = 0.0F;
    livingMovement::travel(stony.state, onStone, 1.0, 0.0, kWalkSpeed);

    Loaded slippery;
    ServerEntity onIce = slippery.standing("ice");
    onIce.yaw = 0.0F;
    livingMovement::travel(slippery.state, onIce, 1.0, 0.0, kWalkSpeed);
    CHECK(onIce.velocityZ < onStone.velocityZ);

    // What carries it is the tick after the input stops: stone keeps 0.546 of the speed, ice 0.891.
    ServerEntity stoneCoast = stony.standing("stone");
    stoneCoast.velocityZ = 1.0;
    livingMovement::travel(stony.state, stoneCoast, 0.0, 0.0, kWalkSpeed);
    CHECK(stoneCoast.velocityZ == doctest::Approx(0.6 * 0.91));

    ServerEntity iceCoast = slippery.standing("ice");
    iceCoast.velocityZ = 1.0;
    livingMovement::travel(slippery.state, iceCoast, 0.0, 0.0, kWalkSpeed);
    CHECK(iceCoast.velocityZ == doctest::Approx(0.98 * 0.91));
}

TEST_CASE("the ground's own friction only counts while standing on it") {
    Loaded loaded;
    ServerEntity falling = loaded.standing("ice");
    falling.onGround = false;
    falling.yaw = 0.0F;

    livingMovement::travel(loaded.state, falling, 1.0, 0.0, kWalkSpeed);

    // parity: in the air the input is the flying speed and the drag is the air's alone.
    CHECK(falling.velocityZ == doctest::Approx(0.02 * 0.91));
    CHECK(falling.velocityY == doctest::Approx(-0.08 * 0.98));
}

TEST_CASE("gravity comes off after the move and the drag after that") {
    Loaded loaded;
    ServerEntity mob = loaded.standing("stone");
    mob.onGround = false;
    mob.y = 70.0;

    livingMovement::travel(loaded.state, mob, 0.0, 0.0, kWalkSpeed);

    CHECK(mob.y == doctest::Approx(70.0));
    CHECK(mob.velocityY == doctest::Approx(-0.08 * 0.98));

    // A second tick falls by what the first tick left behind, and falling is not standing.
    livingMovement::travel(loaded.state, mob, 0.0, 0.0, kWalkSpeed);
    CHECK(mob.y == doctest::Approx(70.0 - (0.08 * 0.98)));
    CHECK(mob.velocityY == doctest::Approx(((-0.08 * 0.98) - 0.08) * 0.98));
    CHECK_FALSE(mob.onGround);
}

TEST_CASE("a mob that walks into a slab climbs it instead of stopping") {
    Loaded loaded;
    ServerEntity mob = loaded.standing("stone");
    mob.yaw = 0.0F;
    REQUIRE(loaded.state.setBlockState(8, 64, 9, stateOf("oak_slab")));
    mob.velocityZ = 0.5;

    livingMovement::travel(loaded.state, mob, 0.0, 0.0, kWalkSpeed);

    // parity: six tenths of step height, which half a block is under and a whole one is not.
    CHECK(mob.y == doctest::Approx(64.5));
    CHECK(mob.z > 8.5);
    CHECK_FALSE(mob.horizontallyBlocked);
}

TEST_CASE("a mob that walks into a wall says so and loses that axis") {
    Loaded loaded;
    ServerEntity mob = loaded.standing("stone");
    mob.yaw = 0.0F;
    REQUIRE(loaded.state.setBlockState(8, 64, 9, stateOf("stone")));
    REQUIRE(loaded.state.setBlockState(8, 65, 9, stateOf("stone")));
    mob.velocityZ = 0.5;

    livingMovement::travel(loaded.state, mob, 0.0, 0.0, kWalkSpeed);

    CHECK(mob.horizontallyBlocked);
    CHECK(mob.velocityZ == doctest::Approx(0.0));
    CHECK(mob.y == doctest::Approx(64.0));
}

TEST_CASE("swimming is slow to start and sinks a sixteenth of the way down") {
    Loaded loaded;
    ServerEntity swimmer{1, basalt::wire::common::Uuid::random(), Identifier::vanilla("zombie"), 1, 8.5, 70.0, 8.5};
    swimmer.yaw = 0.0F;

    livingMovement::travelInWater(loaded.state, swimmer, 1.0, 0.0);

    // parity: a fiftieth of a block of input, eight tenths of it kept.
    CHECK(swimmer.velocityZ == doctest::Approx(0.02 * 0.8));
    CHECK(swimmer.z == doctest::Approx(70.0 - 70.0 + 8.5 + 0.02));
    CHECK(swimmer.velocityY == doctest::Approx(-0.08 / 16.0));
    CHECK_FALSE(swimmer.onGround);

    // A second tick keeps eight tenths of the sink and adds another sixteenth of gravity.
    livingMovement::travelInWater(loaded.state, swimmer, 0.0, 0.0);
    CHECK(swimmer.velocityY == doctest::Approx((-0.08 / 16.0 * 0.8) - (0.08 / 16.0)));
}

TEST_CASE("a swimmer that reaches the bottom stops sinking and stands on it") {
    Loaded loaded;
    REQUIRE(loaded.state.setBlockState(8, 63, 8, stateOf("stone")));
    ServerEntity swimmer{1, basalt::wire::common::Uuid::random(), Identifier::vanilla("zombie"), 1, 8.5, 64.0, 8.5};
    swimmer.velocityY = -0.5;

    livingMovement::travelInWater(loaded.state, swimmer, 0.0, 0.0);

    CHECK(swimmer.y == doctest::Approx(64.0));
    CHECK(swimmer.onGround);
    CHECK(swimmer.velocityY == doctest::Approx(-0.08 / 16.0));
}

TEST_CASE("a jump only happens from the ground, and the block under it decides how high") {
    Loaded loaded;
    ServerEntity mob = loaded.standing("stone");

    livingMovement::jump(loaded.state, mob);

    CHECK(mob.velocityY == doctest::Approx(0.42));
    CHECK_FALSE(mob.onGround);

    // Once in the air, another jump does nothing at all.
    mob.velocityY = 0.0;
    livingMovement::jump(loaded.state, mob);
    CHECK(mob.velocityY == doctest::Approx(0.0));

    // parity: honey is the block whose jump factor is not one.
    Loaded sticky;
    ServerEntity onHoney = sticky.standing("honey_block");
    livingMovement::jump(sticky.state, onHoney);
    CHECK(onHoney.velocityY < 0.42);
    CHECK(onHoney.velocityY > 0.0);
}

TEST_CASE("a swimmer moves on each axis by its own share of the speed") {
    Loaded loaded;
    ServerEntity swimmer{1, basalt::wire::common::Uuid::random(), Identifier::vanilla("zombie"), 1, 8.5, 70.0, 8.5};
    swimmer.velocityX = 0.1;
    swimmer.velocityY = 0.2;
    swimmer.velocityZ = 0.3;

    livingMovement::travelInWater(loaded.state, swimmer, 0.0, 0.0);

    CHECK(swimmer.x == doctest::Approx(8.6));
    CHECK(swimmer.y == doctest::Approx(70.2));
    CHECK(swimmer.z == doctest::Approx(8.8));
    CHECK(swimmer.velocityX == doctest::Approx(0.1 * 0.8));
    CHECK(swimmer.velocityZ == doctest::Approx(0.3 * 0.8));
    CHECK(swimmer.velocityY == doctest::Approx((0.2 * 0.8) - (0.08 / 16.0)));
    CHECK_FALSE(swimmer.horizontallyBlocked);
    CHECK_FALSE(swimmer.onGround);
}

TEST_CASE("a swimmer that hits a wall loses that axis and says it was blocked") {
    const auto against = [](std::int32_t wallX, std::int32_t wallZ, double dx, double dz) {
        Loaded loaded;
        REQUIRE(loaded.state.setBlockState(wallX, 70, wallZ, stateOf("stone")));
        ServerEntity swimmer{1, basalt::wire::common::Uuid::random(), Identifier::vanilla("zombie"), 1, 8.5, 70.0, 8.5};
        swimmer.velocityX = dx;
        swimmer.velocityZ = dz;
        livingMovement::travelInWater(loaded.state, swimmer, 0.0, 0.0);
        return swimmer;
    };

    const ServerEntity eastwards = against(10, 8, 5.0, 0.25);
    CHECK(eastwards.horizontallyBlocked);
    CHECK(eastwards.velocityX == doctest::Approx(0.0));
    CHECK(eastwards.x < 10.0);
    // The axis that was not stopped keeps its own speed.
    CHECK(eastwards.velocityZ == doctest::Approx(0.25 * 0.8));

    const ServerEntity southwards = against(8, 10, 0.25, 5.0);
    CHECK(southwards.horizontallyBlocked);
    CHECK(southwards.velocityZ == doctest::Approx(0.0));
    CHECK(southwards.velocityX == doctest::Approx(0.25 * 0.8));
}

TEST_CASE("a swimmer that hits a ceiling stops rising without counting as landed") {
    Loaded loaded;
    REQUIRE(loaded.state.setBlockState(8, 72, 8, stateOf("stone")));
    ServerEntity swimmer{1, basalt::wire::common::Uuid::random(), Identifier::vanilla("zombie"), 1, 8.5, 70.0, 8.5};
    swimmer.velocityY = 5.0;

    livingMovement::travelInWater(loaded.state, swimmer, 0.0, 0.0);

    CHECK(swimmer.velocityY == doctest::Approx(-0.08 / 16.0));
    CHECK(swimmer.y < 72.0);
    CHECK_FALSE(swimmer.onGround);
}

TEST_CASE("swimming steers along the yaw like walking does") {
    Loaded loaded;
    ServerEntity west{1, basalt::wire::common::Uuid::random(), Identifier::vanilla("zombie"), 1, 8.5, 70.0, 8.5};
    west.yaw = 90.0F;

    livingMovement::travelInWater(loaded.state, west, 1.0, 0.0);

    CHECK(west.x < 8.5);
    CHECK(west.z == doctest::Approx(8.5));
    CHECK(west.velocityX == doctest::Approx(-0.02 * 0.8));
}
