#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/player/ExperienceData.hpp>

#include <doctest.h>

using basalt::server::runtime::player::ExperienceData;

TEST_CASE("what the next level costs follows vanilla's three curves") {
    ExperienceData experience;

    // parity: `Player.getXpNeededForNextLevel`, whose three pieces meet at fifteen and thirty.
    experience.setLevels(0);
    CHECK(experience.xpNeededForNextLevel() == 7);
    experience.setLevels(14);
    CHECK(experience.xpNeededForNextLevel() == 35);
    experience.setLevels(15);
    CHECK(experience.xpNeededForNextLevel() == 37);
    experience.setLevels(29);
    CHECK(experience.xpNeededForNextLevel() == 107);
    experience.setLevels(30);
    CHECK(experience.xpNeededForNextLevel() == 112);
    experience.setLevels(40);
    CHECK(experience.xpNeededForNextLevel() == 202);
}

TEST_CASE("seven points is exactly one level from nothing") {
    ExperienceData experience;

    experience.givePoints(7);

    CHECK(experience.level() == 1);
    CHECK(experience.progress() == doctest::Approx(0.0F));
    CHECK(experience.total() == 7);
}

TEST_CASE("points short of a level leave the bar part of the way along") {
    ExperienceData experience;

    experience.givePoints(3);

    CHECK(experience.level() == 0);
    CHECK(experience.progress() == doctest::Approx(3.0F / 7.0F));
    CHECK(experience.total() == 3);
}

TEST_CASE("a heap of points climbs level after level and keeps the leftovers") {
    ExperienceData experience;

    // parity: the overflow loop renormalises twice, so where a big grant lands is not simply the
    // sum divided by anything.
    experience.givePoints(100);

    // Seven levels cost 7+9+11+13+15+17+19 = 91, so nine of the hundred are left over on a bar
    // twenty-one points wide.
    CHECK(experience.level() == 7);
    CHECK(experience.progress() == doctest::Approx(9.0F / 21.0F));
    CHECK(experience.total() == 100);
}

TEST_CASE("taking points back walks the levels down again") {
    ExperienceData experience;
    experience.givePoints(100);

    experience.givePoints(-50);

    CHECK(experience.level() == 4);
    CHECK(experience.total() == 50);
    CHECK(experience.progress() >= 0.0F);
    CHECK(experience.progress() < 1.0F);
}

TEST_CASE("taking back more than there is empties the bar rather than going under") {
    ExperienceData experience;
    experience.givePoints(30);

    experience.givePoints(-1000);

    CHECK(experience.level() == 0);
    CHECK(experience.progress() == doctest::Approx(0.0F));
    CHECK(experience.total() == 0);
}

TEST_CASE("a level below zero takes the whole bar with it") {
    ExperienceData experience;
    experience.givePoints(50);

    experience.giveLevels(-100);

    CHECK(experience.level() == 0);
    CHECK(experience.progress() == doctest::Approx(0.0F));
    CHECK(experience.total() == 0);
}

TEST_CASE("levels handed over on their own leave the bar and the total alone") {
    ExperienceData experience;
    experience.givePoints(10);
    const float bar = experience.progress();

    experience.giveLevels(3);

    CHECK(experience.level() == 4);
    CHECK(experience.progress() == doctest::Approx(bar));
    CHECK(experience.total() == 10);
}

TEST_CASE("the running total never overflows however much is given") {
    ExperienceData experience;
    experience.restore(0, 0.0F, 2000000000);

    experience.givePoints(2000000000);

    CHECK(experience.total() == 2147483647);
}

TEST_CASE("setting the bar to a whole level's worth stops one point short") {
    ExperienceData experience;
    experience.setLevels(0);

    experience.setPoints(7);

    // parity: `ServerPlayer.setExperiencePoints` clamps to `(needed - 1) / needed`, so a full bar
    // never rolls over on its own.
    CHECK(experience.level() == 0);
    CHECK(experience.progress() == doctest::Approx(6.0F / 7.0F));
}

TEST_CASE("setting the bar below nothing leaves it empty") {
    ExperienceData experience;
    experience.setLevels(5);

    experience.setPoints(-4);

    CHECK(experience.progress() == doctest::Approx(0.0F));
    CHECK(experience.level() == 5);
}

TEST_CASE("restoring puts back the level, the bar and the total together") {
    ExperienceData experience;

    experience.restore(12, 0.25F, 400);

    CHECK(experience.level() == 12);
    CHECK(experience.progress() == doctest::Approx(0.25F));
    CHECK(experience.total() == 400);

    experience.reset();
    CHECK(experience.level() == 0);
    CHECK(experience.progress() == doctest::Approx(0.0F));
    CHECK(experience.total() == 0);
}
