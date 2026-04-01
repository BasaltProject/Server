#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/entity/MobStatusEffects.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <doctest.h>

#include <string>

using basalt::server::runtime::entity::ServerEntity;
using basalt::server::runtime::world::ActiveEffect;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
namespace effects = basalt::server::runtime::entity::mobStatusEffects;
namespace mobEffects = basalt::server::runtime::world::mobEffects;

namespace {

[[nodiscard]] ServerEntity mobOf(const std::string& path) {
    return ServerEntity{1, Uuid{1, 1}, Identifier::vanilla(path), 0, 0.0, 64.0, 0.0};
}

} // namespace

TEST_CASE("who refuses which effect") {
    CHECK_FALSE(effects::canBeAffected(mobOf("zombie"), mobEffects::poison()));
    CHECK_FALSE(effects::canBeAffected(mobOf("skeleton"), mobEffects::regeneration()));
    CHECK(effects::canBeAffected(mobOf("zombie"), mobEffects::wither()));
    CHECK_FALSE(effects::canBeAffected(mobOf("spider"), mobEffects::poison()));
    CHECK_FALSE(effects::canBeAffected(mobOf("wither_skeleton"), mobEffects::wither()));
    CHECK_FALSE(effects::canBeAffected(mobOf("ender_dragon"), mobEffects::haste()));
    CHECK(effects::canBeAffected(mobOf("cow"), mobEffects::poison()));
}

TEST_CASE("a second dose replaces the first only when it is stronger or longer") {
    ServerEntity cow = mobOf("cow");
    const Identifier speed = Identifier::vanilla("speed");
    CHECK(effects::add(cow, ActiveEffect{.effect = speed, .amplifier = 0, .remainingTicks = 100}));
    CHECK_FALSE(effects::add(cow, ActiveEffect{.effect = speed, .amplifier = 0, .remainingTicks = 50}));
    CHECK(cow.effects.at(speed).remainingTicks == 100);
    CHECK(effects::add(cow, ActiveEffect{.effect = speed, .amplifier = 0, .remainingTicks = 200}));
    CHECK(cow.effects.at(speed).remainingTicks == 200);
    CHECK(effects::add(cow, ActiveEffect{.effect = speed, .amplifier = 1, .remainingTicks = 20}));
    CHECK(cow.effects.at(speed).amplifier == 1);
    CHECK(cow.effects.at(speed).remainingTicks == 20);
}

TEST_CASE("poison stops at one point and every effect runs out") {
    ServerEntity cow = mobOf("cow");
    cow.health = 1.0;
    REQUIRE(effects::add(cow, ActiveEffect{.effect = mobEffects::poison(), .remainingTicks = 50}));
    std::vector<Identifier> expired;
    std::int32_t hurts = 0;
    for (std::int32_t tick = 0; tick < 60; ++tick) {
        hurts += static_cast<std::int32_t>(effects::tick(cow, expired).size());
    }
    CHECK(hurts == 0);
    REQUIRE(expired.size() == 1);
    CHECK(cow.effects.empty());

    ServerEntity pig = mobOf("pig");
    REQUIRE(effects::add(pig, ActiveEffect{.effect = mobEffects::wither(), .remainingTicks = 80}));
    std::int32_t witherHits = 0;
    for (std::int32_t tick = 0; tick < 80; ++tick) {
        for (const effects::TickEffect& owed : effects::tick(pig, expired)) {
            CHECK(owed.cause == "wither");
            ++witherHits;
        }
    }
    CHECK(witherHits == 2);
}

TEST_CASE("an instant effect heals the living and hurts the undead") {
    CHECK(effects::instantOf(mobOf("cow"), mobEffects::instantHealth(), 0).heal == doctest::Approx(4.0F));
    CHECK(effects::instantOf(mobOf("zombie"), mobEffects::instantHealth(), 0).damage == doctest::Approx(6.0F));
    CHECK(effects::instantOf(mobOf("zombie"), mobEffects::instantDamage(), 1).heal == doctest::Approx(8.0F));
}

TEST_CASE("effects survive a save") {
    ServerEntity cow = mobOf("cow");
    REQUIRE(effects::add(cow, ActiveEffect{.effect = mobEffects::haste(), .amplifier = 2, .remainingTicks = 77}));
    basalt::wire::nbt::NbtCompound tag;
    tag.set("active_effects", effects::encode(cow));
    ServerEntity copy = mobOf("cow");
    effects::decode(copy, tag);
    REQUIRE(copy.effects.contains(mobEffects::haste()));
    CHECK(copy.effects.at(mobEffects::haste()).amplifier == 2);
    CHECK(copy.effects.at(mobEffects::haste()).remainingTicks == 77);
}

TEST_CASE("strength and speed change the mob's own attributes while they last") {
    ServerEntity zombie = mobOf("zombie");
    const double damage = zombie.attributeWithModifiers("minecraft:attack_damage");
    const double speed = zombie.attributeWithModifiers("minecraft:movement_speed");
    REQUIRE(effects::add(zombie,
                         ActiveEffect{.effect = Identifier::vanilla("strength"), .amplifier = 1, .remainingTicks = 2}));
    REQUIRE(effects::add(zombie, ActiveEffect{.effect = Identifier::vanilla("slowness"), .remainingTicks = 20}));
    CHECK(zombie.attributeWithModifiers("minecraft:attack_damage") == doctest::Approx(damage + 6.0));
    CHECK(zombie.attributeWithModifiers("minecraft:movement_speed") ==
          doctest::Approx(speed * (1.0 + static_cast<double>(-0.15F))));
    std::vector<Identifier> expired;
    static_cast<void>(effects::tick(zombie, expired));
    static_cast<void>(effects::tick(zombie, expired));
    CHECK(zombie.attributeWithModifiers("minecraft:attack_damage") == doctest::Approx(damage));
    effects::clear(zombie);
    CHECK(zombie.attributeWithModifiers("minecraft:movement_speed") == doctest::Approx(speed));
}

TEST_CASE("a stronger, shorter dose hides the weaker one, which comes back when it runs out") {
    ServerEntity cow = mobOf("cow");
    const Identifier speed = Identifier::vanilla("speed");
    REQUIRE(effects::add(cow, ActiveEffect{.effect = speed, .amplifier = 0, .remainingTicks = 100}));
    REQUIRE(effects::add(cow, ActiveEffect{.effect = speed, .amplifier = 2, .remainingTicks = 10}));
    CHECK(cow.effects.at(speed).amplifier == 2);
    std::vector<Identifier> expired;
    for (std::int32_t tick = 0; tick < 10; ++tick) {
        static_cast<void>(effects::tick(cow, expired));
    }
    CHECK(expired.empty());
    CHECK(cow.effects.at(speed).amplifier == 0);
    CHECK(cow.effects.at(speed).remainingTicks == 90);
    // The hidden dose travels with the save.
    REQUIRE(effects::add(cow, ActiveEffect{.effect = speed, .amplifier = 1, .remainingTicks = 5}));
    basalt::wire::nbt::NbtCompound saved;
    saved.set("active_effects", effects::encode(cow));
    ServerEntity loaded = mobOf("cow");
    effects::decode(loaded, saved);
    REQUIRE(loaded.hiddenEffects.contains(speed));
    CHECK(loaded.hiddenEffects.at(speed).front().remainingTicks == 90);
}
