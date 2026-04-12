#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/PostPiercingEffects.hpp>
#include <basalt/wire/nbt/Snbt.hpp>

#include <doctest.h>

#include <cmath>

using basalt::server::runtime::world::postPiercing::Outcome;
using basalt::server::runtime::world::postPiercing::User;
using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtCompound;

namespace postPiercing = basalt::server::runtime::world::postPiercing;

namespace {

/// A made-up enchantment with the shape lunge has: every effect kind, behind the same requirements.
constexpr std::string_view kDefinition = R"({effects: {"minecraft:post_piercing_attack": [{
    effect: {type: "minecraft:all_of", effects: [
        {type: "minecraft:change_item_damage", amount: 2.0d},
        {type: "minecraft:apply_exhaustion", amount: {type: "minecraft:linear", base: 3.0d, per_level_above_first: 1.0d}},
        {type: "minecraft:apply_impulse", coordinate_scale: [1.0d, 0.0d, 1.0d], direction: [0.0d, 0.0d, 1.0d],
         magnitude: {type: "minecraft:linear", base: 0.5d, per_level_above_first: 0.25d}},
        {type: "minecraft:play_sound", pitch: 1.0d, volume: 1.0d, sound: ["test:one", "test:two"]}]},
    requirements: {type: "minecraft:all_of", terms: [
        {type: "minecraft:inverted", term: {type: "minecraft:entity_properties", entity: "this",
                                             predicate: {"minecraft:vehicle": {}}}},
        {type: "minecraft:entity_properties", entity: "this", predicate: {"minecraft:flags": {is_in_water: 0b}}},
        {type: "minecraft:any_of", terms: [
            {type: "minecraft:entity_properties", entity: "this",
             predicate: {"minecraft:type_specific/player": {gamemode: ["creative"]}}},
            {type: "minecraft:entity_properties", entity: "this",
             predicate: {"minecraft:type_specific/player": {food: {level: {min: 7}}}}}]}]}}]}})";

struct Fixture {
    NbtCompound definition;
    basalt::server::storage::StoredItemStack weapon;

    explicit Fixture(std::int32_t level) {
        definition = *basalt::wire::nbt::snbt::parse(kDefinition)->get<NbtCompound>();
        NbtCompound levels;
        levels.putInt("test:push", level);
        weapon.id = "minecraft:iron_spear";
        weapon.components.set("minecraft:enchantments", basalt::wire::nbt::NbtTag::ofCompound(std::move(levels)));
    }

    [[nodiscard]] Outcome run(const User& user) const {
        return postPiercing::of(weapon, [this](const Identifier&) { return &definition; }, user);
    }
};

} // namespace

TEST_CASE("an unmet requirement leaves every effect out") {
    const Fixture fixture{1};
    const Outcome hungry = fixture.run(User{.food = 6});
    CHECK(hungry.itemDamage == 0);
    CHECK_FALSE(hungry.pushed);
    CHECK(hungry.sounds.empty());
    CHECK_FALSE(fixture.run(User{.riding = true, .food = 20}).pushed);
    CHECK_FALSE(fixture.run(User{.inWater = true, .food = 20}).pushed);
    CHECK(fixture.run(User{.gamemode = "creative", .food = 0}).pushed);
}

TEST_CASE("the effects scale with the level and push along the look") {
    const Fixture fixture{3};
    const Outcome outcome = fixture.run(User{.food = 7, .yaw = 90.0F});
    CHECK(outcome.itemDamage == 2);
    CHECK(outcome.exhaustion == doctest::Approx(3.0F + (1.0F * 2.0F)));
    // Facing yaw 90 looks down negative x, and the vertical is scaled away.
    const double magnitude = 0.5 + (0.25 * 2.0);
    CHECK(outcome.impulse[0] == doctest::Approx(-magnitude));
    CHECK(outcome.impulse[1] == doctest::Approx(0.0));
    CHECK(std::abs(outcome.impulse[2]) < 1.0E-9);
    REQUIRE(outcome.sounds.size() == 1);
    // Past the end of the list the last sound is the one played.
    CHECK(outcome.sounds.front().name == "test:two");
}
