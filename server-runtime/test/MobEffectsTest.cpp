#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/PotionEffects.hpp>

#include <doctest.h>

#include <cstdint>
#include <set>

using namespace basalt::server::runtime::world;
using basalt::wire::common::Identifier;

TEST_CASE("the registry runs from speed at zero to the last effect there is") {
    CHECK(mobEffects::protocolIdOf(Identifier::vanilla("speed")) == 0);
    CHECK(mobEffects::protocolIdOf(Identifier::vanilla("slowness")) == 1);
    CHECK(mobEffects::protocolIdOf(Identifier::vanilla("instant_health")) == 5);
    CHECK(mobEffects::protocolIdOf(Identifier::vanilla("bad_omen")) == 30);
    CHECK(mobEffects::protocolIdOf(Identifier::vanilla("breath_of_the_nautilus")) == 39);
    CHECK(mobEffects::ids().size() == 40);
}

TEST_CASE("an effect nobody has heard of has no id, and neither has one from another namespace") {
    CHECK_FALSE(mobEffects::protocolIdOf(Identifier::vanilla("not_an_effect")).has_value());
    CHECK_FALSE(mobEffects::protocolIdOf(Identifier{"mypack", "speed"}).has_value());
    CHECK_FALSE(mobEffects::protocolIdOf(Identifier::vanilla("")).has_value());
}

TEST_CASE("every id reads back as the name it was given, and nothing outside the table does") {
    for (std::int32_t id = 0; id < 40; ++id) {
        const std::optional<Identifier> name = mobEffects::byProtocolId(id);
        REQUIRE(name.has_value());
        CHECK(mobEffects::protocolIdOf(*name) == id);
    }
    CHECK_FALSE(mobEffects::byProtocolId(-1).has_value());
    CHECK_FALSE(mobEffects::byProtocolId(40).has_value());
    CHECK_FALSE(mobEffects::byProtocolId(1000).has_value());
}

TEST_CASE("every name in the registry is its own") {
    const std::vector<Identifier> all = mobEffects::ids();
    const std::set<Identifier> distinct{all.begin(), all.end()};

    CHECK(distinct.size() == all.size());
    // And the list is in id order, which is what makes the index the id.
    for (std::size_t index = 0; index < all.size(); ++index) {
        CHECK(mobEffects::protocolIdOf(all[index]) == static_cast<std::int32_t>(index));
    }
}

TEST_CASE("the effects that other systems name are the ones they mean") {
    CHECK(mobEffects::regeneration() == Identifier::vanilla("regeneration"));
    CHECK(mobEffects::poison() == Identifier::vanilla("poison"));
    CHECK(mobEffects::wither() == Identifier::vanilla("wither"));
    CHECK(mobEffects::hunger() == Identifier::vanilla("hunger"));
    CHECK(mobEffects::instantHealth() == Identifier::vanilla("instant_health"));
    CHECK(mobEffects::instantDamage() == Identifier::vanilla("instant_damage"));
    CHECK(mobEffects::haste() == Identifier::vanilla("haste"));
    CHECK(mobEffects::miningFatigue() == Identifier::vanilla("mining_fatigue"));
    CHECK(mobEffects::badOmen() == Identifier::vanilla("bad_omen"));
    CHECK(mobEffects::raidOmen() == Identifier::vanilla("raid_omen"));
    CHECK(mobEffects::heroOfTheVillage() == Identifier::vanilla("hero_of_the_village"));
    CHECK(mobEffects::resistance() == Identifier::vanilla("resistance"));
    CHECK(mobEffects::absorption() == Identifier::vanilla("absorption"));
    // The same reference every time, so a caller may keep one.
    CHECK(&mobEffects::poison() == &mobEffects::poison());
}

TEST_CASE("an effect counts down unless it was given for ever") {
    const ActiveEffect timed{.effect = mobEffects::poison(), .amplifier = 1, .remainingTicks = 100};
    CHECK_FALSE(timed.infinite());

    // parity: minus one on the wire is what the client draws as an infinity.
    const ActiveEffect forever{.effect = mobEffects::poison(), .amplifier = 0, .remainingTicks = -1};
    CHECK(forever.infinite());

    const ActiveEffect over{.effect = mobEffects::poison(), .amplifier = 0, .remainingTicks = 0};
    CHECK_FALSE(over.infinite());
}

TEST_CASE("an effect is shown, not ambient, and carries an icon unless told otherwise") {
    const ActiveEffect plain{.effect = *mobEffects::byProtocolId(0)};
    CHECK_FALSE(plain.ambient);
    CHECK(plain.visible);
    CHECK(plain.showIcon);
    CHECK(plain.amplifier == 0);

    const ActiveEffect beacon{.effect = mobEffects::haste(),
                              .amplifier = 1,
                              .remainingTicks = 200,
                              .ambient = true,
                              .visible = false,
                              .showIcon = false};
    CHECK(beacon.ambient);
    CHECK_FALSE(beacon.visible);
    CHECK_FALSE(beacon.showIcon);
    CHECK_FALSE(beacon == plain);
}

TEST_CASE("a potion's effects come scaled and floored, never under a tick, custom ones after") {
    basalt::wire::nbt::NbtCompound contents;
    contents.putString("potion", "minecraft:strong_poison");
    basalt::wire::nbt::NbtCompound custom;
    custom.putString("id", "minecraft:slowness");
    custom.putByte("amplifier", 2);
    custom.putInt("duration", 3);
    contents.set("custom_effects",
                 basalt::wire::nbt::NbtTag::ofList(basalt::wire::nbt::NbtList{
                     basalt::wire::nbt::NbtType::Compound, {basalt::wire::nbt::NbtTag::ofCompound(custom)}}));
    // parity: a tipped arrow's eighth: 432 poison ticks are 54, and 3 slowness ticks floor to the one.
    const std::vector<ActiveEffect> effects = potionEffects::of(contents, 0.125F);
    REQUIRE(effects.size() == 2);
    CHECK(effects[0].effect == Identifier::vanilla("poison"));
    CHECK(effects[0].amplifier == 1);
    CHECK(effects[0].remainingTicks == 54);
    CHECK(effects[1].effect == Identifier::vanilla("slowness"));
    CHECK(effects[1].amplifier == 2);
    CHECK(effects[1].remainingTicks == 1);
    CHECK(potionEffects::ofPotion("turtle_master").size() == 2);
    CHECK(potionEffects::ofPotion("water").empty());
}
