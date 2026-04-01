#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/BlockPickData.hpp>
#include <basalt/server/world/ChunkTypes.hpp>

#include <doctest.h>

using basalt::server::world::ChunkBlockEntity;
using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtCompound;
using basalt::wire::nbt::NbtList;
using basalt::wire::nbt::NbtTag;
using basalt::wire::nbt::NbtType;
namespace blockPickData = basalt::server::runtime::world::blockPickData;

TEST_CASE("a chest's items, name and table become components and nothing else is left") {
    NbtCompound stone;
    stone.putString("id", "minecraft:stone");
    stone.putInt("count", 3);
    stone.putByte("Slot", 5);
    NbtList items{NbtType::Compound};
    items.push(NbtTag::ofCompound(stone));
    NbtCompound data;
    data.set("Items", NbtTag::ofList(std::move(items)));
    data.putString("CustomName", "Loot");
    data.putString("LootTable", "minecraft:chests/simple_dungeon");
    const ChunkBlockEntity chest{0, 64, 0, Identifier::vanilla("chest"), data};

    NbtCompound components;
    blockPickData::addBlockData(chest, components);

    // parity: `ItemContainerContents`, `{slot, item}` with the slot the stack sat in.
    const NbtList* container = components.getList("minecraft:container");
    REQUIRE(container != nullptr);
    const NbtCompound* first = container->begin()->get<NbtCompound>();
    REQUIRE(first != nullptr);
    CHECK(first->getInt("slot") == 5);
    CHECK(first->getCompound("item")->getString("id") == "minecraft:stone");
    CHECK(first->getCompound("item")->find("Slot") == nullptr);
    CHECK(components.getString("minecraft:custom_name") == "Loot");
    CHECK(components.getCompound("minecraft:container_loot")->getString("loot_table") ==
          "minecraft:chests/simple_dungeon");
    CHECK(components.find("minecraft:block_entity_data") == nullptr);
}

TEST_CASE("what no component carries stays behind as block entity data with its type") {
    NbtCompound data;
    data.putBoolean("allow_op_features", true);
    data.putBoolean("is_waxed", true);
    const ChunkBlockEntity sign{0, 64, 0, Identifier::vanilla("sign"), data};

    NbtCompound components;
    blockPickData::addBlockData(sign, components);

    CHECK(components.find("minecraft:waxed") != nullptr);
    const NbtCompound* rest = components.getCompound("minecraft:block_entity_data");
    REQUIRE(rest != nullptr);
    CHECK(rest->getString("id") == "minecraft:sign");
    CHECK(rest->getBoolean("allow_op_features"));
    CHECK(rest->find("is_waxed") == nullptr);
}
