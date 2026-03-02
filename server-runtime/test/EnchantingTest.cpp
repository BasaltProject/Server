#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/Enchanting.hpp>

#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

using namespace basalt::server::runtime::world;
using basalt::server::storage::StoredItemStack;
using basalt::wire::common::Identifier;

namespace {

struct FakeJar {
    std::map<std::string, std::string> files;

    [[nodiscard]] RealEnchantments::Jar handle() {
        return RealEnchantments::Jar{.readText = [this](std::string_view name) -> std::optional<std::string> {
                                         const auto found = files.find(std::string{name});
                                         return found == files.end() ? std::nullopt : std::optional{found->second};
                                     },
                                     .entriesUnder =
                                         [this](std::string_view prefix) {
                                             std::vector<std::string> names;
                                             for (const auto& [name, text] : files) {
                                                 if (name.starts_with(prefix)) {
                                                     names.push_back(name);
                                                 }
                                             }
                                             return names;
                                         }};
    }
};

[[nodiscard]] std::string definition(std::int32_t weight, std::int32_t maxLevel, std::int32_t minBase,
                                     std::int32_t maxBase) {
    return R"({"anvil_cost":1,"weight":)" + std::to_string(weight) + R"(,"max_level":)" + std::to_string(maxLevel) +
           R"(,"min_cost":{"base":)" + std::to_string(minBase) + R"(,"per_level_above_first":10},)" +
           R"("max_cost":{"base":)" + std::to_string(maxBase) + R"(,"per_level_above_first":10},)" +
           R"("primary_items":"#minecraft:enchantable/sharp_weapon","supported_items":"#minecraft:enchantable/sharp_weapon"})";
}

[[nodiscard]] FakeJar swordWorld() {
    FakeJar jar;
    jar.files["data/minecraft/enchantment/sharpness.json"] = definition(10, 5, 1, 200);
    jar.files["data/minecraft/enchantment/unbreaking.json"] = definition(5, 3, 1, 200);
    return jar;
}

[[nodiscard]] realItemTags::TagMembers swordItems() {
    return {{Identifier::vanilla("enchantable/sharp_weapon"), {Identifier::vanilla("iron_sword")}}};
}

[[nodiscard]] realEnchantmentTags::TagMembers tableSet() {
    return {{Identifier::vanilla("in_enchanting_table"),
             {Identifier::vanilla("sharpness"), Identifier::vanilla("unbreaking")}}};
}

} // namespace

TEST_CASE("the ring a table counts its shelves in is sixteen squares two out") {
    const std::span<const std::array<std::int32_t, 3>> offsets = enchanting::bookshelfOffsets();

    // parity: `BOOKSHELF_OFFSETS`, five by five at two heights minus the three by three inside it.
    CHECK(offsets.size() == 32);
    for (const std::array<std::int32_t, 3>& offset : offsets) {
        CHECK((std::abs(offset[0]) == 2 || std::abs(offset[2]) == 2));
        CHECK((offset[1] == 0 || offset[1] == 1));
    }
}

TEST_CASE("the three rows are drawn off one seed and come back the same") {
    basalt::server::random::RandomSource first = basalt::server::random::RandomSource::legacy(12345);
    basalt::server::random::RandomSource again = basalt::server::random::RandomSource::legacy(12345);

    for (std::int32_t slot = 0; slot < 3; ++slot) {
        CHECK(enchanting::costFor(first, slot, 15) == enchanting::costFor(again, slot, 15));
    }
}

TEST_CASE("more shelves never make a cheaper top row") {
    // parity: the third row is at least twice the shelves, so a full ring never offers under thirty.
    for (std::int32_t seed = 0; seed < 32; ++seed) {
        basalt::server::random::RandomSource random = basalt::server::random::RandomSource::legacy(seed);
        static_cast<void>(enchanting::costFor(random, 0, 15));
        static_cast<void>(enchanting::costFor(random, 1, 15));
        CHECK(enchanting::costFor(random, 2, 15) >= 30);
    }
}

TEST_CASE("a table with no shelves still offers the first row a level") {
    for (std::int32_t seed = 0; seed < 32; ++seed) {
        basalt::server::random::RandomSource random = basalt::server::random::RandomSource::legacy(seed);
        CHECK(enchanting::costFor(random, 0, 0) >= 1);
    }
}

TEST_CASE("what it puts on comes out of the tag and fits the item") {
    FakeJar jar = swordWorld();
    const RealEnchantments enchantments{jar.handle()};
    const realItemTags::TagMembers items = swordItems();
    const realEnchantmentTags::TagMembers sets = tableSet();
    const EnchantmentBooks books{.enchantments = &enchantments, .itemTags = &items, .enchantmentTags = &sets};
    basalt::server::random::RandomSource random = basalt::server::random::RandomSource::legacy(9);

    const std::vector<enchanting::Offer> offers =
        enchanting::select(random, books, StoredItemStack{.id = "minecraft:iron_sword", .count = 1}, 30);

    REQUIRE_FALSE(offers.empty());
    for (const enchanting::Offer& offer : offers) {
        CHECK(sets.at(Identifier::vanilla("in_enchanting_table")).contains(offer.enchantment));
        CHECK(offer.level >= 1);
        CHECK(offer.level <= books.maxLevel(offer.enchantment));
    }
}

TEST_CASE("an item nothing goes on is offered nothing") {
    FakeJar jar = swordWorld();
    const RealEnchantments enchantments{jar.handle()};
    const realItemTags::TagMembers items = swordItems();
    const realEnchantmentTags::TagMembers sets = tableSet();
    const EnchantmentBooks books{.enchantments = &enchantments, .itemTags = &items, .enchantmentTags = &sets};
    basalt::server::random::RandomSource random = basalt::server::random::RandomSource::legacy(9);

    // A stone is not enchantable at all, so the draw never happens.
    CHECK(enchanting::select(random, books, StoredItemStack{.id = "minecraft:stone", .count = 1}, 30).empty());
}

TEST_CASE("the same seed offers the same thing twice") {
    FakeJar jar = swordWorld();
    const RealEnchantments enchantments{jar.handle()};
    const realItemTags::TagMembers items = swordItems();
    const realEnchantmentTags::TagMembers sets = tableSet();
    const EnchantmentBooks books{.enchantments = &enchantments, .itemTags = &items, .enchantmentTags = &sets};
    const StoredItemStack sword{.id = "minecraft:iron_sword", .count = 1};

    basalt::server::random::RandomSource first = basalt::server::random::RandomSource::legacy(4242);
    basalt::server::random::RandomSource again = basalt::server::random::RandomSource::legacy(4242);
    const std::vector<enchanting::Offer> one = enchanting::select(first, books, sword, 25);
    const std::vector<enchanting::Offer> two = enchanting::select(again, books, sword, 25);

    REQUIRE(one.size() == two.size());
    for (std::size_t at = 0; at < one.size(); ++at) {
        CHECK(one[at].enchantment == two[at].enchantment);
        CHECK(one[at].level == two[at].level);
    }
}
