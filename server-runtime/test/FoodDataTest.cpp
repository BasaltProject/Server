#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/player/FoodData.hpp>

#include <doctest.h>

#include <cstdint>
#include <vector>

using basalt::server::runtime::player::Difficulty;
using basalt::server::runtime::player::FoodData;

namespace {

/// A tick with nothing to heal or hurt, for the cases that are only about the counters.
void quietTick(FoodData& food, Difficulty difficulty, float health = 20.0F, float maxHealth = 20.0F) {
    food.tick(difficulty, health, maxHealth, [](float) {}, [](float) {});
}

} // namespace

TEST_CASE("a fresh player is full and unspent") {
    const FoodData food;

    CHECK(food.foodLevel() == FoodData::kMaxFood);
    CHECK(food.saturationLevel() == doctest::Approx(FoodData::kDefaultSaturation));
    CHECK(food.exhaustionLevel() == doctest::Approx(0.0F));
}

TEST_CASE("exhaustion piles up to forty and no further") {
    FoodData food;

    for (std::int32_t turn = 0; turn < 100; turn++) {
        food.addExhaustion(1.0F);
    }

    CHECK(food.exhaustionLevel() == doctest::Approx(40.0F));
}

TEST_CASE("four points of exhaustion spend a point of saturation") {
    FoodData food;
    food.restore(20, 5.0F, 4.5F);

    quietTick(food, Difficulty::Normal);

    // parity: `FoodData.tick` takes four off and spends one saturation, leaving the rest behind.
    CHECK(food.exhaustionLevel() == doctest::Approx(0.5F));
    CHECK(food.saturationLevel() == doctest::Approx(4.0F));
    CHECK(food.foodLevel() == 20);
}

TEST_CASE("exactly four points of exhaustion are not enough to spend anything") {
    FoodData food;
    food.restore(20, 5.0F, 4.0F);

    quietTick(food, Difficulty::Normal);

    CHECK(food.exhaustionLevel() == doctest::Approx(4.0F));
    CHECK(food.saturationLevel() == doctest::Approx(5.0F));
}

TEST_CASE("with no saturation left the food goes instead, unless the world is peaceful") {
    FoodData food;
    food.restore(20, 0.0F, 4.5F);
    quietTick(food, Difficulty::Normal);
    CHECK(food.foodLevel() == 19);

    FoodData peaceful;
    peaceful.restore(20, 0.0F, 4.5F);
    quietTick(peaceful, Difficulty::Peaceful);
    CHECK(peaceful.foodLevel() == 20);
    CHECK(peaceful.exhaustionLevel() == doctest::Approx(0.5F));
}

TEST_CASE("a full and saturated player heals every ten ticks and pays for it") {
    FoodData food;
    food.restore(20, 5.0F, 0.0F);
    std::vector<float> healed;
    const auto heal = [&healed](float amount) { healed.push_back(amount); };
    const auto hurt = [](float) { FAIL("a fed player is not hurt"); };

    for (std::int32_t turn = 0; turn < 9; turn++) {
        food.tick(Difficulty::Normal, 10.0F, 20.0F, heal, hurt);
    }
    CHECK(healed.empty());

    food.tick(Difficulty::Normal, 10.0F, 20.0F, heal, hurt);

    // parity: the saturation spent is `min(saturation, 6)`, and the health it buys is that over six.
    REQUIRE(healed.size() == 1);
    CHECK(healed[0] == doctest::Approx(5.0F / 6.0F));
    CHECK(food.exhaustionLevel() == doctest::Approx(5.0F));
}

TEST_CASE("a player at full health does not regenerate however saturated") {
    FoodData food;
    food.restore(20, 5.0F, 0.0F);

    for (std::int32_t turn = 0; turn < 200; turn++) {
        food.tick(Difficulty::Normal, 20.0F, 20.0F, [](float) { FAIL("nothing to heal"); }, [](float) {});
    }

    CHECK(food.saturationLevel() == doctest::Approx(5.0F));
    CHECK(food.exhaustionLevel() == doctest::Approx(0.0F));
}

TEST_CASE("eighteen food regenerates a heart every eighty ticks") {
    FoodData food;
    food.restore(18, 0.0F, 0.0F);
    std::int32_t heals = 0;

    for (std::int32_t turn = 0; turn < 80; turn++) {
        food.tick(
            Difficulty::Normal, 10.0F, 20.0F,
            [&heals](float amount) {
                CHECK(amount == doctest::Approx(1.0F));
                heals++;
            },
            [](float) { FAIL("a fed player is not hurt"); });
    }

    CHECK(heals == 1);
    CHECK(food.exhaustionLevel() == doctest::Approx(6.0F));
}

TEST_CASE("seventeen food is one short of regenerating at all") {
    FoodData food;
    food.restore(17, 0.0F, 0.0F);

    for (std::int32_t turn = 0; turn < 400; turn++) {
        food.tick(Difficulty::Normal, 10.0F, 20.0F, [](float) { FAIL("seventeen does not regenerate"); }, [](float) {});
    }

    CHECK(food.exhaustionLevel() == doctest::Approx(0.0F));
}

TEST_CASE("an empty stomach starves every eighty ticks") {
    FoodData food;
    food.restore(0, 0.0F, 0.0F);
    std::int32_t hits = 0;

    for (std::int32_t turn = 0; turn < 160; turn++) {
        food.tick(
            Difficulty::Normal, 20.0F, 20.0F, [](float) { FAIL("nothing heals a starving player"); },
            [&hits](float amount) {
                CHECK(amount == doctest::Approx(1.0F));
                hits++;
            });
    }

    CHECK(hits == 2);
}

TEST_CASE("how far starvation goes is what the difficulty says") {
    // parity: `FoodData.tick`, which stops at five hearts anywhere but on normal and hard, at half
    // a heart on normal, and never stops on hard. Peaceful only gets this far in a test: there the
    // food never runs out in the first place.
    const auto starves = [](Difficulty difficulty, float health) {
        FoodData food;
        food.restore(0, 0.0F, 0.0F);
        bool hurt = false;
        for (std::int32_t turn = 0; turn < 80; turn++) {
            food.tick(difficulty, health, 20.0F, [](float) {}, [&hurt](float) { hurt = true; });
        }
        return hurt;
    };

    CHECK(starves(Difficulty::Easy, 10.5F));
    CHECK_FALSE(starves(Difficulty::Easy, 10.0F));
    CHECK(starves(Difficulty::Normal, 1.5F));
    CHECK_FALSE(starves(Difficulty::Normal, 1.0F));
    CHECK(starves(Difficulty::Hard, 0.5F));
    CHECK_FALSE(starves(Difficulty::Peaceful, 10.0F));
    CHECK(starves(Difficulty::Peaceful, 10.5F));
}

TEST_CASE("eating fills the stomach and never leaves more saturation than food") {
    FoodData food;
    food.set(4, 0.0F);

    food.eat(2, 20.0F);

    CHECK(food.foodLevel() == 6);
    CHECK(food.saturationLevel() == doctest::Approx(6.0F));

    // A meal bigger than the stomach fills it and stops there.
    food.eat(40, 1.0F);
    CHECK(food.foodLevel() == FoodData::kMaxFood);
    CHECK(food.saturationLevel() == doctest::Approx(7.0F));
}

TEST_CASE("restoring a saved player puts the timer back to the start") {
    FoodData food;
    food.restore(18, 0.0F, 0.0F);
    for (std::int32_t turn = 0; turn < 79; turn++) {
        quietTick(food, Difficulty::Normal, 10.0F);
    }

    food.restore(18, 0.0F, 0.0F);
    std::int32_t heals = 0;
    food.tick(Difficulty::Normal, 10.0F, 20.0F, [&heals](float) { heals++; }, [](float) {});

    CHECK(heals == 0);
}

TEST_CASE("resetting is a fresh player again") {
    FoodData food;
    food.restore(3, 1.0F, 12.0F);

    food.reset();

    CHECK(food.foodLevel() == FoodData::kMaxFood);
    CHECK(food.saturationLevel() == doctest::Approx(FoodData::kDefaultSaturation));
    CHECK(food.exhaustionLevel() == doctest::Approx(0.0F));
}

TEST_CASE("walking is free and sprinting costs a tenth of a point a block") {
    CHECK(FoodData::movementExhaustion(1.0, 0.0, 0.0, false, true, false) == doctest::Approx(0.0F));
    CHECK(FoodData::movementExhaustion(1.0, 0.0, 0.0, true, true, false) == doctest::Approx(0.1F));

    // parity: the distance is counted in whole centimetres, so half a block is half the cost.
    CHECK(FoodData::movementExhaustion(0.5, 0.0, 0.0, true, true, false) == doctest::Approx(0.05F));

    // A sprint through the air costs nothing: vanilla charges for it on the ground.
    CHECK(FoodData::movementExhaustion(1.0, 0.0, 0.0, true, false, false) == doctest::Approx(0.0F));
}

TEST_CASE("swimming counts the height it covers and costs a hundredth a block") {
    CHECK(FoodData::movementExhaustion(3.0, 4.0, 0.0, false, false, true) == doctest::Approx(0.05F));

    // Out of the water the same movement is free, because the height is not counted at all.
    CHECK(FoodData::movementExhaustion(0.0, 4.0, 0.0, false, true, false) == doctest::Approx(0.0F));
}

TEST_CASE("a movement of no distance at all costs nothing") {
    CHECK(FoodData::movementExhaustion(0.0, 0.0, 0.0, true, true, true) == doctest::Approx(0.0F));

    // Under half a centimetre rounds to nothing, and nothing is not charged for.
    CHECK(FoodData::movementExhaustion(0.004, 0.0, 0.0, true, true, false) == doctest::Approx(0.0F));
    CHECK(FoodData::movementExhaustion(0.006, 0.0, 0.0, true, true, false) == doctest::Approx(0.001F));
}

TEST_CASE("a jump costs four times as much while sprinting") {
    CHECK(FoodData::jumpExhaustion(false) == doctest::Approx(0.05F));
    CHECK(FoodData::jumpExhaustion(true) == doctest::Approx(0.2F));
}

TEST_CASE("with natural regeneration off a full belly heals nothing") {
    FoodData food;
    food.set(20, 5.0F);
    for (std::int32_t tick = 0; tick < 200; ++tick) {
        food.tick(Difficulty::Normal, 10.0F, 20.0F, [](float) { FAIL("the rule is off"); }, [](float) {}, false);
    }
    CHECK(food.foodLevel() == 20);
}
