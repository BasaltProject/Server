#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/world/EnchantmentAttributes.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <string>

using basalt::server::storage::StoredItemStack;
using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtCompound;
using basalt::wire::nbt::NbtList;
using basalt::wire::nbt::NbtTag;
using basalt::wire::nbt::NbtType;
using basalt::wire::protocol::play::UpdateAttributesPacket;
namespace attributes = basalt::server::runtime::world::enchantmentAttributes;
namespace player = basalt::server::runtime::player;
namespace reg = basalt::server::registry;

namespace {

[[nodiscard]] StoredItemStack enchantedWith(const std::string& enchantment, std::int32_t level) {
    StoredItemStack stack{.id = "minecraft:diamond_pickaxe", .count = 1, .components = NbtCompound{}};
    NbtCompound levels;
    levels.putInt(enchantment, level);
    stack.components.set(std::string{player::kEnchantmentsComponent}, NbtTag::ofCompound(levels));
    return stack;
}

/// One enchantment definition shaped like the jar's: a slot list and one attribute effect.
[[nodiscard]] NbtCompound definitionWith(const std::string& slot, const std::string& attribute, NbtCompound amount,
                                         const std::string& operation = "add_value", const std::string& id = "") {
    NbtCompound effect;
    effect.putString("attribute", attribute);
    effect.set("amount", NbtTag::ofCompound(std::move(amount)));
    effect.putString("operation", operation);
    if (!id.empty()) {
        effect.putString("id", id);
    }

    NbtList list{NbtType::Compound};
    list.push(NbtTag::ofCompound(effect));
    NbtCompound effects;
    effects.set("minecraft:attributes", NbtTag::ofList(std::move(list)));

    NbtList slots{NbtType::String};
    slots.push(NbtTag::ofString(slot));

    NbtCompound definition;
    definition.set("slots", NbtTag::ofList(std::move(slots)));
    definition.set("effects", NbtTag::ofCompound(effects));
    return definition;
}

[[nodiscard]] NbtCompound levelsSquared(double added) {
    NbtCompound amount;
    amount.putString("type", "minecraft:levels_squared");
    amount.putDouble("added", added);
    return amount;
}

[[nodiscard]] const UpdateAttributesPacket::AttributeSnapshot*
snapshotOf(const std::vector<UpdateAttributesPacket::AttributeSnapshot>& snapshots, const std::string& attribute) {
    const std::optional<std::int32_t> id = reg::attributeIds::protocolId(attribute);
    REQUIRE(id.has_value());
    for (const UpdateAttributesPacket::AttributeSnapshot& snapshot : snapshots) {
        if (snapshot.attribute == *id) {
            return &snapshot;
        }
    }
    return nullptr;
}

} // namespace

TEST_CASE("an empty hand still sends every attribute a held item could touch") {
    const std::vector<UpdateAttributesPacket::AttributeSnapshot> snapshots = attributes::snapshotsFor(std::nullopt, {});

    // parity aside: sending them all every time is what lets an attribute go back to its base when
    // the enchantment that moved it is put away.
    CHECK(snapshots.size() == 6);
    for (const UpdateAttributesPacket::AttributeSnapshot& snapshot : snapshots) {
        CHECK(snapshot.modifiers.empty());
    }
}

TEST_CASE("the bases are the ones vanilla gives those attributes") {
    const std::vector<UpdateAttributesPacket::AttributeSnapshot> snapshots = attributes::snapshotsFor(std::nullopt, {});

    CHECK(snapshotOf(snapshots, "minecraft:block_break_speed")->base == doctest::Approx(1.0));
    CHECK(snapshotOf(snapshots, "minecraft:attack_speed")->base == doctest::Approx(1.0));
    CHECK(snapshotOf(snapshots, "minecraft:attack_damage")->base == doctest::Approx(1.0));
    CHECK(snapshotOf(snapshots, "minecraft:submerged_mining_speed")->base == doctest::Approx(0.2));
    CHECK(snapshotOf(snapshots, "minecraft:mining_efficiency")->base == doctest::Approx(0.0));
    CHECK(snapshotOf(snapshots, "minecraft:sweeping_damage_ratio")->base == doctest::Approx(0.0));
}

TEST_CASE("efficiency moves the mining attribute by the square of its level") {
    // parity: `minecraft:efficiency`'s own `levels_squared` amount, level squared plus one.
    const NbtCompound definition = definitionWith("mainhand", "minecraft:mining_efficiency", levelsSquared(1.0));
    const std::vector<UpdateAttributesPacket::AttributeSnapshot> snapshots = attributes::snapshotsFor(
        enchantedWith("minecraft:efficiency", 4), [&definition](const Identifier&) { return &definition; });

    const UpdateAttributesPacket::AttributeSnapshot* mining = snapshotOf(snapshots, "minecraft:mining_efficiency");
    REQUIRE(mining != nullptr);
    REQUIRE(mining->modifiers.size() == 1);
    CHECK(mining->modifiers[0].amount == doctest::Approx(17.0));
    CHECK(mining->modifiers[0].operation == UpdateAttributesPacket::Operation::AddValue);
    // With no id of its own the modifier is named after the enchantment.
    CHECK(mining->modifiers[0].id == Identifier::vanilla("enchantment.efficiency"));
}

TEST_CASE("a linear amount grows a step at a time above the first level") {
    NbtCompound linear;
    linear.putString("type", "minecraft:linear");
    linear.putDouble("base", 1.0);
    linear.putDouble("per_level_above_first", 0.5);
    const NbtCompound definition = definitionWith("mainhand", "minecraft:attack_damage", linear);

    const std::vector<UpdateAttributesPacket::AttributeSnapshot> first = attributes::snapshotsFor(
        enchantedWith("minecraft:sharpness", 1), [&definition](const Identifier&) { return &definition; });
    CHECK(snapshotOf(first, "minecraft:attack_damage")->modifiers[0].amount == doctest::Approx(1.0));

    const std::vector<UpdateAttributesPacket::AttributeSnapshot> third = attributes::snapshotsFor(
        enchantedWith("minecraft:sharpness", 3), [&definition](const Identifier&) { return &definition; });
    CHECK(snapshotOf(third, "minecraft:attack_damage")->modifiers[0].amount == doctest::Approx(2.0));
}

TEST_CASE("an amount with neither shape is its value, or its base when it has none") {
    NbtCompound plain;
    plain.putDouble("value", 3.5);
    const NbtCompound valued = definitionWith("mainhand", "minecraft:attack_speed", plain);
    const std::vector<UpdateAttributesPacket::AttributeSnapshot> snapshots = attributes::snapshotsFor(
        enchantedWith("minecraft:whatever", 1), [&valued](const Identifier&) { return &valued; });
    CHECK(snapshotOf(snapshots, "minecraft:attack_speed")->modifiers[0].amount == doctest::Approx(3.5));

    NbtCompound based;
    based.putDouble("base", 2.5);
    const NbtCompound fallback = definitionWith("mainhand", "minecraft:attack_speed", based);
    const std::vector<UpdateAttributesPacket::AttributeSnapshot> other = attributes::snapshotsFor(
        enchantedWith("minecraft:whatever", 1), [&fallback](const Identifier&) { return &fallback; });
    CHECK(snapshotOf(other, "minecraft:attack_speed")->modifiers[0].amount == doctest::Approx(2.5));
}

TEST_CASE("the operation is the one the effect names, and add_value when it names none") {
    NbtCompound amount;
    amount.putDouble("value", 1.0);
    const NbtCompound multiplied =
        definitionWith("mainhand", "minecraft:attack_damage", amount, "add_multiplied_total");
    const std::vector<UpdateAttributesPacket::AttributeSnapshot> snapshots = attributes::snapshotsFor(
        enchantedWith("minecraft:sharpness", 1), [&multiplied](const Identifier&) { return &multiplied; });
    CHECK(snapshotOf(snapshots, "minecraft:attack_damage")->modifiers[0].operation ==
          UpdateAttributesPacket::Operation::AddMultipliedTotal);

    const NbtCompound base = definitionWith("mainhand", "minecraft:attack_damage", amount, "add_multiplied_base");
    CHECK(snapshotOf(attributes::snapshotsFor(enchantedWith("minecraft:sharpness", 1),
                                              [&base](const Identifier&) { return &base; }),
                     "minecraft:attack_damage")
              ->modifiers[0]
              .operation == UpdateAttributesPacket::Operation::AddMultipliedBase);

    const NbtCompound nonsense = definitionWith("mainhand", "minecraft:attack_damage", amount, "nothing_like_it");
    CHECK(snapshotOf(attributes::snapshotsFor(enchantedWith("minecraft:sharpness", 1),
                                              [&nonsense](const Identifier&) { return &nonsense; }),
                     "minecraft:attack_damage")
              ->modifiers[0]
              .operation == UpdateAttributesPacket::Operation::AddValue);
}

TEST_CASE("an enchantment worn rather than held moves nothing") {
    const NbtCompound definition = definitionWith("feet", "minecraft:mining_efficiency", levelsSquared(1.0));

    const std::vector<UpdateAttributesPacket::AttributeSnapshot> snapshots = attributes::snapshotsFor(
        enchantedWith("minecraft:depth_strider", 3), [&definition](const Identifier&) { return &definition; });

    CHECK(snapshotOf(snapshots, "minecraft:mining_efficiency")->modifiers.empty());
}

TEST_CASE("a hand slot of any kind counts as held") {
    for (const std::string& slot : {std::string{"mainhand"}, std::string{"hand"}, std::string{"any"}}) {
        const NbtCompound definition = definitionWith(slot, "minecraft:mining_efficiency", levelsSquared(0.0));
        const std::vector<UpdateAttributesPacket::AttributeSnapshot> snapshots = attributes::snapshotsFor(
            enchantedWith("minecraft:efficiency", 2), [&definition](const Identifier&) { return &definition; });
        CHECK(snapshotOf(snapshots, "minecraft:mining_efficiency")->modifiers.size() == 1);
    }
}

TEST_CASE("an attribute a held item cannot touch is left out") {
    const NbtCompound definition = definitionWith("mainhand", "minecraft:max_health", levelsSquared(0.0));

    const std::vector<UpdateAttributesPacket::AttributeSnapshot> snapshots = attributes::snapshotsFor(
        enchantedWith("minecraft:whatever", 1), [&definition](const Identifier&) { return &definition; });

    CHECK(snapshots.size() == 6);
    for (const UpdateAttributesPacket::AttributeSnapshot& snapshot : snapshots) {
        CHECK(snapshot.modifiers.empty());
    }
}

TEST_CASE("an enchantment with no definition to read moves nothing") {
    const std::vector<UpdateAttributesPacket::AttributeSnapshot> snapshots = attributes::snapshotsFor(
        enchantedWith("minecraft:efficiency", 5), [](const Identifier&) -> const NbtCompound* { return nullptr; });

    for (const UpdateAttributesPacket::AttributeSnapshot& snapshot : snapshots) {
        CHECK(snapshot.modifiers.empty());
    }
}

TEST_CASE("an effect that names its own modifier keeps that name") {
    const NbtCompound definition = definitionWith("mainhand", "minecraft:attack_damage", levelsSquared(0.0),
                                                  "add_value", "minecraft:base_attack_damage");

    const std::vector<UpdateAttributesPacket::AttributeSnapshot> snapshots = attributes::snapshotsFor(
        enchantedWith("minecraft:sharpness", 1), [&definition](const Identifier&) { return &definition; });

    CHECK(snapshotOf(snapshots, "minecraft:attack_damage")->modifiers[0].id ==
          Identifier::vanilla("base_attack_damage"));
}
