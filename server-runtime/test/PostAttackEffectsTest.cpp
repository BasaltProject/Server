#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/PostAttackEffects.hpp>
#include <basalt/wire/nbt/Snbt.hpp>

#include <doctest.h>

using basalt::server::runtime::world::postAttack::Equipped;
using basalt::server::runtime::world::postAttack::Hit;
using basalt::server::runtime::world::postAttack::Outcome;
using basalt::server::runtime::world::postAttack::Side;
using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtCompound;

namespace postAttack = basalt::server::runtime::world::postAttack;
namespace weaponDamage = basalt::server::runtime::world::weaponDamage;

namespace {

/// Made-up enchantments with the shapes the ones that fire on a hit have.
constexpr std::string_view kIgnite = R"({slots: ["mainhand"], effects: {"minecraft:post_attack": [{
    affected: "victim", enchanted: "attacker",
    effect: {type: "minecraft:ignite", duration: {type: "minecraft:linear", base: 4.0f, per_level_above_first: 4.0f}},
    requirements: {type: "minecraft:damage_source_properties", predicate: {is_direct: 1b}}}]}})";

constexpr std::string_view kReflect = R"({slots: ["armor"], effects: {"minecraft:post_attack": [{
    affected: "attacker", enchanted: "victim",
    effect: {type: "minecraft:all_of", effects: [
        {type: "minecraft:damage_entity", damage_type: "minecraft:thorns", min_damage: 1.0f, max_damage: 5.0f},
        {type: "minecraft:change_item_damage", amount: 2.0f}]},
    requirements: {type: "minecraft:random_chance", chance: {type: "minecraft:enchantment_level",
                   amount: {type: "minecraft:linear", base: 1.0f, per_level_above_first: 0.0f}}}}]}})";

constexpr std::string_view kSlow = R"({slots: ["mainhand"], effects: {"minecraft:post_attack": [{
    affected: "victim", enchanted: "attacker",
    effect: {type: "minecraft:apply_mob_effect", to_apply: "minecraft:slowness", min_duration: 1.5f,
             max_duration: 1.5f, min_amplifier: 3.0f, max_amplifier: 3.0f},
    requirements: {type: "minecraft:entity_properties", entity: "this",
                   predicate: {"minecraft:entity_type": "#test:bugs"}}}]}})";

constexpr std::string_view kBurst = R"({slots: ["mainhand"], effects: {"minecraft:post_attack": [{
    affected: "attacker", enchanted: "attacker",
    effect: {type: "minecraft:explode", block_interaction: "trigger", radius: 3.5f,
             knockback_multiplier: {type: "minecraft:lookup", values: [1.2f, 1.75f, 2.2f],
                                    fallback: {type: "minecraft:linear", base: 1.5f, per_level_above_first: 0.35f}},
             small_particle: {type: "minecraft:gust_emitter_small"}, large_particle: {type: "minecraft:gust_emitter_large"},
             sound: "minecraft:entity.wind_charge.wind_burst"},
    requirements: {type: "minecraft:entity_properties", entity: "direct_attacker",
                   predicate: {"minecraft:flags": {is_flying: 0b}, "minecraft:movement": {fall_distance: {min: 1.5d}}}}}]}})";

[[nodiscard]] NbtCompound parsed(std::string_view snbt) {
    return *basalt::wire::nbt::snbt::parse(snbt)->get<NbtCompound>();
}

[[nodiscard]] basalt::server::storage::StoredItemStack enchanted(std::string id, std::string_view enchantment,
                                                                 std::int32_t level) {
    basalt::server::storage::StoredItemStack stack;
    stack.id = std::move(id);
    NbtCompound levels;
    levels.putInt(enchantment, level);
    stack.components.set("minecraft:enchantments", basalt::wire::nbt::NbtTag::ofCompound(std::move(levels)));
    return stack;
}

struct Fixture {
    NbtCompound ignite = parsed(kIgnite);
    NbtCompound reflect = parsed(kReflect);
    NbtCompound slow = parsed(kSlow);
    NbtCompound burst = parsed(kBurst);

    [[nodiscard]] weaponDamage::DefinitionOf definitions() const {
        return [this](const Identifier& id) -> const NbtCompound* {
            if (id.path() == "ignite") {
                return &ignite;
            }
            if (id.path() == "reflect") {
                return &reflect;
            }
            if (id.path() == "burst") {
                return &burst;
            }
            return id.path() == "slow" ? &slow : nullptr;
        };
    }
};

} // namespace

TEST_CASE("a direct blow sets the victim alight for the weapon's level") {
    const Fixture fixture;
    const weaponDamage::Victim victim{.type = Identifier::vanilla("player"), .inTag = {}};
    auto random = basalt::server::random::RandomSource::legacy(1);
    Hit hit{.weapon = Equipped{.slot = 27,
                               .equipmentSlot = "mainhand",
                               .stack = enchanted("minecraft:iron_sword", "test:ignite", 2)},
            .victim = &victim};
    const Outcome outcome = postAttack::of(hit, fixture.definitions(), random);
    REQUIRE(outcome.ignites.size() == 1);
    CHECK(outcome.ignites[0].target == Side::kVictim);
    CHECK(outcome.ignites[0].ticks == 160);

    hit.direct = false;
    CHECK(postAttack::of(hit, fixture.definitions(), random).ignites.empty());
}

TEST_CASE("armour hurts the attacker back and wears itself, and only from an armour slot") {
    const Fixture fixture;
    const weaponDamage::Victim victim{.type = Identifier::vanilla("player"), .inTag = {}};
    auto random = basalt::server::random::RandomSource::legacy(7);
    const auto chest = enchanted("minecraft:iron_chestplate", "test:reflect", 1);
    Hit hit{.victimEquipment = {Equipped{.slot = 37, .equipmentSlot = "chest", .stack = chest}}, .victim = &victim};
    const Outcome outcome = postAttack::of(hit, fixture.definitions(), random);
    REQUIRE(outcome.damages.size() == 1);
    CHECK(outcome.damages[0].target == Side::kAttacker);
    CHECK(outcome.damages[0].type == Identifier::vanilla("thorns"));
    CHECK(outcome.damages[0].amount >= 1.0F);
    CHECK(outcome.damages[0].amount <= 5.0F);
    REQUIRE(outcome.wear.size() == 1);
    CHECK(outcome.wear[0].owner == Side::kVictim);
    CHECK(outcome.wear[0].slot == 37);
    CHECK(outcome.wear[0].amount == 2);

    hit.victimEquipment = {Equipped{.slot = 27, .equipmentSlot = "mainhand", .stack = chest}};
    CHECK(postAttack::of(hit, fixture.definitions(), random).damages.empty());
}

TEST_CASE("an effect waits on the victim's entity type") {
    const Fixture fixture;
    auto random = basalt::server::random::RandomSource::legacy(3);
    const weaponDamage::Victim spider{.type = Identifier::vanilla("spider"),
                                      .inTag = [](const Identifier& tag) { return tag.toString() == "test:bugs"; }};
    const weaponDamage::Victim player{.type = Identifier::vanilla("player"),
                                      .inTag = [](const Identifier&) { return false; }};
    Hit hit{.weapon = Equipped{.slot = 27,
                               .equipmentSlot = "mainhand",
                               .stack = enchanted("minecraft:iron_sword", "test:slow", 1)},
            .victim = &spider};
    const Outcome outcome = postAttack::of(hit, fixture.definitions(), random);
    REQUIRE(outcome.effects.size() == 1);
    CHECK(outcome.effects[0].effect == Identifier::vanilla("slowness"));
    CHECK(outcome.effects[0].ticks == 30);
    CHECK(outcome.effects[0].amplifier == 3);

    hit.victim = &player;
    CHECK(postAttack::of(hit, fixture.definitions(), random).effects.empty());
}

TEST_CASE("a gust answers a fall that is not a glide, its push looked up by level") {
    const Fixture fixture;
    const weaponDamage::Victim victim{.type = Identifier::vanilla("zombie"), .inTag = {}};
    auto random = basalt::server::random::RandomSource::legacy(5);
    Hit hit{
        .weapon =
            Equipped{.slot = 27, .equipmentSlot = "mainhand", .stack = enchanted("minecraft:mace", "test:burst", 2)},
        .victim = &victim,
        .attackerFallDistance = 2.0};
    const Outcome outcome = postAttack::of(hit, fixture.definitions(), random);
    REQUIRE(outcome.explosions.size() == 1);
    CHECK(outcome.explosions[0].target == Side::kAttacker);
    CHECK(outcome.explosions[0].radius == doctest::Approx(3.5F));
    CHECK(*outcome.explosions[0].knockbackMultiplier == doctest::Approx(1.75F));
    CHECK(outcome.explosions[0].triggersBlocks);

    hit.attackerFlying = true;
    CHECK(postAttack::of(hit, fixture.definitions(), random).explosions.empty());
    hit.attackerFlying = false;
    hit.attackerFallDistance = 1.0;
    CHECK(postAttack::of(hit, fixture.definitions(), random).explosions.empty());
}
