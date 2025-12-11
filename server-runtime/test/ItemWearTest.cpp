#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/item/ItemComponents.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>

#include <doctest.h>

#include <string>

using namespace basalt::server::runtime::item;
using basalt::server::storage::StoredItemStack;
using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtCompound;
namespace registry = basalt::server::registry;
namespace rng = basalt::server::random;

namespace {

[[nodiscard]] StoredItemStack stack(const std::string& id, std::int32_t count = 1) {
    return StoredItemStack{.id = id, .count = count};
}

[[nodiscard]] StoredItemStack worn(const std::string& id, std::int32_t damage, std::int32_t count = 1) {
    StoredItemStack made = stack(id, count);
    made.components.putInt(itemComponents::kDamage, damage);
    return made;
}

[[nodiscard]] std::int32_t maxOf(const std::string& id) {
    return registry::itemDurability::maxDamageOf(Identifier::parse(id).value());
}

} // namespace

TEST_CASE("a tool wears and a block does not") {
    CHECK(itemWear::isDamageable(stack("minecraft:iron_pickaxe")));
    CHECK(itemWear::isDamageable(stack("minecraft:diamond_sword")));
    CHECK_FALSE(itemWear::isDamageable(stack("minecraft:stone")));
    CHECK_FALSE(itemWear::isDamageable(stack("minecraft:not_an_item")));
    // parity: the unbreakable component alone stops it, whatever the item is.
    StoredItemStack blessed = stack("minecraft:iron_pickaxe");
    blessed.components.set(itemComponents::kUnbreakable, basalt::wire::nbt::NbtTag::ofCompound(NbtCompound{}));
    CHECK_FALSE(itemWear::isDamageable(blessed));
}

TEST_CASE("a stack with no damage component has taken none") {
    CHECK(itemWear::damage(stack("minecraft:iron_pickaxe")) == 0);
    CHECK(itemWear::damage(worn("minecraft:iron_pickaxe", 17)) == 17);
    // parity: the value is clamped to what the item can take, so a bad one reads as full wear.
    const std::int32_t max = maxOf("minecraft:iron_pickaxe");
    CHECK(itemWear::damage(worn("minecraft:iron_pickaxe", max + 100)) == max);
    CHECK(itemWear::damage(worn("minecraft:iron_pickaxe", -5)) == 0);
    CHECK(itemWear::maxDamage(stack("minecraft:iron_pickaxe")) == max);
    CHECK(max > 0);
}

TEST_CASE("what a use costs comes from the item's own table") {
    CHECK(itemWear::costPerBlock(stack("minecraft:iron_pickaxe")) > 0);
    CHECK(itemWear::costPerAttack(stack("minecraft:iron_sword")) > 0);
    CHECK(itemWear::costPerBlock(stack("minecraft:stone")) == 0);
    CHECK(itemWear::costPerAttack(stack("minecraft:stone")) == 0);
}

TEST_CASE("wear lands on the damage component and nothing else") {
    rng::RandomSource random = rng::RandomSource::legacy(1);

    const std::optional<StoredItemStack> used = itemWear::hurt(stack("minecraft:iron_pickaxe"), 3, random);

    REQUIRE(used.has_value());
    CHECK(itemWear::damage(*used) == 3);
    CHECK(used->id == "minecraft:iron_pickaxe");
    CHECK(used->count == 1);

    const std::optional<StoredItemStack> again = itemWear::hurt(*used, 4, random);
    REQUIRE(again.has_value());
    CHECK(itemWear::damage(*again) == 7);
}

TEST_CASE("nothing wears an item that does not, and nothing is no wear at all") {
    rng::RandomSource random = rng::RandomSource::legacy(1);

    CHECK(itemWear::hurt(stack("minecraft:stone"), 5, random)->components.empty());
    CHECK(itemWear::hurt(stack("minecraft:iron_pickaxe"), 0, random)->components.empty());
    CHECK(itemWear::hurt(stack("minecraft:iron_pickaxe"), -1, random)->components.empty());
}

TEST_CASE("the last point of durability is the one that breaks it") {
    const std::int32_t max = maxOf("minecraft:iron_pickaxe");
    rng::RandomSource random = rng::RandomSource::legacy(1);

    // parity: `isBroken` is `>= max`, so a tool never sits at zero durability.
    const std::optional<StoredItemStack> nearly = itemWear::hurt(worn("minecraft:iron_pickaxe", max - 2), 1, random);
    REQUIRE(nearly.has_value());
    CHECK(itemWear::damage(*nearly) == max - 1);

    CHECK_FALSE(itemWear::hurt(worn("minecraft:iron_pickaxe", max - 1), 1, random).has_value());
    CHECK_FALSE(itemWear::hurt(worn("minecraft:iron_pickaxe", 0), max, random).has_value());
}

TEST_CASE("breaking one of a stack leaves the rest, unworn") {
    const std::int32_t max = maxOf("minecraft:iron_pickaxe");
    rng::RandomSource random = rng::RandomSource::legacy(1);

    const std::optional<StoredItemStack> left = itemWear::hurt(worn("minecraft:iron_pickaxe", max - 1, 3), 1, random);

    REQUIRE(left.has_value());
    CHECK(left->count == 2);
    // parity: the wear on the broken one goes with it; what is left carries what it carried.
    CHECK(itemWear::damage(*left) == max - 1);

    // Two is still more than one, so one of them survives.
    const std::optional<StoredItemStack> pair = itemWear::hurt(worn("minecraft:iron_pickaxe", max - 1, 2), 1, random);
    REQUIRE(pair.has_value());
    CHECK(pair->count == 1);
}

TEST_CASE("an enchantment gets to lower the cost before the wear lands") {
    rng::RandomSource random = rng::RandomSource::legacy(1);
    std::int32_t asked = 0;

    const std::optional<StoredItemStack> used =
        itemWear::hurt(stack("minecraft:iron_pickaxe"), 4, random,
                       [&asked](const StoredItemStack& stack, std::int32_t amount, rng::RandomSource&) {
                           ++asked;
                           CHECK(stack.id == "minecraft:iron_pickaxe");
                           CHECK(amount == 4);
                           return 1;
                       });

    REQUIRE(used.has_value());
    CHECK(asked == 1);
    CHECK(itemWear::damage(*used) == 1);

    // A reduction all the way to nothing leaves the tool untouched.
    const std::optional<StoredItemStack> spared =
        itemWear::hurt(stack("minecraft:iron_pickaxe"), 4, random,
                       [](const StoredItemStack&, std::int32_t, rng::RandomSource&) { return 0; });
    REQUIRE(spared.has_value());
    CHECK(spared->components.empty());
}
