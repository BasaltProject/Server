#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/entity/MobCategory.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <doctest.h>

#include <string>

using namespace basalt::server::runtime::entity;
namespace registry = basalt::server::registry;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;

namespace {

[[nodiscard]] ServerEntity spawn(std::string path, std::int32_t id = 1) {
    return ServerEntity{id, Uuid{1, 2}, Identifier{"minecraft", std::move(path)}, 0, 0.0, 64.0, 0.0};
}

} // namespace

TEST_CASE("a spawned entity starts with what its kind is born with") {
    const ServerEntity cow = spawn("cow");
    CHECK(cow.entityId() == 1);
    CHECK(cow.type().toString() == "minecraft:cow");
    CHECK(cow.y == 64.0);

    // parity: a cow's own attribute supplier, not a global default.
    CHECK(cow.health == 10.0);
    CHECK(cow.attribute(registry::entityAttributes::kMaxHealth) == 10.0);
    CHECK(cow.attribute(registry::entityAttributes::kMovementSpeed) == doctest::Approx(0.2));

    const ServerEntity zombie = spawn("zombie");
    CHECK(zombie.health == 20.0);
    // A zombie walks faster than a cow because its own supplier says so.
    CHECK(zombie.attribute(registry::entityAttributes::kMovementSpeed) == doctest::Approx(0.23));

    // Something with no attributes at all falls back to the attribute's own default.
    const ServerEntity arrow = spawn("arrow");
    CHECK(arrow.health == ServerEntity::kDefaultMaxHealth);
    CHECK(arrow.attribute(attribute::kScale) == 1.0);
    CHECK(arrow.attribute(attribute::kMaxHealth) == 20.0);
    CHECK(arrow.attribute(attribute::kMovementSpeed) == doctest::Approx(0.7));
    CHECK(arrow.attribute(attribute::kAttackDamage) == 2.0);
    CHECK(arrow.attribute(attribute::kArmor) == 0.0);
    CHECK(arrow.attribute("minecraft:nothing_at_all") == 0.0);

    // The fallbacks are keyed by the full id, which is how a command names them; the registry's
    // own table answers the short spelling as well, and only for the types that have one.
    CHECK(arrow.attribute("max_health") == 0.0);
    CHECK(cow.attribute("max_health") == 10.0);
    CHECK(cow.attribute(attribute::kMaxHealth) == 10.0);
}

TEST_CASE("the two entities whose constructor does something of its own") {
    // parity: `Dolphin`, which raises the air supply and turns loot picking on in its constructor.
    const ServerEntity dolphin = spawn("dolphin");
    CHECK(dolphin.airSupply == ServerEntity::kDolphinMaxAir);
    CHECK(dolphin.canPickUpLoot);

    const ServerEntity cow = spawn("cow");
    CHECK(cow.airSupply == ServerEntity::kDefaultMaxAir);
    CHECK_FALSE(cow.canPickUpLoot);

    // parity: `TraderLlama`, whose escort is temporary from the moment it exists.
    CHECK(spawn("trader_llama").despawnDelay == ServerEntity::kTraderLlamaDespawnDelay);
    CHECK(spawn("llama").despawnDelay == 0);
}

TEST_CASE("what a command set beats what the entity was born with") {
    ServerEntity cow = spawn("cow");
    CHECK(cow.attribute(registry::entityAttributes::kMaxHealth) == 10.0);
    cow.attributeValues[std::string{registry::entityAttributes::kMaxHealth}] = 40.0;
    CHECK(cow.attribute(registry::entityAttributes::kMaxHealth) == 40.0);
    // And the ones it did not touch are still its own.
    CHECK(cow.attribute(registry::entityAttributes::kMovementSpeed) == doctest::Approx(0.2));
}

TEST_CASE("the three operations of a modifier, in the order vanilla applies them") {
    ServerEntity zombie = spawn("zombie");
    const std::string speed{registry::entityAttributes::kMovementSpeed};
    zombie.attributeValues[speed] = 1.0;

    // Nothing on it is the base value.
    CHECK(zombie.attributeWithModifiers(speed) == doctest::Approx(1.0));

    // Additions first.
    zombie.attributeModifiers[speed]["a"] =
        AttributeModifier{.id = "a", .amount = 1.0, .operation = AttributeOperation::AddValue};
    CHECK(zombie.attributeWithModifiers(speed) == doctest::Approx(2.0));

    // Then each multiply-base, against what the additions left, not against the running total: two
    // of them add their share of the same number rather than compounding.
    zombie.attributeModifiers[speed]["b"] =
        AttributeModifier{.id = "b", .amount = 0.5, .operation = AttributeOperation::AddMultipliedBase};
    zombie.attributeModifiers[speed]["c"] =
        AttributeModifier{.id = "c", .amount = 0.5, .operation = AttributeOperation::AddMultipliedBase};
    CHECK(zombie.attributeWithModifiers(speed) == doctest::Approx(4.0));

    // Then each multiply-total, which does compound.
    zombie.attributeModifiers[speed]["d"] =
        AttributeModifier{.id = "d", .amount = 1.0, .operation = AttributeOperation::AddMultipliedTotal};
    zombie.attributeModifiers[speed]["e"] =
        AttributeModifier{.id = "e", .amount = 1.0, .operation = AttributeOperation::AddMultipliedTotal};
    CHECK(zombie.attributeWithModifiers(speed) == doctest::Approx(16.0));

    // A modifier of the same id replaces rather than stacking, which is what keeps an effect
    // applied twice from counting twice.
    zombie.attributeModifiers[speed]["a"] =
        AttributeModifier{.id = "a", .amount = 0.0, .operation = AttributeOperation::AddValue};
    CHECK(zombie.attributeWithModifiers(speed) == doctest::Approx(8.0));

    // An attribute nobody modified is its plain value.
    CHECK(zombie.attributeWithModifiers(registry::entityAttributes::kMaxHealth) == 20.0);
}

TEST_CASE("what `/attribute` calls each operation") {
    CHECK(attributeOperationName(AttributeOperation::AddValue) == "add_value");
    CHECK(attributeOperationName(AttributeOperation::AddMultipliedBase) == "add_multiplied_base");
    CHECK(attributeOperationName(AttributeOperation::AddMultipliedTotal) == "add_multiplied_total");
    CHECK(attributeOperationByName("add_value") == AttributeOperation::AddValue);
    CHECK(attributeOperationByName("add_multiplied_total") == AttributeOperation::AddMultipliedTotal);
    CHECK_FALSE(attributeOperationByName("multiply").has_value());
    // The serialized name, not the enum's: a command takes what the wire calls it.
    CHECK_FALSE(attributeOperationByName("ADD_VALUE").has_value());
}

TEST_CASE("the eight categories and what each is worth to the world") {
    // parity: MobCategory's own numbers, which decide the spawn cap, the despawn distance and
    // whether that kind counts as friendly.
    CHECK(mobCategoryFacts(MobCategory::Monster).maxPerChunk == 70);
    CHECK_FALSE(mobCategoryFacts(MobCategory::Monster).friendly);
    CHECK_FALSE(mobCategoryFacts(MobCategory::Monster).persistent);
    CHECK(mobCategoryFacts(MobCategory::Creature).maxPerChunk == 10);
    CHECK(mobCategoryFacts(MobCategory::Creature).persistent);
    CHECK(mobCategoryFacts(MobCategory::Ambient).maxPerChunk == 15);
    CHECK(mobCategoryFacts(MobCategory::Axolotls).maxPerChunk == 5);
    CHECK(mobCategoryFacts(MobCategory::UndergroundWaterCreature).maxPerChunk == 5);
    CHECK(mobCategoryFacts(MobCategory::WaterCreature).maxPerChunk == 5);
    CHECK(mobCategoryFacts(MobCategory::WaterAmbient).maxPerChunk == 20);
    // The only one with a different despawn distance, and the only one with no cap at all.
    CHECK(mobCategoryFacts(MobCategory::WaterAmbient).despawnDistance == 64);
    CHECK(mobCategoryFacts(MobCategory::Misc).maxPerChunk == -1);
    CHECK(mobCategoryFacts(MobCategory::Misc).persistent);
    CHECK(kNoDespawnDistance == 32);

    CHECK(mobCategoryByName("monster") == MobCategory::Monster);
    CHECK(mobCategoryByName("underground_water_creature") == MobCategory::UndergroundWaterCreature);
    CHECK(mobCategoryByName("misc") == MobCategory::Misc);
    CHECK_FALSE(mobCategoryByName("MONSTER").has_value());
    CHECK_FALSE(mobCategoryByName("boss").has_value());
}

TEST_CASE("a scoreboard tag goes on once and comes off once") {
    ServerEntity cow = spawn("cow");
    CHECK(cow.tags.empty());
    CHECK(cow.addTag("marked"));
    CHECK_FALSE(cow.addTag("marked"));
    CHECK(cow.hasTag("marked"));
    CHECK_FALSE(cow.hasTag("other"));
    CHECK(cow.tags.size() == 1);
    CHECK(cow.removeTag("marked"));
    CHECK_FALSE(cow.removeTag("marked"));
    CHECK(cow.tags.empty());
}

TEST_CASE("an entity is the size its type says") {
    const ServerEntity cow = spawn("cow");
    CHECK(cow.width() == doctest::Approx(0.9));
    CHECK(cow.height() == doctest::Approx(1.4));
    const ServerEntity slime = spawn("slime");
    CHECK(slime.cubeSize == 1);
    CHECK(spawn("tnt").fuse == ServerEntity::kNoFuse);
}
