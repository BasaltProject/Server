#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/world/datafix/DataFixer.hpp>

#include <doctest.h>

using namespace basalt::wire::nbt;
using basalt::server::world::datafix::Domain;

namespace {

NbtCompound stack(std::string_view id, std::int32_t count) {
    NbtCompound tag;
    tag.putString("id", std::string{id});
    tag.putInt("count", count);
    return tag;
}

} // namespace

TEST_CASE("a 26.2 chunk is brought to 26.3") {
    NbtCompound components;
    NbtCompound explorer;
    explorer.putString("type", "minecraft:monument");
    NbtCompound decorations;
    decorations.set("+", NbtTag::ofCompound(explorer));
    components.set("minecraft:map_decorations", NbtTag::ofCompound(decorations));
    NbtCompound name;
    name.putString("translate", "filled_map.monument");
    components.set("minecraft:item_name", NbtTag::ofCompound(name));
    components.putInt("minecraft:map_color", 5);
    NbtCompound map = stack("minecraft:filled_map", 1);
    map.set("components", NbtTag::ofCompound(components));
    NbtCompound chest;
    chest.putString("id", "minecraft:chest");
    NbtList items{NbtType::Compound, {}};
    items.push(NbtTag::ofCompound(map));
    chest.set("Items", NbtTag::ofList(items));
    NbtCompound sign;
    sign.putString("id", "minecraft:sign");
    NbtCompound pot;
    pot.putString("id", "minecraft:decorated_pot");
    NbtList sherds{NbtType::String, {}};
    sherds.push(NbtTag::ofString("minecraft:angler_pottery_sherd"));
    pot.set("sherds", NbtTag::ofList(sherds));
    NbtList blockEntities{NbtType::Compound, {}};
    blockEntities.push(NbtTag::ofCompound(chest));
    blockEntities.push(NbtTag::ofCompound(sign));
    blockEntities.push(NbtTag::ofCompound(pot));

    NbtCompound chunk;
    chunk.putString("Status", "minecraft:carvers");
    chunk.set("block_entities", NbtTag::ofList(blockEntities));

    basalt::server::world::datafix::upgrade(Domain::Chunk, chunk, 4903);

    CHECK(chunk.getString("Status") == "minecraft:terrain");
    const NbtCompound* fixedMap = chunk.getList("block_entities")
                                      ->elements()[0]
                                      .get<NbtCompound>()
                                      ->getList("Items")
                                      ->elements()[0]
                                      .get<NbtCompound>();
    CHECK(fixedMap->getString("id") == "minecraft:ocean_monument_map");
    const NbtCompound* fixedComponents = fixedMap->getCompound("components");
    CHECK_FALSE(fixedComponents->contains("minecraft:map_color"));
    CHECK_FALSE(fixedComponents->contains("minecraft:item_name"));
    const NbtCompound* fixedSign = chunk.getList("block_entities")->elements()[1].get<NbtCompound>();
    CHECK(fixedSign->getBoolean("allow_op_features"));
    const NbtCompound* fixedSherds =
        chunk.getList("block_entities")->elements()[2].get<NbtCompound>()->getCompound("sherds");
    REQUIRE(fixedSherds != nullptr);
    CHECK(fixedSherds->getCompound("back")->getString("id") == "minecraft:angler_pottery_sherd");
    CHECK(fixedSherds->getCompound("front")->getString("id") == "minecraft:brick");
}
