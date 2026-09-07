#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/item/ItemComponents.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>
#include <basalt/server/runtime/world/Anvils.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>

using namespace basalt::server::runtime::world;
namespace itemComponents = basalt::server::runtime::item::itemComponents;
using basalt::server::storage::StoredItemStack;
using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtCompound;
namespace itemWear = basalt::server::runtime::item::itemWear;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] StoredItemStack stack(const std::string& id, std::int32_t count = 1) {
    return StoredItemStack{.id = id, .count = count};
}

[[nodiscard]] StoredItemStack worn(const std::string& id, std::int32_t damage) {
    StoredItemStack made = stack(id);
    made.components.putInt(itemComponents::kDamage, damage);
    return made;
}

[[nodiscard]] StoredItemStack named(const std::string& id, const std::string& name) {
    StoredItemStack made = stack(id);
    made.components.putString(itemComponents::kCustomName, name);
    return made;
}

[[nodiscard]] StoredItemStack repaired(const std::string& id, std::int32_t cost) {
    StoredItemStack made = stack(id);
    made.components.putInt(itemComponents::kRepairCost, cost);
    return made;
}

[[nodiscard]] std::int32_t maxOf(const std::string& id) {
    return registry::itemDurability::maxDamageOf(Identifier::parse(id).value());
}

[[nodiscard]] std::optional<std::string> nameOf(const StoredItemStack& stack) {
    const basalt::wire::nbt::NbtTag* value = stack.components.find(itemComponents::kCustomName);
    const std::string* name = value == nullptr ? nullptr : value->get<std::string>();
    return name == nullptr ? std::nullopt : std::optional{*name};
}

} // namespace

TEST_CASE("nothing in the first slot is nothing at all") {
    const anvils::Result result = anvils::combine(nullptr, nullptr);

    CHECK_FALSE(result.output.has_value());
    CHECK(result.cost == 0);
}

TEST_CASE("an item on its own with nothing to change costs nothing") {
    const StoredItemStack input = stack("minecraft:iron_pickaxe");

    const anvils::Result result = anvils::combine(&input, nullptr);

    CHECK_FALSE(result.output.has_value());
    CHECK(result.cost == 0);
}

TEST_CASE("renaming an item costs one level") {
    const StoredItemStack input = stack("minecraft:iron_pickaxe");

    const anvils::Result result = anvils::combine(&input, nullptr, "Digger");

    REQUIRE(result.output.has_value());
    CHECK(nameOf(*result.output) == "Digger");
    CHECK(result.cost == 1);
    // parity: renaming alone leaves the prior work where it was.
    CHECK(anvils::repairCost(*result.output) == 0);
}

TEST_CASE("renaming to the name it already has changes nothing") {
    const StoredItemStack input = named("minecraft:iron_pickaxe", "Digger");

    const anvils::Result result = anvils::combine(&input, nullptr, "Digger");

    CHECK_FALSE(result.output.has_value());
    CHECK(result.cost == 0);
}

TEST_CASE("taking a name off costs a level too") {
    const StoredItemStack input = named("minecraft:iron_pickaxe", "Digger");

    const anvils::Result result = anvils::combine(&input, nullptr, std::nullopt);

    REQUIRE(result.output.has_value());
    CHECK_FALSE(nameOf(*result.output).has_value());
    CHECK(result.cost == 1);
    // An empty name is the same as no name at all.
    const anvils::Result emptied = anvils::combine(&input, nullptr, std::string{});
    REQUIRE(emptied.output.has_value());
    CHECK_FALSE(nameOf(*emptied.output).has_value());
}

TEST_CASE("a repair material mends a quarter of the maximum for a level each") {
    const std::int32_t max = maxOf("minecraft:iron_pickaxe");
    const StoredItemStack input = worn("minecraft:iron_pickaxe", max - 1);
    const StoredItemStack ingots = stack("minecraft:iron_ingot", 4);

    const anvils::Result result = anvils::combine(&input, &ingots, std::nullopt, anvils::isRepairMaterialFor);

    REQUIRE(result.output.has_value());
    // parity: each ingot mends a quarter and costs one level, and only as many as are needed go in.
    CHECK(result.materialCount == 4);
    CHECK(result.cost == 4);
    CHECK(itemWear::damage(*result.output) == max - 1 - (4 * (max / 4)));
}

TEST_CASE("a repair takes only as many materials as the damage needs") {
    const std::int32_t max = maxOf("minecraft:iron_pickaxe");
    const StoredItemStack input = worn("minecraft:iron_pickaxe", max / 4);
    const StoredItemStack ingots = stack("minecraft:iron_ingot", 64);

    const anvils::Result result = anvils::combine(&input, &ingots, std::nullopt, anvils::isRepairMaterialFor);

    REQUIRE(result.output.has_value());
    CHECK(result.materialCount == 1);
    CHECK(result.cost == 1);
    CHECK(itemWear::damage(*result.output) == 0);
}

TEST_CASE("an undamaged item has nothing for a repair material to mend") {
    const StoredItemStack input = stack("minecraft:iron_pickaxe");
    const StoredItemStack ingots = stack("minecraft:iron_ingot", 4);

    const anvils::Result result = anvils::combine(&input, &ingots, std::nullopt, anvils::isRepairMaterialFor);

    CHECK_FALSE(result.output.has_value());
    CHECK(result.cost == 0);
}

TEST_CASE("each tool family has its own repair material") {
    CHECK(anvils::isRepairMaterialFor(stack("minecraft:wooden_axe"), stack("minecraft:oak_planks")));
    CHECK(anvils::isRepairMaterialFor(stack("minecraft:stone_hoe"), stack("minecraft:cobblestone")));
    CHECK(anvils::isRepairMaterialFor(stack("minecraft:iron_sword"), stack("minecraft:iron_ingot")));
    CHECK(anvils::isRepairMaterialFor(stack("minecraft:golden_pickaxe"), stack("minecraft:gold_ingot")));
    CHECK(anvils::isRepairMaterialFor(stack("minecraft:diamond_shovel"), stack("minecraft:diamond")));
    CHECK(anvils::isRepairMaterialFor(stack("minecraft:netherite_axe"), stack("minecraft:netherite_ingot")));
    CHECK(anvils::isRepairMaterialFor(stack("minecraft:leather_boots"), stack("minecraft:leather")));
    CHECK(anvils::isRepairMaterialFor(stack("minecraft:chainmail_helmet"), stack("minecraft:iron_ingot")));

    // The wrong material for the family, and a family nobody named.
    CHECK_FALSE(anvils::isRepairMaterialFor(stack("minecraft:iron_sword"), stack("minecraft:gold_ingot")));
    CHECK_FALSE(anvils::isRepairMaterialFor(stack("minecraft:trident"), stack("minecraft:iron_ingot")));
}

TEST_CASE("two of the same tool add their durability and twelve per cent") {
    const std::int32_t max = maxOf("minecraft:iron_pickaxe");
    const StoredItemStack input = worn("minecraft:iron_pickaxe", max - 10);
    const StoredItemStack addition = worn("minecraft:iron_pickaxe", max - 10);

    const anvils::Result result = anvils::combine(&input, &addition);

    REQUIRE(result.output.has_value());
    // parity: what is left of both, plus twelve per cent of the maximum, capped at undamaged.
    const std::int32_t expected = std::max(max - (10 + 10 + (max * 12 / 100)), 0);
    CHECK(itemWear::damage(*result.output) == expected);
    CHECK(result.cost == 2);
}

TEST_CASE("two of the same tool that would not improve it cost nothing") {
    const StoredItemStack input = stack("minecraft:iron_pickaxe");
    const StoredItemStack addition = stack("minecraft:iron_pickaxe");

    const anvils::Result result = anvils::combine(&input, &addition);

    CHECK_FALSE(result.output.has_value());
    CHECK(result.cost == 0);
}

TEST_CASE("two things that do not go together at all make nothing") {
    const StoredItemStack input = stack("minecraft:iron_pickaxe");
    const StoredItemStack addition = stack("minecraft:stone");

    const anvils::Result result = anvils::combine(&input, &addition);

    CHECK_FALSE(result.output.has_value());
    CHECK(result.cost == 0);
}

TEST_CASE("an enchantment merge is what makes an unrelated pair work") {
    const StoredItemStack input = stack("minecraft:iron_pickaxe");
    const StoredItemStack book = stack("minecraft:enchanted_book");
    std::int32_t asked = 0;

    const anvils::Result result = anvils::combine(&input, &book, std::nullopt, {},
                                                  [&asked](const StoredItemStack& one, const StoredItemStack& other) {
                                                      ++asked;
                                                      CHECK(one.id == "minecraft:iron_pickaxe");
                                                      CHECK(other.id == "minecraft:enchanted_book");
                                                      NbtCompound merged;
                                                      merged.putInt("minecraft:enchantments", 1);
                                                      return anvils::Merge{.components = merged, .cost = 3};
                                                  });

    REQUIRE(result.output.has_value());
    CHECK(asked == 1);
    CHECK(result.cost == 3);
    CHECK(result.output->components.getInt("minecraft:enchantments") == 1);
}

TEST_CASE("the prior work of both items is the tax on top of the price") {
    const StoredItemStack input = repaired("minecraft:iron_pickaxe", 3);
    const StoredItemStack book = repaired("minecraft:enchanted_book", 5);

    const anvils::Result result =
        anvils::combine(&input, &book, std::nullopt, {}, [](const StoredItemStack&, const StoredItemStack&) {
            return anvils::Merge{.components = std::nullopt, .cost = 2};
        });

    REQUIRE(result.output.has_value());
    // parity: three plus five of prior work, plus two of work.
    CHECK(result.cost == 10);
    // And the prior work of the result doubles and adds one, from the higher of the two.
    CHECK(anvils::repairCost(*result.output) == anvils::increasedRepairCost(5));
}

TEST_CASE("the prior work doubles and adds one, every time") {
    CHECK(anvils::increasedRepairCost(0) == 1);
    CHECK(anvils::increasedRepairCost(1) == 3);
    CHECK(anvils::increasedRepairCost(3) == 7);
    CHECK(anvils::increasedRepairCost(7) == 15);
    // And it never wraps round.
    CHECK(anvils::increasedRepairCost(2147483647) == 2147483647);
}

TEST_CASE("forty levels is where an anvil refuses") {
    const StoredItemStack input = repaired("minecraft:iron_pickaxe", 39);
    const StoredItemStack book = stack("minecraft:enchanted_book");

    const anvils::Result refused =
        anvils::combine(&input, &book, std::nullopt, {}, [](const StoredItemStack&, const StoredItemStack&) {
            return anvils::Merge{.components = std::nullopt, .cost = 1};
        });

    CHECK_FALSE(refused.output.has_value());
    CHECK(refused.cost == 40);

    const StoredItemStack cheaper = repaired("minecraft:iron_pickaxe", 38);
    const anvils::Result allowed =
        anvils::combine(&cheaper, &book, std::nullopt, {}, [](const StoredItemStack&, const StoredItemStack&) {
            return anvils::Merge{.components = std::nullopt, .cost = 1};
        });
    REQUIRE(allowed.output.has_value());
    CHECK(allowed.cost == 39);
}

TEST_CASE("renaming on its own is never refused, only capped") {
    const StoredItemStack input = repaired("minecraft:iron_pickaxe", 100);

    const anvils::Result result = anvils::combine(&input, nullptr, "Digger");

    // parity: the one case the forty-level wall does not apply to.
    REQUIRE(result.output.has_value());
    CHECK(result.cost == 39);
    CHECK(nameOf(*result.output) == "Digger");
    // And renaming alone leaves the prior work alone, so it can be renamed again.
    CHECK(anvils::repairCost(*result.output) == 100);
}

TEST_CASE("two things that do not go together are not renamed either") {
    const StoredItemStack input = stack("minecraft:stone");
    const StoredItemStack addition = stack("minecraft:stone");

    // parity: the pair is refused before the naming is ever looked at.
    const anvils::Result result = anvils::combine(&input, &addition, "Rocky");

    CHECK_FALSE(result.output.has_value());
    CHECK(result.cost == 0);
}

TEST_CASE("the twelve per cent bonus is twelve hundredths of the maximum") {
    // A tool whose maximum makes the hundredth visible: 1561 * 12 is not a round hundred.
    const std::int32_t max = maxOf("minecraft:diamond_pickaxe");
    REQUIRE(max * 12 / 100 != max * 12 / 99);
    const StoredItemStack input = worn("minecraft:diamond_pickaxe", max - 10);
    const StoredItemStack addition = worn("minecraft:diamond_pickaxe", max - 10);

    const anvils::Result result = anvils::combine(&input, &addition);

    REQUIRE(result.output.has_value());
    CHECK(itemWear::damage(*result.output) == std::max(max - (10 + 10 + (max * 12 / 100)), 0));
}

TEST_CASE("two nearly whole tools come out whole rather than better than whole") {
    const std::int32_t max = maxOf("minecraft:iron_pickaxe");
    const StoredItemStack input = worn("minecraft:iron_pickaxe", 5);
    const StoredItemStack addition = worn("minecraft:iron_pickaxe", 5);

    const anvils::Result result = anvils::combine(&input, &addition);

    REQUIRE(result.output.has_value());
    CHECK(itemWear::damage(*result.output) == 0);
    CHECK(max > 0);
}

TEST_CASE("a rename that would cost exactly forty is capped rather than refused") {
    const StoredItemStack input = repaired("minecraft:iron_pickaxe", 39);

    const anvils::Result result = anvils::combine(&input, nullptr, "Digger");

    // parity: thirty-nine of prior work plus one for the name is forty, which is the wall.
    REQUIRE(result.output.has_value());
    CHECK(result.cost == 39);
}

TEST_CASE("work alongside a rename still raises the prior work") {
    const StoredItemStack input = stack("minecraft:iron_pickaxe");
    const StoredItemStack book = stack("minecraft:enchanted_book");

    const anvils::Result result =
        anvils::combine(&input, &book, "Digger", {}, [](const StoredItemStack&, const StoredItemStack&) {
            return anvils::Merge{.components = std::nullopt, .cost = 2};
        });

    REQUIRE(result.output.has_value());
    // parity: only a rename on its own leaves the prior work alone.
    CHECK(result.cost == 3);
    CHECK(anvils::repairCost(*result.output) == anvils::increasedRepairCost(0));
    CHECK(nameOf(*result.output) == "Digger");
}
