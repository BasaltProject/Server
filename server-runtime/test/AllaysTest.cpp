#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/Allays.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <doctest.h>

using basalt::server::runtime::entity::ServerEntity;
using basalt::server::world::loot::ItemStack;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
using basalt::wire::nbt::NbtCompound;
namespace allays = basalt::server::runtime::entity::allays;

TEST_CASE("an allay's stack, cooldown and memories survive a save and a load") {
    const basalt::server::registry::EntityTypeEntry* entry =
        basalt::server::registry::entityTypes::of(Identifier::vanilla("allay"));
    REQUIRE(entry != nullptr);
    ServerEntity allay{1, Uuid{1, 1}, entry->id, entry->protocolId, 0.0, 64.0, 0.0};
    allays::AllayState& state = allays::stateOf(allay);
    state.inventory = ItemStack{.item = Identifier::vanilla("cobblestone"), .count = 12};
    state.duplicationCooldown = 5999;
    state.likedPlayer = Uuid{7, 8};
    state.likedNoteblock = allays::Pos{3, -60, 9};
    state.noteblockDimension = "minecraft:the_nether";
    state.noteblockCooldown = 321;
    NbtCompound tag;
    allays::write(allay, tag);

    ServerEntity loaded{2, Uuid{2, 2}, entry->id, entry->protocolId, 0.0, 64.0, 0.0};
    loaded.sourceNbt = tag;
    const allays::AllayState& read = allays::stateOf(loaded);
    REQUIRE(read.inventory.has_value());
    CHECK(read.inventory->count == 12);
    CHECK(read.duplicationCooldown == 5999);
    CHECK(read.likedPlayer == state.likedPlayer);
    CHECK(read.likedNoteblock == state.likedNoteblock);
    CHECK(read.noteblockDimension == "minecraft:the_nether");
    CHECK(read.noteblockCooldown == 321);
}

TEST_CASE("an allay only gathers what it holds, with the same potion, up to a full stack") {
    allays::AllayState state;
    const ItemStack stone{.item = Identifier::vanilla("cobblestone"), .count = 40};
    CHECK(allays::roomFor(state, stone) == 40);
    state.inventory = ItemStack{.item = Identifier::vanilla("cobblestone"), .count = 50};
    CHECK(allays::roomFor(state, stone) == 14);
    CHECK(allays::roomFor(state, ItemStack{.item = Identifier::vanilla("dirt"), .count = 1}) == 0);

    ItemStack water{.item = Identifier::vanilla("potion"), .count = 1};
    water.components.putString("minecraft:potion_contents", "minecraft:water");
    ItemStack awkward = water;
    awkward.components.putString("minecraft:potion_contents", "minecraft:awkward");
    CHECK(allays::considersEqual(water, water));
    CHECK_FALSE(allays::considersEqual(water, awkward));
}
