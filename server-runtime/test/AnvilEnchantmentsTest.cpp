#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/item/ItemComponents.hpp>
#include <basalt/server/runtime/world/AnvilEnchantments.hpp>
#include <basalt/server/runtime/world/EnchantmentBooks.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

using namespace basalt::server::runtime::world;
namespace itemComponents = basalt::server::runtime::item::itemComponents;
using basalt::server::storage::StoredItemStack;
using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtCompound;

namespace {

/// A jar that is a map of names to text, which is all either registry asks of one.
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

/// The three files an anvil reads, for a world with two clashing damage enchantments and one that
/// only goes on a bow.
[[nodiscard]] FakeJar sharpWorld() {
    FakeJar jar;
    jar.files["data/minecraft/enchantment/sharpness.json"] =
        R"({"anvil_cost":1,"max_level":5,"supported_items":"#minecraft:enchantable/sharp_weapon",)"
        R"("exclusive_set":"#minecraft:exclusive_set/damage"})";
    jar.files["data/minecraft/enchantment/smite.json"] =
        R"({"anvil_cost":2,"max_level":5,"supported_items":"#minecraft:enchantable/sharp_weapon",)"
        R"("exclusive_set":"#minecraft:exclusive_set/damage"})";
    jar.files["data/minecraft/enchantment/power.json"] =
        R"({"anvil_cost":1,"max_level":5,"supported_items":"minecraft:bow"})";
    return jar;
}

[[nodiscard]] realItemTags::TagMembers sharpItems() {
    return {{Identifier::vanilla("enchantable/sharp_weapon"),
             {Identifier::vanilla("iron_sword"), Identifier::vanilla("diamond_sword")}}};
}

[[nodiscard]] realEnchantmentTags::TagMembers damageSet() {
    return {{Identifier::vanilla("exclusive_set/damage"),
             {Identifier::vanilla("sharpness"), Identifier::vanilla("smite")}}};
}

[[nodiscard]] StoredItemStack stack(const std::string& id) { return StoredItemStack{.id = id, .count = 1}; }

[[nodiscard]] StoredItemStack enchanted(const std::string& id, const std::string& enchantment, std::int32_t level) {
    StoredItemStack made = stack(id);
    NbtCompound levels;
    levels.putInt(enchantment, level);
    made.components.set(id == "minecraft:enchanted_book" ? itemComponents::kStoredEnchantments
                                                         : itemComponents::kEnchantments,
                        basalt::wire::nbt::NbtTag::ofCompound(std::move(levels)));
    return made;
}

[[nodiscard]] std::int32_t levelOf(const NbtCompound& components, const std::string& enchantment) {
    const NbtCompound* levels = components.getCompound(itemComponents::kEnchantments);
    return levels == nullptr ? 0 : levels->getInt(enchantment);
}

} // namespace

TEST_CASE("a book puts what is written in it onto a sword") {
    FakeJar jar = sharpWorld();
    const RealEnchantments enchantments{jar.handle()};
    const realItemTags::TagMembers items = sharpItems();
    const realEnchantmentTags::TagMembers sets = damageSet();
    const anvils::MergeEnchantments merge =
        anvils::enchantmentMerge({.enchantments = &enchantments, .itemTags = &items, .enchantmentTags = &sets});

    const anvils::Merge merged =
        merge(stack("minecraft:iron_sword"), enchanted("minecraft:enchanted_book", "minecraft:sharpness", 2));

    REQUIRE(merged.components.has_value());
    CHECK(levelOf(*merged.components, "minecraft:sharpness") == 2);
    // parity: half of the anvil cost out of a book, never under one, times the level.
    CHECK(merged.cost == 2);
    CHECK_FALSE(merged.refused);
}

TEST_CASE("two of the same level make the next one up") {
    FakeJar jar = sharpWorld();
    const RealEnchantments enchantments{jar.handle()};
    const realItemTags::TagMembers items = sharpItems();
    const realEnchantmentTags::TagMembers sets = damageSet();
    const anvils::MergeEnchantments merge =
        anvils::enchantmentMerge({.enchantments = &enchantments, .itemTags = &items, .enchantmentTags = &sets});

    const anvils::Merge merged = merge(enchanted("minecraft:iron_sword", "minecraft:sharpness", 2),
                                       enchanted("minecraft:iron_sword", "minecraft:sharpness", 2));

    REQUIRE(merged.components.has_value());
    CHECK(levelOf(*merged.components, "minecraft:sharpness") == 3);
    // Not out of a book, so the full price, times the level it came to.
    CHECK(merged.cost == 3);
}

TEST_CASE("a level past the maximum is capped rather than refused") {
    FakeJar jar = sharpWorld();
    const RealEnchantments enchantments{jar.handle()};
    const realItemTags::TagMembers items = sharpItems();
    const realEnchantmentTags::TagMembers sets = damageSet();
    const anvils::MergeEnchantments merge =
        anvils::enchantmentMerge({.enchantments = &enchantments, .itemTags = &items, .enchantmentTags = &sets});

    const anvils::Merge merged = merge(enchanted("minecraft:iron_sword", "minecraft:sharpness", 5),
                                       enchanted("minecraft:iron_sword", "minecraft:sharpness", 5));

    REQUIRE(merged.components.has_value());
    CHECK(levelOf(*merged.components, "minecraft:sharpness") == 5);
}

TEST_CASE("two enchantments of the same exclusive set will not sit together") {
    FakeJar jar = sharpWorld();
    const RealEnchantments enchantments{jar.handle()};
    const realItemTags::TagMembers items = sharpItems();
    const realEnchantmentTags::TagMembers sets = damageSet();
    const anvils::MergeEnchantments merge =
        anvils::enchantmentMerge({.enchantments = &enchantments, .itemTags = &items, .enchantmentTags = &sets});

    const anvils::Merge merged = merge(enchanted("minecraft:iron_sword", "minecraft:sharpness", 1),
                                       enchanted("minecraft:enchanted_book", "minecraft:smite", 1));

    // parity: the only offer clashed, so the pair is refused rather than charged for nothing.
    CHECK(merged.refused);
    CHECK_FALSE(merged.components.has_value());
}

TEST_CASE("an enchantment that does not go on the item is refused") {
    FakeJar jar = sharpWorld();
    const RealEnchantments enchantments{jar.handle()};
    const realItemTags::TagMembers items = sharpItems();
    const realEnchantmentTags::TagMembers sets = damageSet();
    const anvils::MergeEnchantments merge =
        anvils::enchantmentMerge({.enchantments = &enchantments, .itemTags = &items, .enchantmentTags = &sets});

    const anvils::Merge merged =
        merge(stack("minecraft:iron_sword"), enchanted("minecraft:enchanted_book", "minecraft:power", 1));

    CHECK(merged.refused);

    // parity: a book takes anything, whatever its own file says the enchantment goes on.
    const anvils::Merge onto =
        merge(stack("minecraft:enchanted_book"), enchanted("minecraft:enchanted_book", "minecraft:power", 1));
    REQUIRE(onto.components.has_value());
    const NbtCompound* stored = onto.components->getCompound(itemComponents::kStoredEnchantments);
    REQUIRE(stored != nullptr);
    CHECK(stored->getInt("minecraft:power") == 1);
}

TEST_CASE("with no jar behind it an anvil merges nothing") {
    const anvils::MergeEnchantments merge = anvils::enchantmentMerge({});

    const anvils::Merge merged =
        merge(stack("minecraft:iron_sword"), enchanted("minecraft:enchanted_book", "minecraft:sharpness", 1));

    CHECK(merged.refused);
}
