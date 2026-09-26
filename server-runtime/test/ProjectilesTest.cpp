#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace basalt::server::runtime::entity;
using basalt::server::runtime::world::WorldState;
using basalt::server::world::Chunk;
namespace registry = basalt::server::registry;
namespace rng = basalt::server::random;

namespace {

[[nodiscard]] basalt::wire::common::Identifier item(const std::string& path) {
    return basalt::wire::common::Identifier::vanilla(path);
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

struct Loaded {
    WorldState state = emptyWorld();

    Loaded() { state.getOrGenerateChunk(0, 0); }
};

/// A projectile standing still at the middle of the chunk, ready to be aimed.
[[nodiscard]] ServerEntity flying(const projectiles::Kind& kind, double x = 8.5, double y = 64.5, double z = 8.5) {
    return ServerEntity{1, basalt::wire::common::Uuid::random(), kind.type, 0, x, y, z};
}

} // namespace

TEST_CASE("each kind carries what vanilla gives it") {
    CHECK(projectiles::arrow().type == item("arrow"));
    CHECK(projectiles::arrow().gravity == doctest::Approx(0.05));
    CHECK(projectiles::arrow().drag == doctest::Approx(0.99));
    CHECK(projectiles::arrow().sticks);

    // parity: a fireball does not fall at all, and keeps 0.95 of its speed a tick.
    CHECK(projectiles::fireball().gravity == doctest::Approx(0.0));
    CHECK(projectiles::fireball().drag == doctest::Approx(0.95));
    CHECK_FALSE(projectiles::fireball().sticks);

    CHECK(projectiles::snowball().gravity == doctest::Approx(0.03));
    CHECK_FALSE(projectiles::snowball().sticks);
    CHECK(projectiles::all().size() == 13);
}

TEST_CASE("the whole table of kinds, since a wrong number here is a projectile that flies wrong") {
    struct Row {
        std::string type;
        double gravity;
        double drag;
        bool sticks;
        double acceleration = 0.0;
    };

    // parity: `AbstractArrow`, `AbstractHurtingProjectile` and `ThrowableProjectile`'s own defaults.
    const std::vector<Row> table{{"arrow", 0.05, 0.99, true},
                                 {"spectral_arrow", 0.05, 0.99, true},
                                 {"snowball", 0.03, 0.99, false},
                                 {"egg", 0.03, 0.99, false},
                                 {"ender_pearl", 0.03, 0.99, false},
                                 {"experience_bottle", 0.07, 0.99, false},
                                 {"potion", 0.05, 0.99, false},
                                 {"fireball", 0.0, 0.95, false, 0.1},
                                 {"small_fireball", 0.0, 0.95, false, 0.1},
                                 // parity: a wind charge keeps all of its speed and adds nothing back.
                                 {"wind_charge", 0.0, 1.0, false},
                                 {"breeze_wind_charge", 0.0, 1.0, false},
                                 {"dragon_fireball", 0.0, 0.95, false, 0.1},
                                 {"wither_skull", 0.0, 0.95, false, 0.1}};

    CHECK(table.size() == projectiles::all().size());
    for (const Row& row : table) {
        const projectiles::Kind* kind = projectiles::kindOf(item(row.type));
        REQUIRE(kind != nullptr);
        CHECK(kind->gravity == doctest::Approx(row.gravity));
        CHECK(kind->drag == doctest::Approx(row.drag));
        CHECK(kind->sticks == row.sticks);
        CHECK(kind->acceleration == doctest::Approx(row.acceleration));
    }
}

TEST_CASE("a dispenser shoots what vanilla registered a behaviour for") {
    REQUIRE(projectiles::dispensed(item("arrow")).has_value());
    CHECK(projectiles::dispensed(item("arrow"))->kind->type == item("arrow"));
    // A tipped arrow flies as a plain one, and every egg as an egg.
    CHECK(projectiles::dispensed(item("tipped_arrow"))->kind->type == item("arrow"));
    CHECK(projectiles::dispensed(item("blue_egg"))->kind->type == item("egg"));
    CHECK(projectiles::dispensed(item("brown_egg"))->kind->type == item("egg"));
    CHECK(projectiles::dispensed(item("spectral_arrow"))->kind->type == item("spectral_arrow"));
    // A splash potion flies as the potion entity, a quarter harder and half as wide as the rest.
    CHECK(projectiles::dispensed(item("splash_potion"))->kind->type == item("potion"));
    CHECK(projectiles::dispensed(item("splash_potion"))->power == doctest::Approx(1.375));
    CHECK(projectiles::dispensed(item("splash_potion"))->uncertainty == doctest::Approx(3.0));
    // A fire charge flies as a small fireball, from a whole block out and with no lift.
    CHECK(projectiles::dispensed(item("fire_charge"))->kind->type == item("small_fireball"));
    CHECK(projectiles::dispensed(item("fire_charge"))->reach == doctest::Approx(1.0));
    CHECK(projectiles::dispensed(item("fire_charge"))->lift == doctest::Approx(0.0));

    // Anything else is thrown as an item instead.
    CHECK(!projectiles::dispensed(item("stone")).has_value());
    CHECK(!projectiles::dispensed(item("firework_rocket")).has_value());
    CHECK(!projectiles::dispensed(item("lingering_potion")).has_value());
}

TEST_CASE("a flying entity says what it weighs by its own type") {
    REQUIRE(projectiles::kindOf(item("snowball")) != nullptr);
    CHECK(projectiles::kindOf(item("snowball"))->gravity == doctest::Approx(0.03));
    CHECK(projectiles::kindOf(item("small_fireball"))->drag == doctest::Approx(0.95));
    CHECK(projectiles::kindOf(item("cow")) == nullptr);
}

TEST_CASE("shooting with no spread aims exactly along the direction") {
    ServerEntity entity = flying(projectiles::arrow());
    rng::RandomSource random = rng::RandomSource::legacy(1);

    projectiles::shoot(entity, 0.0, 0.0, 2.0, 1.5F, 0.0F, random);

    // The direction is normalised first, then scaled by the power.
    CHECK(entity.velocityX == doctest::Approx(0.0));
    CHECK(entity.velocityY == doctest::Approx(0.0));
    CHECK(entity.velocityZ == doctest::Approx(1.5));
    // And the rotation follows the movement: straight south is a yaw of zero.
    CHECK(entity.yaw == doctest::Approx(0.0));
    CHECK(entity.pitch == doctest::Approx(0.0));
}

TEST_CASE("the rotation follows the movement whichever way it goes") {
    rng::RandomSource random = rng::RandomSource::legacy(1);
    const auto aimedAt = [&](double dirX, double dirY, double dirZ) {
        ServerEntity entity = flying(projectiles::arrow());
        projectiles::shoot(entity, dirX, dirY, dirZ, 1.0F, 0.0F, random);
        return std::pair{entity.yaw, entity.pitch};
    };

    CHECK(aimedAt(1.0, 0.0, 0.0).first == doctest::Approx(90.0));
    CHECK(aimedAt(-1.0, 0.0, 0.0).first == doctest::Approx(-90.0));
    CHECK(aimedAt(0.0, 0.0, -1.0).first == doctest::Approx(180.0));
    CHECK(aimedAt(0.0, 1.0, 0.0).second == doctest::Approx(90.0));
    CHECK(aimedAt(0.0, -1.0, 0.0).second == doctest::Approx(-90.0));
}

TEST_CASE("uncertainty spreads the shot and a direction of nothing is not a shot") {
    ServerEntity spread = flying(projectiles::arrow());
    rng::RandomSource random = rng::RandomSource::legacy(3);
    projectiles::shoot(spread, 0.0, 0.0, 1.0, 1.0F, 6.0F, random);

    // parity: one degree of uncertainty is worth 0.0172275 of spread, six of them a tenth.
    CHECK(spread.velocityZ != doctest::Approx(1.0));
    CHECK(std::abs(spread.velocityX) < 0.2);
    CHECK(std::abs(spread.velocityZ - 1.0) < 0.2);

    ServerEntity still = flying(projectiles::arrow());
    projectiles::shoot(still, 0.0, 0.0, 0.0, 1.0F, 0.0F, random);
    CHECK(still.velocityX == doctest::Approx(0.0));
    CHECK(still.velocityY == doctest::Approx(0.0));
    CHECK(still.velocityZ == doctest::Approx(0.0));
}

// parity: `AbstractArrow.tick` moves with the speed it already had and applies the drag and the
// gravity **after**, which is the opposite of a thrown projectile.
TEST_CASE("an arrow flies the whole step first and slows down after") {
    Loaded loaded;
    ServerEntity entity = flying(projectiles::arrow());
    entity.velocityZ = 1.0;

    CHECK_FALSE(projectiles::tick(loaded.state, entity, projectiles::arrow()).has_value());

    CHECK(entity.z == doctest::Approx(9.5));
    CHECK(entity.velocityZ == doctest::Approx(0.99));
    CHECK(entity.velocityY == doctest::Approx(-0.05));
    CHECK(entity.age == 1);
    CHECK(entity.y == doctest::Approx(64.5));
}

// parity: `ThrowableProjectile.tick` is `applyGravity`, `applyInertia` and only then the move, so a
// snowball is already falling on the tick it leaves the hand.
TEST_CASE("a thrown projectile falls and slows down before it moves") {
    Loaded loaded;
    ServerEntity entity = flying(projectiles::snowball());
    entity.velocityZ = 1.0;

    CHECK_FALSE(projectiles::tick(loaded.state, entity, projectiles::snowball()).has_value());

    CHECK(entity.velocityY == doctest::Approx(-0.03 * 0.99));
    CHECK(entity.velocityZ == doctest::Approx(0.99));
    CHECK(entity.z == doctest::Approx(8.5 + 0.99));
    CHECK(entity.y == doctest::Approx(64.5 - (0.03 * 0.99)));
}

TEST_CASE("a fireball keeps going in a straight line") {
    Loaded loaded;
    ServerEntity entity = flying(projectiles::fireball());
    entity.velocityZ = 1.0;

    for (std::int32_t turn = 0; turn < 5; ++turn) {
        CHECK_FALSE(projectiles::tick(loaded.state, entity, projectiles::fireball()).has_value());
    }

    CHECK(entity.velocityY == doctest::Approx(0.0));
    CHECK(entity.y == doctest::Approx(64.5));
}

TEST_CASE("an arrow sticks in what it hits and a snowball does not") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(11, 64, 8, registry::blocks::named("stone")->stateId));

    ServerEntity stuck = flying(projectiles::arrow(), 8.5, 64.5, 8.5);
    stuck.velocityX = 3.0;
    const std::optional<BlockHit> arrowHit = projectiles::tick(world, stuck, projectiles::arrow());
    REQUIRE(arrowHit.has_value());
    CHECK(arrowHit->blockX == 11);
    // parity: `AbstractArrow.onHitBlock` backs it off a twentieth of a block along the axis it was
    // moving on, so it ends up in the square in front of the stone and not on its face. Measured in
    // `flechavibra`, where the jar's arrow rested one square short of where ours did.
    CHECK(stuck.x == doctest::Approx(10.95));
    CHECK(stuck.velocityX == doctest::Approx(0.0));
    CHECK(stuck.onGround);
    // Once stuck it stays where it is, whatever else happens.
    CHECK_FALSE(projectiles::tick(world, stuck, projectiles::arrow()).has_value());
    CHECK(stuck.x == doctest::Approx(10.95));

    ServerEntity gone = flying(projectiles::snowball(), 8.5, 64.5, 8.5);
    gone.velocityX = 3.0;
    const std::optional<BlockHit> snowballHit = projectiles::tick(world, gone, projectiles::snowball());
    REQUIRE(snowballHit.has_value());
    // It is left where it hit and the caller removes it: nothing is zeroed.
    CHECK_FALSE(gone.onGround);
    CHECK(gone.velocityX != doctest::Approx(0.0));
}

TEST_CASE("a projectile put in the world comes out aimed") {
    Loaded loaded;
    rng::RandomSource random = rng::RandomSource::legacy(5);

    const ServerEntity& shot =
        projectiles::spawn(loaded.state, projectiles::arrow(), 42, 3, 8.5, 64.5, 8.5, 0.0, 0.0, 1.0,
                           projectiles::kDispenserPower, projectiles::kDispenserUncertainty, random);

    CHECK(shot.entityId() == 42);
    CHECK(shot.type() == item("arrow"));
    CHECK(shot.protocolType() == 3);
    CHECK(shot.x == doctest::Approx(8.5));
    CHECK(shot.velocityZ > 0.9);
    CHECK(loaded.state.entities().size() == 1);
}

TEST_CASE("a line clipped against a box says how far along it went in") {
    // Straight at the middle of a box a block away.
    CHECK(projectiles::clipBox(0.0, 0.5, 0.5, 4.0, 0.5, 0.5, 1.0, 0.0, 0.0, 2.0, 1.0, 1.0) == doctest::Approx(0.25));
    // A line that starts inside it is already in at zero.
    CHECK(projectiles::clipBox(1.5, 0.5, 0.5, 4.0, 0.5, 0.5, 1.0, 0.0, 0.0, 2.0, 1.0, 1.0) == doctest::Approx(0.0));
    // One that stops short never gets there.
    CHECK_FALSE(projectiles::clipBox(0.0, 0.5, 0.5, 0.5, 0.5, 0.5, 1.0, 0.0, 0.0, 2.0, 1.0, 1.0).has_value());
    // One that passes beside it misses.
    CHECK_FALSE(projectiles::clipBox(0.0, 5.0, 0.5, 4.0, 5.0, 0.5, 1.0, 0.0, 0.0, 2.0, 1.0, 1.0).has_value());
    // And one that runs along an axis outside the box misses on that axis alone.
    CHECK_FALSE(projectiles::clipBox(0.0, 5.0, 0.5, 4.0, 5.0, 0.5, 1.0, 0.0, 0.0, 2.0, 1.0, 1.0).has_value());
}
