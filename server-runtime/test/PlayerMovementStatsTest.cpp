#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/player/PlayerMovementStats.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>

#include <doctest.h>

using basalt::server::runtime::player::playerMovement::Counted;
using basalt::server::runtime::player::playerMovement::Facts;
using basalt::server::runtime::player::playerMovement::movementStat;
namespace stats = basalt::server::storage::stats;

TEST_CASE("one step counts towards exactly the counter vanilla picks for it") {
    // parity: the branches of `checkMovementStatistics`, in the order it tries them.
    const Facts walking{.onGround = true};
    const std::optional<Counted> walked = movementStat(1.0, 0.0, 0.0, walking);
    REQUIRE(walked.has_value());
    CHECK(walked->key == stats::walkOneCm());
    CHECK(walked->centimetres == 100);

    const Facts sprinting{.onGround = true, .sprinting = true};
    CHECK(movementStat(1.0, 0.0, 0.0, sprinting)->key == stats::sprintOneCm());

    const Facts crouching{.onGround = true, .crouching = true};
    CHECK(movementStat(1.0, 0.0, 0.0, crouching)->key == stats::crouchOneCm());

    // Swimming wins over both water branches, and counts the vertical part too.
    const Facts swimming{.swimming = true, .eyeInWater = true, .inWater = true};
    const std::optional<Counted> swam = movementStat(0.0, 3.0, 4.0, swimming);
    REQUIRE(swam.has_value());
    CHECK(swam->key == stats::swimOneCm());
    CHECK(swam->centimetres == 500);

    // Eyes under water beats standing in it, and standing in it counts only the horizontal part.
    CHECK(movementStat(1.0, 1.0, 0.0, Facts{.eyeInWater = true, .inWater = true})->key == stats::walkUnderWaterOneCm());
    const std::optional<Counted> wading = movementStat(1.0, 1.0, 0.0, Facts{.inWater = true});
    REQUIRE(wading.has_value());
    CHECK(wading->key == stats::walkOnWaterOneCm());
    CHECK(wading->centimetres == 100);
}

TEST_CASE("climbing counts what was gained, and the air counts nothing under a quarter block") {
    const Facts climbing{.climbing = true};
    const std::optional<Counted> up = movementStat(0.0, 0.5, 0.0, climbing);
    REQUIRE(up.has_value());
    CHECK(up->key == stats::climbOneCm());
    CHECK(up->centimetres == 50);
    // parity: going down a ladder counts for nothing at all, not even as walking.
    CHECK_FALSE(movementStat(0.0, -0.5, 0.0, climbing).has_value());

    // parity: the last branch needs more than twenty-five centimetres before it counts.
    CHECK_FALSE(movementStat(0.25, 0.0, 0.0, Facts{}).has_value());
    CHECK(movementStat(0.26, 0.0, 0.0, Facts{})->key == stats::flyOneCm());

    // A passenger and a player standing still both count nowhere.
    CHECK_FALSE(movementStat(1.0, 0.0, 0.0, Facts{.riding = true, .onGround = true}).has_value());
    CHECK_FALSE(movementStat(0.0, 0.0, 0.0, Facts{.onGround = true}).has_value());
}
