#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/entity/MobConversions.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <doctest.h>

#include <optional>
#include <string>

using basalt::server::runtime::entity::ServerEntity;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
using basalt::wire::nbt::NbtCompound;
namespace conversions = basalt::server::runtime::entity::mobConversions;

namespace {

[[nodiscard]] ServerEntity mobOf(const std::string& path) {
    return ServerEntity{1, Uuid{1, 1}, Identifier::vanilla(path), 0, 0.0, 64.0, 0.0};
}

/// The tick a conversion comes due, or nothing within the limit.
[[nodiscard]] std::optional<std::int32_t> ticksUntil(ServerEntity& mob, const conversions::Around& around,
                                                     std::int32_t limit) {
    for (std::int32_t tick = 1; tick <= limit; ++tick) {
        if (conversions::tick(mob, around).has_value()) {
            return tick;
        }
    }
    return std::nullopt;
}

} // namespace

TEST_CASE("a zombie turns into a drowned after thirty seconds under and fifteen of shaking") {
    ServerEntity zombie = mobOf("zombie");
    const conversions::Around under{.eyeInWater = true};
    // parity: the clock starts at zero and the countdown runs to below zero, so 600 + 301.
    CHECK(ticksUntil(zombie, under, 1000) == 901);

    ServerEntity husk = mobOf("husk");
    for (std::int32_t tick = 0; tick < 600; ++tick) {
        CHECK_FALSE(conversions::tick(husk, under).has_value());
    }
    CHECK(conversions::isShaking(husk));
    // parity: surfacing does not stop a conversion that has started.
    const std::optional<std::int32_t> left = ticksUntil(husk, {}, 1000);
    CHECK(left == 301);
}

TEST_CASE("surfacing before the clock is up starts it over") {
    ServerEntity zombie = mobOf("zombie");
    for (std::int32_t tick = 0; tick < 500; ++tick) {
        static_cast<void>(conversions::tick(zombie, {.eyeInWater = true}));
    }
    static_cast<void>(conversions::tick(zombie, {}));
    CHECK(zombie.conversionClock == -1);
    ServerEntity drowned = mobOf("drowned");
    CHECK_FALSE(ticksUntil(drowned, {.eyeInWater = true}, 2000).has_value());
}

TEST_CASE("a skeleton freezes into a stray only while it stays in powder snow") {
    ServerEntity skeleton = mobOf("skeleton");
    const std::optional<std::int32_t> due = ticksUntil(skeleton, {.inPowderSnow = true}, 1000);
    CHECK(due == 141 + 300);
    ServerEntity other = mobOf("skeleton");
    for (std::int32_t tick = 0; tick < 200; ++tick) {
        static_cast<void>(conversions::tick(other, {.inPowderSnow = true}));
    }
    CHECK(conversions::isShaking(other));
    static_cast<void>(conversions::tick(other, {}));
    CHECK_FALSE(conversions::isShaking(other));
}

TEST_CASE("piglins and hoglins zombify outside the Nether unless immune") {
    ServerEntity piglin = mobOf("piglin");
    const std::optional<std::int32_t> due = ticksUntil(piglin, {.piglinsZombify = true}, 1000);
    CHECK(due == 301);
    ServerEntity hoglin = mobOf("hoglin");
    const conversions::Around outside{.piglinsZombify = true};
    std::optional<conversions::Conversion> conversion;
    for (std::int32_t tick = 0; tick < 400 && !conversion.has_value(); ++tick) {
        conversion = conversions::tick(hoglin, outside);
    }
    REQUIRE(conversion.has_value());
    CHECK(conversion->into.path() == "zoglin");
    CHECK_FALSE(conversion->keepCanPickUpLoot);

    ServerEntity immune = mobOf("piglin_brute");
    immune.immuneToZombification = true;
    CHECK_FALSE(ticksUntil(immune, outside, 1000).has_value());
    ServerEntity home = mobOf("piglin");
    CHECK_FALSE(ticksUntil(home, {}, 1000).has_value());
}

TEST_CASE("the converted mob keeps its place, equipment and name, and nothing it must lose") {
    NbtCompound from;
    from.putString("id", "minecraft:zombie");
    from.putFloat("Health", 3.0F);
    from.putString("CustomName", "Bob");
    from.putBoolean("IsBaby", true);
    from.putBoolean("CanPickUpLoot", true);
    from.putBoolean("CanBreakDoors", true);
    NbtCompound equipment;
    equipment.putString("mainhand", "iron_sword");
    from.set("equipment", basalt::wire::nbt::NbtTag::ofCompound(equipment));
    from.putInt("InWaterTime", 600);

    const NbtCompound to = conversions::convertedTag(from, Identifier::vanilla("zombie"),
                                                     conversions::Conversion{.into = Identifier::vanilla("drowned")});
    CHECK(to.getString("id") == "minecraft:drowned");
    CHECK(to.getString("CustomName") == "Bob");
    CHECK(to.getBoolean("IsBaby"));
    CHECK(to.getBoolean("CanPickUpLoot"));
    CHECK(to.getBoolean("CanBreakDoors"));
    CHECK(to.contains("equipment"));
    CHECK_FALSE(to.contains("Health"));
    CHECK_FALSE(to.contains("UUID"));
    CHECK_FALSE(to.contains("InWaterTime"));

    const NbtCompound zoglin = conversions::convertedTag(
        from, Identifier::vanilla("hoglin"),
        conversions::Conversion{.into = Identifier::vanilla("zoglin"), .keepCanPickUpLoot = false});
    CHECK_FALSE(zoglin.contains("CanPickUpLoot"));
}

TEST_CASE("a zombie villager being cured turns back once its countdown is spent") {
    ServerEntity zombie = mobOf("zombie_villager");
    CHECK_FALSE(ticksUntil(zombie, {.eyeInWater = true}, 2000).has_value());
    zombie.conversionCountdown = 100;
    CHECK(conversions::isShaking(zombie));
    std::optional<conversions::Conversion> cured;
    std::int32_t ticks = 0;
    while (!cured.has_value() && ticks < 200) {
        cured = conversions::tick(zombie, {.cureProgress = 2});
        ++ticks;
    }
    REQUIRE(cured.has_value());
    CHECK(ticks == 50);
    CHECK(cured->into.path() == "villager");
    CHECK(cured->nausea);
    CHECK_FALSE(cured->keepEquipment);
}

TEST_CASE("a tadpole becomes a frog after twenty minutes, wherever it is") {
    ServerEntity tadpole = mobOf("tadpole");
    // parity: `Tadpole.aiStep` counts every turn, so the clock is the age and nothing gates it.
    const std::optional<std::int32_t> grown = ticksUntil(tadpole, {}, conversions::kTadpoleTicks + 10);
    REQUIRE(grown.has_value());
    CHECK(*grown == conversions::kTadpoleTicks);

    ServerEntity another = mobOf("tadpole");
    for (std::int32_t tick = 1; tick < conversions::kTadpoleTicks; ++tick) {
        static_cast<void>(conversions::tick(another, {}));
    }
    const std::optional<conversions::Conversion> due = conversions::tick(another, {});
    REQUIRE(due.has_value());
    CHECK(due->into == Identifier::vanilla("frog"));
    // parity: `ConversionParams.single(this, false, false)` plus `setPersistenceRequired`.
    CHECK_FALSE(due->keepEquipment);
    CHECK_FALSE(due->keepCanPickUpLoot);
    CHECK(due->persistent);

    // parity: a dead one stops counting; `remove` is what ends it, not the clock.
    ServerEntity dying = mobOf("tadpole");
    dying.dying = true;
    CHECK_FALSE(ticksUntil(dying, {}, conversions::kTadpoleTicks + 10).has_value());
}
