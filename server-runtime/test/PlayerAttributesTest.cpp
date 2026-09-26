#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/player/PlayerAttributes.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>

#include <doctest.h>

#include <string>

using basalt::server::storage::StoredItemStack;
using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtCompound;
using basalt::wire::nbt::NbtList;
using basalt::wire::nbt::NbtTag;
using basalt::wire::nbt::NbtType;
namespace attributes = basalt::server::runtime::player::playerAttributes;

TEST_CASE("the three operations stack as `AttributeInstance.calculateValue` does") {
    // parity: every add first, then each multiplied base on the added total, then each total in turn.
    const std::vector<attributes::Modifier> modifiers{
        {.id = Identifier::vanilla("a"), .amount = 2.0, .operation = attributes::Operation::AddValue},
        {.id = Identifier::vanilla("b"), .amount = 0.5, .operation = attributes::Operation::AddMultipliedBase},
        {.id = Identifier::vanilla("c"), .amount = 1.0, .operation = attributes::Operation::AddMultipliedTotal},
    };
    // (1 + 2) + 3 * 0.5 = 4.5, doubled = 9.
    CHECK(attributes::calculate("minecraft:attack_damage", 1.0, modifiers) == doctest::Approx(9.0));
    // The armour attribute never goes past thirty.
    CHECK(attributes::calculate("minecraft:armor", 25.0, modifiers) == doctest::Approx(30.0));
}

TEST_CASE("a stack gives its default modifiers only in its own slot, and a component replaces them") {
    attributes::Modifiers worn;
    attributes::addStackModifiers(StoredItemStack{.id = "minecraft:diamond_chestplate", .count = 1}, "chest", {}, worn);
    REQUIRE(worn.contains("minecraft:armor"));
    CHECK(worn.at("minecraft:armor").front().id == Identifier::vanilla("armor.chestplate"));
    CHECK(worn.at("minecraft:armor").front().amount == doctest::Approx(8.0));
    attributes::Modifiers held;
    attributes::addStackModifiers(StoredItemStack{.id = "minecraft:diamond_chestplate", .count = 1}, "mainhand", {},
                                  held);
    CHECK(held.empty());

    NbtCompound entry;
    entry.putString("type", "minecraft:movement_speed");
    entry.putString("id", "minecraft:fast");
    entry.putDouble("amount", 0.5);
    entry.putString("operation", "add_multiplied_base");
    entry.putString("slot", "hand");
    NbtList list{NbtType::Compound};
    list.push(NbtTag::ofCompound(entry));
    StoredItemStack sword{.id = "minecraft:diamond_sword", .count = 1};
    sword.components.set("minecraft:attribute_modifiers", NbtTag::ofList(std::move(list)));
    attributes::Modifiers custom;
    attributes::addStackModifiers(sword, "offhand", {}, custom);
    CHECK(custom.contains("minecraft:movement_speed"));
    CHECK_FALSE(custom.contains("minecraft:attack_damage"));
}
