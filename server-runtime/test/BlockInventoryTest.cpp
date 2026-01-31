#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/BlockInventory.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>

using namespace basalt::server::runtime::world;
using basalt::server::world::loot::ItemStack;
namespace nbt = basalt::wire::nbt;

namespace {

[[nodiscard]] basalt::wire::common::Identifier item(const std::string& path) {
    return basalt::wire::common::Identifier::vanilla(path);
}

[[nodiscard]] nbt::NbtCompound entryFor(std::int8_t slot, const std::string& id, std::int32_t count) {
    nbt::NbtCompound entry;
    entry.putByte("Slot", slot);
    entry.putString("id", "minecraft:" + id);
    entry.putInt("count", count);
    return entry;
}

} // namespace

TEST_CASE("slots go out and come back the way vanilla writes them") {
    blockInventory::Slots slots(9);
    slots[0] = ItemStack{.item = item("stone"), .count = 64};
    slots[4] = ItemStack{.item = item("oak_planks"), .count = 1};
    nbt::NbtCompound data;

    blockInventory::write(data, slots);
    const blockInventory::Slots read = blockInventory::read(data, 9);

    REQUIRE(read.size() == 9);
    REQUIRE(read[0].has_value());
    CHECK(read[0]->item == item("stone"));
    CHECK(read[0]->count == 64);
    REQUIRE(read[4].has_value());
    CHECK(read[4]->item == item("oak_planks"));
    CHECK(read[4]->count == 1);
    for (const std::size_t empty : {1U, 2U, 3U, 5U, 6U, 7U, 8U}) {
        CHECK_FALSE(read[empty].has_value());
    }
}

TEST_CASE("only the slots with something in them are written down") {
    blockInventory::Slots slots(27);
    slots[26] = ItemStack{.item = item("diamond"), .count = 2};
    nbt::NbtCompound data;

    blockInventory::write(data, slots);

    const nbt::NbtList* items = data.getList("Items");
    REQUIRE(items != nullptr);
    CHECK(items->size() == 1);
    const nbt::NbtCompound* entry = (*items)[0].get<nbt::NbtCompound>();
    REQUIRE(entry != nullptr);
    CHECK(entry->getByte("Slot") == 26);
    CHECK(entry->getString("id") == "minecraft:diamond");
    CHECK(entry->getInt("count") == 2);
}

TEST_CASE("an empty container writes an empty list, not nothing") {
    nbt::NbtCompound data;
    blockInventory::write(data, blockInventory::Slots(5));

    const nbt::NbtList* items = data.getList("Items");
    REQUIRE(items != nullptr);
    CHECK(items->empty());
    CHECK(blockInventory::read(data, 5).size() == 5);
}

TEST_CASE("a compound with no list of items reads as empty slots") {
    nbt::NbtCompound data;
    data.putInt("something_else", 3);

    const blockInventory::Slots slots = blockInventory::read(data, 3);

    REQUIRE(slots.size() == 3);
    for (const std::optional<ItemStack>& slot : slots) {
        CHECK_FALSE(slot.has_value());
    }
}

TEST_CASE("a slot outside the container is left where it was") {
    nbt::NbtCompound data;
    nbt::NbtList items{nbt::NbtType::Compound};
    items.push(nbt::NbtTag::ofCompound(entryFor(0, "stone", 1)));
    items.push(nbt::NbtTag::ofCompound(entryFor(9, "diamond", 1)));
    items.push(nbt::NbtTag::ofCompound(entryFor(-1, "gold_ingot", 1)));
    data.set("Items", nbt::NbtTag::ofList(std::move(items)));

    const blockInventory::Slots slots = blockInventory::read(data, 9);

    REQUIRE(slots.size() == 9);
    CHECK(slots[0].has_value());
    // The two out-of-range entries are skipped rather than throwing or landing somewhere else.
    std::int32_t filled = 0;
    for (const std::optional<ItemStack>& slot : slots) {
        filled += slot.has_value() ? 1 : 0;
    }
    CHECK(filled == 1);
}

TEST_CASE("an entry with no slot number belongs nowhere") {
    nbt::NbtCompound data;
    nbt::NbtList items{nbt::NbtType::Compound};
    nbt::NbtCompound entry;
    entry.putString("id", "minecraft:stone");
    entry.putInt("count", 1);
    items.push(nbt::NbtTag::ofCompound(std::move(entry)));
    data.set("Items", nbt::NbtTag::ofList(std::move(items)));

    const blockInventory::Slots slots = blockInventory::read(data, 4);

    REQUIRE(slots.size() == 4);
    for (const std::optional<ItemStack>& slot : slots) {
        CHECK_FALSE(slot.has_value());
    }
}

TEST_CASE("a container of no slots holds nothing at all") {
    nbt::NbtCompound data;
    nbt::NbtList items{nbt::NbtType::Compound};
    items.push(nbt::NbtTag::ofCompound(entryFor(0, "stone", 1)));
    data.set("Items", nbt::NbtTag::ofList(std::move(items)));

    CHECK(blockInventory::read(data, 0).empty());
}

TEST_CASE("an entry with no count is one item, and one with no name is nothing") {
    nbt::NbtCompound data;
    nbt::NbtList items{nbt::NbtType::Compound};
    nbt::NbtCompound plain;
    plain.putByte("Slot", 0);
    plain.putString("id", "minecraft:stick");
    items.push(nbt::NbtTag::ofCompound(std::move(plain)));
    nbt::NbtCompound nameless;
    nameless.putByte("Slot", 1);
    nameless.putInt("count", 5);
    items.push(nbt::NbtTag::ofCompound(std::move(nameless)));
    data.set("Items", nbt::NbtTag::ofList(std::move(items)));

    const blockInventory::Slots slots = blockInventory::read(data, 2);

    REQUIRE(slots[0].has_value());
    CHECK(slots[0]->count == 1);
    CHECK_FALSE(slots[1].has_value());
}

TEST_CASE("what an item carries travels with it") {
    // parity: `ItemStack.CODEC` is `{id, count, components}`. Without the third an enchanted book in
    // a chest came back a plain one, and a written book on a lectern lost its pages.
    nbt::NbtCompound enchantments;
    enchantments.putInt("minecraft:sharpness", 3);
    nbt::NbtCompound components;
    components.set("minecraft:enchantments", nbt::NbtTag::ofCompound(std::move(enchantments)));

    blockInventory::Slots slots(3);
    slots[1] = ItemStack{.item = item("diamond_sword"), .count = 1, .components = components};
    nbt::NbtCompound data;
    blockInventory::write(data, slots);

    const blockInventory::Slots back = blockInventory::read(data, 3);
    REQUIRE(back[1].has_value());
    const nbt::NbtCompound* kept = back[1]->components.getCompound("minecraft:enchantments");
    REQUIRE(kept != nullptr);
    CHECK(kept->getInt("minecraft:sharpness") == 3);

    // An item that carries nothing writes no field at all, which is what vanilla does.
    blockInventory::Slots plain(1);
    plain[0] = ItemStack{.item = item("stone"), .count = 1};
    nbt::NbtCompound bare;
    blockInventory::write(bare, plain);
    const nbt::NbtList* items = bare.getList("Items");
    REQUIRE(items != nullptr);
    REQUIRE(items->size() == 1);
    CHECK((*items)[0].get<nbt::NbtCompound>()->find("components") == nullptr);
}

TEST_CASE("a pot keeps its one stack under vanilla's own key") {
    // parity: `DecoratedPotBlockEntity` is a `ContainerSingleItem`: one stack under `item`, and no
    // list at all. Everything else keeps the list.
    blockInventory::Slots one(1);
    one[0] = ItemStack{.item = item("emerald"), .count = 3};
    nbt::NbtCompound pot;
    blockInventory::writeFor("decorated_pot", pot, one);
    CHECK(pot.find("Items") == nullptr);
    REQUIRE(pot.find("item") != nullptr);

    const blockInventory::Slots back = blockInventory::readFor("decorated_pot", pot, 1);
    REQUIRE(back[0].has_value());
    CHECK(back[0]->count == 3);

    // Emptying it takes the key away rather than leaving an empty one.
    blockInventory::writeFor("decorated_pot", pot, blockInventory::Slots(1));
    CHECK(pot.find("item") == nullptr);

    // And a chest still writes a list.
    nbt::NbtCompound chest;
    blockInventory::writeFor("chest", chest, one);
    CHECK(chest.find("Items") != nullptr);
}
