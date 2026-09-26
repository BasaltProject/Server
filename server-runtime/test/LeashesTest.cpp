#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/entity/Leashes.hpp>

#include <doctest.h>

namespace leashes = basalt::server::runtime::entity::leashes;

TEST_CASE("a lead within its slack does not pull") {
    const leashes::Body mob{.width = 1.0, .height = 2.0};
    const leashes::Body holder{.z = 5.0, .height = 2.0};
    CHECK_FALSE(leashes::elastic(mob, holder, leashes::kElasticDistance).has_value());
}

TEST_CASE("a taut lead pulls the mob's front towards the holder, damped when it already moves that way") {
    // The front of a 1 wide mob sits 0.5 out, so 9.5 of rope, 3.5 past the slack.
    const leashes::Body mob{.width = 1.0, .height = 2.0};
    const leashes::Body holder{.z = 10.0, .height = 2.0};
    const auto pull = leashes::elastic(mob, holder, leashes::kElasticDistance);
    REQUIRE(pull.has_value());
    CHECK(pull->push[0] == 0.0);
    CHECK(pull->push[1] == 0.0);
    CHECK(pull->push[2] == doctest::Approx(3.5 * 0.3 * 0.8).epsilon(1e-6));
    CHECK(pull->angularMomentum == 0.0);
}

TEST_CASE("a lead pulling sideways on the mob's front turns it") {
    const leashes::Body mob{.width = 1.0, .height = 2.0};
    const leashes::Body holder{.x = 10.0, .height = 2.0};
    const auto pull = leashes::elastic(mob, holder, leashes::kElasticDistance);
    REQUIRE(pull.has_value());
    CHECK(pull->angularMomentum > 0.0);
}

TEST_CASE("the leash distance runs between box centres") {
    const leashes::Body mob{.height = 2.0};
    const leashes::Body holder{.x = 3.0, .y = 3.0, .height = 4.0};
    CHECK(leashes::distance(mob, holder) == doctest::Approx(5.0));
}
