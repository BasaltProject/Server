#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/item/ItemDamageEffects.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>

#include <doctest.h>

#include <map>
#include <string>

using basalt::server::runtime::item::ItemDamageEffects;
using basalt::server::storage::StoredItemStack;
using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtCompound;
using basalt::wire::nbt::NbtList;
using basalt::wire::nbt::NbtTag;
using basalt::wire::nbt::NbtType;
namespace player = basalt::server::runtime::player;
namespace rng = basalt::server::random;

namespace {

/// An enchantment definition shaped like the jar's own: one conditional `item_damage` effect.
[[nodiscard]] NbtCompound definitionWith(NbtTag chance, const std::string& type = "minecraft:remove_binomial",
                                         std::optional<std::string> items = std::nullopt) {
    NbtCompound applied;
    applied.putString("type", type);
    applied.set("chance", std::move(chance));

    NbtCompound conditional;
    conditional.set("effect", NbtTag::ofCompound(applied));
    if (items.has_value()) {
        NbtCompound predicate;
        predicate.putString("items", *items);
        NbtCompound requirements;
        requirements.set("predicate", NbtTag::ofCompound(predicate));
        conditional.set("requirements", NbtTag::ofCompound(requirements));
    }

    NbtList list{NbtType::Compound};
    list.push(NbtTag::ofCompound(conditional));
    NbtCompound effects;
    effects.set("minecraft:item_damage", NbtTag::ofList(std::move(list)));
    NbtCompound definition;
    definition.set("effects", NbtTag::ofCompound(effects));
    return definition;
}

[[nodiscard]] StoredItemStack enchanted(const std::string& id, const std::map<std::string, std::int32_t>& levels) {
    StoredItemStack stack{.id = id, .count = 1, .components = NbtCompound{}};
    NbtCompound stored;
    for (const auto& [name, level] : levels) {
        stored.putInt(name, level);
    }
    stack.components.set(std::string{player::kEnchantmentsComponent}, NbtTag::ofCompound(stored));
    return stack;
}

} // namespace

TEST_CASE("an unenchanted item takes the wear it was handed") {
    const ItemDamageEffects effects{[](const Identifier&) -> const NbtCompound* { return nullptr; }};
    const StoredItemStack plain{.id = "minecraft:diamond_sword", .count = 1, .components = NbtCompound{}};
    rng::RandomSource random = rng::RandomSource::legacy(1);

    CHECK(effects.process(plain, 5, random) == 5);
}

TEST_CASE("an enchantment with no definition anywhere takes nothing off") {
    const ItemDamageEffects effects{[](const Identifier&) -> const NbtCompound* { return nullptr; }};
    rng::RandomSource random = rng::RandomSource::legacy(1);

    CHECK(effects.process(enchanted("minecraft:diamond_sword", {{"minecraft:unbreaking", 3}}), 5, random) == 5);
}

TEST_CASE("a chance of one takes every point of wear off") {
    const NbtCompound definition = definitionWith(NbtTag::ofFloat(1.0F));
    const ItemDamageEffects effects{[&definition](const Identifier&) { return &definition; }};
    rng::RandomSource random = rng::RandomSource::legacy(1);

    CHECK(effects.process(enchanted("minecraft:diamond_sword", {{"minecraft:unbreaking", 1}}), 5, random) == 0);
}

TEST_CASE("a chance of nothing leaves the wear alone, and draws once a point all the same") {
    const NbtCompound definition = definitionWith(NbtTag::ofFloat(0.0F));
    const ItemDamageEffects effects{[&definition](const Identifier&) { return &definition; }};
    rng::RandomSource counted = rng::RandomSource::legacy(1);

    CHECK(effects.process(enchanted("minecraft:diamond_sword", {{"minecraft:unbreaking", 1}}), 4, counted) == 4);

    // parity: `RemoveBinomial.process` rolls once per point of wear whether any of them lands.
    rng::RandomSource same = rng::RandomSource::legacy(1);
    for (std::int32_t roll = 0; roll < 4; roll++) {
        (void)same.nextFloat();
    }
    CHECK(same.nextFloat() == counted.nextFloat());
}

TEST_CASE("a linear chance grows with the level, as unbreaking's own does") {
    // parity: `LevelBasedValue.Linear`, base plus per-level above the first.
    NbtCompound linear;
    linear.putString("type", "minecraft:linear");
    linear.putFloat("base", 0.0F);
    linear.putFloat("per_level_above_first", 1.0F);
    const NbtCompound definition = definitionWith(NbtTag::ofCompound(linear));
    const ItemDamageEffects effects{[&definition](const Identifier&) { return &definition; }};
    rng::RandomSource random = rng::RandomSource::legacy(1);

    // At level one the chance is the base, which is nothing.
    CHECK(effects.process(enchanted("minecraft:diamond_sword", {{"minecraft:unbreaking", 1}}), 5, random) == 5);
    // At level two it is one, which takes everything.
    CHECK(effects.process(enchanted("minecraft:diamond_sword", {{"minecraft:unbreaking", 2}}), 5, random) == 0);
}

TEST_CASE("a fraction is worked out from both halves, and a zero below it is not divided by") {
    NbtCompound fraction;
    fraction.putString("type", "minecraft:fraction");
    fraction.putFloat("numerator", 1.0F);
    fraction.putFloat("denominator", 1.0F);
    const NbtCompound whole = definitionWith(NbtTag::ofCompound(fraction));
    const ItemDamageEffects taking{[&whole](const Identifier&) { return &whole; }};
    rng::RandomSource random = rng::RandomSource::legacy(1);
    CHECK(taking.process(enchanted("minecraft:diamond_sword", {{"minecraft:unbreaking", 1}}), 4, random) == 0);

    fraction.putFloat("denominator", 0.0F);
    const NbtCompound divided = definitionWith(NbtTag::ofCompound(fraction));
    const ItemDamageEffects nothing{[&divided](const Identifier&) { return &divided; }};
    CHECK(nothing.process(enchanted("minecraft:diamond_sword", {{"minecraft:unbreaking", 1}}), 4, random) == 4);
}

TEST_CASE("a constant is the value it names") {
    NbtCompound constant;
    constant.putString("type", "minecraft:constant");
    constant.putFloat("value", 1.0F);
    const NbtCompound definition = definitionWith(NbtTag::ofCompound(constant));
    const ItemDamageEffects effects{[&definition](const Identifier&) { return &definition; }};
    rng::RandomSource random = rng::RandomSource::legacy(1);

    CHECK(effects.process(enchanted("minecraft:diamond_sword", {{"minecraft:unbreaking", 1}}), 3, random) == 0);
}

TEST_CASE("an effect this build does not model leaves the wear as it was") {
    const NbtCompound definition = definitionWith(NbtTag::ofFloat(1.0F), "minecraft:add");
    const ItemDamageEffects effects{[&definition](const Identifier&) { return &definition; }};
    rng::RandomSource random = rng::RandomSource::legacy(1);

    CHECK(effects.process(enchanted("minecraft:diamond_sword", {{"minecraft:unbreaking", 1}}), 5, random) == 5);
}

TEST_CASE("an effect that names one item applies to that item alone") {
    const NbtCompound definition =
        definitionWith(NbtTag::ofFloat(1.0F), "minecraft:remove_binomial", "minecraft:diamond_sword");
    const ItemDamageEffects effects{[&definition](const Identifier&) { return &definition; }};
    rng::RandomSource random = rng::RandomSource::legacy(1);

    CHECK(effects.process(enchanted("minecraft:diamond_sword", {{"minecraft:unbreaking", 1}}), 5, random) == 0);
    CHECK(effects.process(enchanted("minecraft:diamond_pickaxe", {{"minecraft:unbreaking", 1}}), 5, random) == 5);
}

TEST_CASE("an effect that names a tag asks whoever loaded the tags") {
    const NbtCompound definition =
        definitionWith(NbtTag::ofFloat(1.0F), "minecraft:remove_binomial", "#minecraft:swords");
    std::string asked;
    const ItemDamageEffects effects{[&definition](const Identifier&) { return &definition; },
                                    [&asked](std::string_view tag, const Identifier& item) {
                                        asked = std::string{tag};
                                        return item.path() == "diamond_sword";
                                    }};
    rng::RandomSource random = rng::RandomSource::legacy(1);

    CHECK(effects.process(enchanted("minecraft:diamond_sword", {{"minecraft:unbreaking", 1}}), 5, random) == 0);
    CHECK(asked == "minecraft:swords");
    CHECK(effects.process(enchanted("minecraft:diamond_pickaxe", {{"minecraft:unbreaking", 1}}), 5, random) == 5);
}

TEST_CASE("without anyone to answer a tag nothing is in it") {
    const NbtCompound definition =
        definitionWith(NbtTag::ofFloat(1.0F), "minecraft:remove_binomial", "#minecraft:swords");
    const ItemDamageEffects effects{[&definition](const Identifier&) { return &definition; }};
    rng::RandomSource random = rng::RandomSource::legacy(1);

    CHECK(effects.process(enchanted("minecraft:diamond_sword", {{"minecraft:unbreaking", 1}}), 5, random) == 5);
}

TEST_CASE("two enchantments each take their turn at the same wear") {
    NbtCompound half;
    half.putString("type", "minecraft:constant");
    half.putFloat("value", 1.0F);
    const NbtCompound definition = definitionWith(NbtTag::ofCompound(half));
    const ItemDamageEffects effects{[&definition](const Identifier&) { return &definition; }};
    rng::RandomSource random = rng::RandomSource::legacy(1);

    // The first takes everything, so the second finds nothing left to roll for.
    CHECK(effects.process(enchanted("minecraft:diamond_sword", {{"minecraft:unbreaking", 1}, {"minecraft:mending", 1}}),
                          6, random) == 0);
}
