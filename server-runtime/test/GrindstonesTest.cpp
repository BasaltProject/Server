#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/item/ItemComponents.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>
#include <basalt/server/runtime/world/Grindstones.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>

using namespace basalt::server::runtime::world;
namespace itemComponents = basalt::server::runtime::item::itemComponents;
namespace itemWear = basalt::server::runtime::item::itemWear;
namespace registry = basalt::server::registry;
using basalt::server::storage::StoredItemStack;
using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtCompound;
using basalt::wire::nbt::NbtTag;

namespace {

[[nodiscard]] StoredItemStack stack(const std::string& id, std::int32_t count = 1) {
    return StoredItemStack{.id = id, .count = count};
}

[[nodiscard]] StoredItemStack enchanted(const std::string& id, const std::string& enchantment, std::int32_t level) {
    StoredItemStack made = stack(id);
    NbtCompound levels;
    levels.putInt(enchantment, level);
    made.components.set(id == "minecraft:enchanted_book" ? itemComponents::kStoredEnchantments
                                                         : itemComponents::kEnchantments,
                        NbtTag::ofCompound(std::move(levels)));
    return made;
}

[[nodiscard]] StoredItemStack worn(const std::string& id, std::int32_t damage) {
    StoredItemStack made = stack(id);
    made.components.putInt(itemComponents::kDamage, damage);
    return made;
}

[[nodiscard]] bool isBindingCurse(const Identifier& id) { return id.path().starts_with("binding_curse"); }

[[nodiscard]] std::int32_t flatCost(const Identifier&, std::int32_t level) { return 5 * level; }

[[nodiscard]] std::int32_t maxOf(const std::string& id) {
    return registry::itemDurability::maxDamageOf(Identifier::parse(id).value());
}

[[nodiscard]] std::size_t enchantmentsOn(const StoredItemStack& stack) {
    const NbtCompound* levels = stack.components.getCompound(itemComponents::kEnchantments);
    return levels == nullptr ? 0 : levels->names().size();
}

} // namespace

TEST_CASE("an empty grindstone makes nothing") {
    CHECK_FALSE(grindstones::combine(nullptr, nullptr).output.has_value());
}

TEST_CASE("one enchanted item on its own comes out bare") {
    const StoredItemStack input = enchanted("minecraft:iron_sword", "minecraft:sharpness", 3);

    const grindstones::Result result = grindstones::combine(&input, nullptr, isBindingCurse, flatCost);

    REQUIRE(result.output.has_value());
    CHECK(enchantmentsOn(*result.output) == 0);
    // parity: five a level, which is what the table charged for it.
    CHECK(result.experience == 15);
}

TEST_CASE("a curse stays on and pays nothing") {
    const StoredItemStack input = enchanted("minecraft:iron_helmet", "minecraft:binding_curse", 1);

    const grindstones::Result result = grindstones::combine(&input, nullptr, isBindingCurse, flatCost);

    REQUIRE(result.output.has_value());
    CHECK(enchantmentsOn(*result.output) == 1);
    CHECK(result.experience == 0);
}

TEST_CASE("an item with nothing on it is not worth grinding") {
    const StoredItemStack input = stack("minecraft:iron_sword");

    CHECK_FALSE(grindstones::combine(&input, nullptr, isBindingCurse, flatCost).output.has_value());
}

TEST_CASE("an enchanted book comes out a plain book") {
    const StoredItemStack input = enchanted("minecraft:enchanted_book", "minecraft:sharpness", 1);

    const grindstones::Result result = grindstones::combine(&input, nullptr, isBindingCurse, flatCost);

    REQUIRE(result.output.has_value());
    CHECK(result.output->id == "minecraft:book");
}

TEST_CASE("two of the same tool add their durability and five per cent") {
    const std::int32_t max = maxOf("minecraft:iron_pickaxe");
    const StoredItemStack input = worn("minecraft:iron_pickaxe", max - 10);
    const StoredItemStack addition = worn("minecraft:iron_pickaxe", max - 10);

    const grindstones::Result result = grindstones::combine(&input, &addition, isBindingCurse, flatCost);

    REQUIRE(result.output.has_value());
    CHECK(itemWear::damage(*result.output) == std::max(max - (10 + 10 + (max * 5 / 100)), 0));
}

TEST_CASE("two different items make nothing") {
    const StoredItemStack input = stack("minecraft:iron_pickaxe");
    const StoredItemStack addition = stack("minecraft:iron_sword");

    CHECK_FALSE(grindstones::combine(&input, &addition, isBindingCurse, flatCost).output.has_value());
}

TEST_CASE("a stack of more than one makes nothing") {
    const StoredItemStack input = enchanted("minecraft:enchanted_book", "minecraft:sharpness", 1);
    StoredItemStack many = input;
    many.count = 2;

    CHECK_FALSE(grindstones::combine(&many, nullptr, isBindingCurse, flatCost).output.has_value());
}

TEST_CASE("what it hands back is half of what it took off, plus a roll of the same") {
    // parity: `getExperienceAmount`, which rounds the half up and then rolls under it.
    CHECK(grindstones::experienceFor(0, 0) == 0);
    CHECK(grindstones::experienceFor(1, 0) == 1);
    CHECK(grindstones::experienceFor(10, 0) == 5);
    CHECK(grindstones::experienceFor(10, 4) == 9);
    CHECK(grindstones::experienceFor(9, 0) == 5);
}
