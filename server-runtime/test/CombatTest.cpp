#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/player/Combat.hpp>

#include <doctest.h>

namespace combat = basalt::server::runtime::player::combat;

TEST_CASE("armour is worth less against a big hit than a small one") {
    // parity: `CombatRules.getDamageAfterAbsorb`, whose whole point is that a full set of armour
    // stops four fifths of a light hit and much less of a heavy one.
    CHECK(combat::afterArmour(1.0F, 20.0F, 0.0F) == doctest::Approx(1.0F * (1.0F - (19.5F / 25.0F))));
    CHECK(combat::afterArmour(10.0F, 20.0F, 0.0F) == doctest::Approx(4.0F));

    // A hit big enough drives the effective armour down to its floor of a fifth.
    CHECK(combat::afterArmour(100.0F, 20.0F, 0.0F) == doctest::Approx(84.0F));
}

TEST_CASE("toughness keeps armour effective against a heavy hit") {
    CHECK(combat::afterArmour(10.0F, 20.0F, 8.0F) == doctest::Approx(3.0F));
    CHECK(combat::afterArmour(10.0F, 20.0F, 8.0F) < combat::afterArmour(10.0F, 20.0F, 0.0F));
}

TEST_CASE("armour never counts for more than twenty") {
    CHECK(combat::afterArmour(1.0F, 40.0F, 0.0F) == doctest::Approx(1.0F * (1.0F - (20.0F / 25.0F))));
}

TEST_CASE("no armour at all leaves the hit as it was") {
    CHECK(combat::afterArmour(7.0F, 0.0F, 0.0F) == doctest::Approx(7.0F));
}

TEST_CASE("protection takes its own fraction and caps at twenty") {
    CHECK(combat::afterProtection(10.0F, 0.0F) == doctest::Approx(10.0F));
    CHECK(combat::afterProtection(10.0F, 10.0F) == doctest::Approx(6.0F));
    CHECK(combat::afterProtection(10.0F, 20.0F) == doctest::Approx(2.0F));
    CHECK(combat::afterProtection(10.0F, 100.0F) == doctest::Approx(2.0F));
    CHECK(combat::afterProtection(10.0F, -5.0F) == doctest::Approx(10.0F));
}

TEST_CASE("a fall of three blocks is free and four hurts") {
    // parity: `calculateFallPower`'s epsilon, without which a fall of exactly four would not hurt.
    CHECK(combat::fallDamage(3.0) == 0);
    CHECK(combat::fallDamage(3.5) == 0);
    CHECK(combat::fallDamage(4.0) == 1);
    CHECK(combat::fallDamage(10.0) == 7);
    CHECK(combat::fallDamage(0.0) == 0);
}

TEST_CASE("a longer safe fall distance moves where the damage starts") {
    CHECK(combat::fallDamage(6.0, 6.0) == 0);
    CHECK(combat::fallDamage(7.0, 6.0) == 1);
}

TEST_CASE("a swing recharges over the delay its speed asks for") {
    // parity: a bare hand swings four times a second, so its delay is five ticks.
    CHECK(combat::attackStrengthScale(0, 4.0) == doctest::Approx(0.1F));
    CHECK(combat::attackStrengthScale(4, 4.0) == doctest::Approx(0.9F));
    CHECK(combat::attackStrengthScale(5, 4.0) == doctest::Approx(1.0F));
    CHECK(combat::attackStrengthScale(500, 4.0) == doctest::Approx(1.0F));

    // A weapon with no speed at all is always ready rather than never.
    CHECK(combat::attackStrengthScale(0, 0.0) == doctest::Approx(1.0F));
}

TEST_CASE("an uncharged swing still lands a fifth of the damage") {
    CHECK(combat::chargeScale(0.0F) == doctest::Approx(0.2F));
    CHECK(combat::chargeScale(0.5F) == doctest::Approx(0.4F));
    CHECK(combat::chargeScale(1.0F) == doctest::Approx(1.0F));
}

TEST_CASE("the difficulty decides how much of a mob's hit lands") {
    CHECK(combat::afterDifficulty(10.0F, "peaceful") == doctest::Approx(0.0F));
    CHECK(combat::afterDifficulty(10.0F, "easy") == doctest::Approx(6.0F));
    CHECK(combat::afterDifficulty(10.0F, "normal") == doctest::Approx(10.0F));
    CHECK(combat::afterDifficulty(10.0F, "hard") == doctest::Approx(15.0F));

    // parity: easy adds one back but never lets the hit grow.
    CHECK(combat::afterDifficulty(1.0F, "easy") == doctest::Approx(1.0F));

    // A word nobody knows is normal, which is what an unset difficulty comes out as.
    CHECK(combat::afterDifficulty(10.0F, "whatever") == doctest::Approx(10.0F));
}

TEST_CASE("knockback halves the speed, pushes against the blow and lifts on the ground") {
    const std::array<double, 3> pushed = combat::knockback(0.4, 1.0, 0.0, 0.0, 0.0, 0.0, true);

    CHECK(pushed[0] == doctest::Approx(-0.4));
    CHECK(pushed[1] == doctest::Approx(0.4));
    CHECK(pushed[2] == doctest::Approx(0.0));
}

TEST_CASE("the lift is capped, and there is none in the air") {
    const std::array<double, 3> onGround = combat::knockback(2.0, 1.0, 0.0, 0.0, 0.0, 0.0, true);
    CHECK(onGround[1] == doctest::Approx(0.4));

    const std::array<double, 3> midAir = combat::knockback(2.0, 1.0, 0.0, 0.0, -0.5, 0.0, false);
    CHECK(midAir[1] == doctest::Approx(-0.5));
}

TEST_CASE("the push is taken off half of what the speed was") {
    const std::array<double, 3> pushed = combat::knockback(0.4, 0.0, 1.0, 1.0, 0.0, 1.0, false);

    CHECK(pushed[0] == doctest::Approx(0.5));
    CHECK(pushed[2] == doctest::Approx(0.5 - 0.4));
}

TEST_CASE("resistance to knockback takes it away, and a blow from nowhere never lands") {
    const std::array<double, 3> resisted = combat::knockback(0.4, 1.0, 0.0, 3.0, 2.0, 1.0, true, 1.0);
    CHECK(resisted[0] == doctest::Approx(3.0));
    CHECK(resisted[1] == doctest::Approx(2.0));
    CHECK(resisted[2] == doctest::Approx(1.0));

    // Half the resistance is half the push, and the speed is still halved.
    const std::array<double, 3> halved = combat::knockback(0.4, 1.0, 0.0, 0.0, 0.0, 0.0, false, 0.5);
    CHECK(halved[0] == doctest::Approx(-0.2));

    // A blow with no direction is not turned into one pointing anywhere.
    const std::array<double, 3> nowhere = combat::knockback(0.4, 0.0, 0.0, 1.0, 1.0, 1.0, true);
    CHECK(nowhere[0] == doctest::Approx(1.0));
    CHECK(nowhere[1] == doctest::Approx(1.0));
}

TEST_CASE("resistance counts in fifths and stops at nothing") {
    CHECK(combat::afterResistance(10.0F, 0) == doctest::Approx(8.0F));
    CHECK(combat::afterResistance(10.0F, 1) == doctest::Approx(6.0F));
    CHECK(combat::afterResistance(10.0F, 3) == doctest::Approx(2.0F));

    // parity: level four and above takes everything rather than healing the player.
    CHECK(combat::afterResistance(10.0F, 4) == doctest::Approx(0.0F));
    CHECK(combat::afterResistance(10.0F, 40) == doctest::Approx(0.0F));
}
