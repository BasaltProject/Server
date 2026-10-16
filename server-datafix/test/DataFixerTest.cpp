#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/datafix/DataFixer.hpp>

#include <doctest.h>

using namespace basalt::wire::nbt;
using basalt::server::datafix::Domain;

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

    basalt::server::datafix::upgrade(Domain::Chunk, chunk, 4903);

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

TEST_CASE("a 1.21.4 chunk loses its light and gains iron chains and tooltip display") {
    NbtCompound levels;
    levels.putInt("minecraft:sharpness", 5);
    NbtCompound enchantments;
    enchantments.set("levels", NbtTag::ofCompound(levels));
    enchantments.putBoolean("show_in_tooltip", false);
    NbtCompound components;
    components.set("minecraft:enchantments", NbtTag::ofCompound(enchantments));
    NbtCompound sword = stack("minecraft:chain", 1);
    sword.set("components", NbtTag::ofCompound(components));
    NbtCompound chest;
    chest.putString("id", "minecraft:chest");
    NbtList items{NbtType::Compound, {}};
    items.push(NbtTag::ofCompound(sword));
    chest.set("Items", NbtTag::ofList(items));
    NbtList blockEntities{NbtType::Compound, {}};
    blockEntities.push(NbtTag::ofCompound(chest));

    NbtCompound entry;
    entry.putString("Name", "minecraft:chain");
    NbtList palette{NbtType::Compound, {}};
    palette.push(NbtTag::ofCompound(entry));
    NbtCompound states;
    states.set("palette", NbtTag::ofList(palette));
    NbtCompound section;
    section.set("block_states", NbtTag::ofCompound(states));
    section.putByteArray("SkyLight", {1, 2});
    NbtList sections{NbtType::Compound, {}};
    sections.push(NbtTag::ofCompound(section));

    NbtCompound chunk;
    chunk.putBoolean("isLightOn", true);
    chunk.set("sections", NbtTag::ofList(sections));
    chunk.set("block_entities", NbtTag::ofList(blockEntities));
    basalt::server::datafix::upgrade(Domain::Chunk, chunk, 4189);

    CHECK_FALSE(chunk.contains("isLightOn"));
    const NbtCompound* fixedSection = chunk.getList("sections")->elements()[0].get<NbtCompound>();
    CHECK_FALSE(fixedSection->contains("SkyLight"));
    const NbtCompound* fixedEntry =
        fixedSection->getCompound("block_states")->getList("palette")->elements()[0].get<NbtCompound>();
    CHECK(fixedEntry->getString("id") == "minecraft:iron_chain");
    const NbtCompound* fixedStack = chunk.getList("block_entities")
                                        ->elements()[0]
                                        .get<NbtCompound>()
                                        ->getList("Items")
                                        ->elements()[0]
                                        .get<NbtCompound>();
    CHECK(fixedStack->getString("id") == "minecraft:iron_chain");
    const NbtCompound* fixedComponents = fixedStack->getCompound("components");
    CHECK(fixedComponents->getCompound("minecraft:enchantments")->getInt("minecraft:sharpness") == 5);
    const NbtList* hidden = fixedComponents->getCompound("minecraft:tooltip_display")->getList("hidden_components");
    REQUIRE(hidden->size() == 1);
    CHECK(*hidden->elements()[0].get<std::string>() == "minecraft:enchantments");
}

TEST_CASE("sign text saved as JSON becomes components with the new events") {
    NbtList messages{NbtType::String, {}};
    messages.push(
        NbtTag::ofString(R"({"text":"hi","bold":"true","clickEvent":{"action":"run_command","value":"/say hi"},)"
                         R"("hoverEvent":{"action":"show_text","contents":{"text":"tip"}}})"));
    messages.push(NbtTag::ofString(R"("plain")"));
    NbtCompound front;
    front.set("messages", NbtTag::ofList(messages));
    NbtCompound sign;
    sign.putString("id", "minecraft:sign");
    sign.set("front_text", NbtTag::ofCompound(front));
    NbtList blockEntities{NbtType::Compound, {}};
    blockEntities.push(NbtTag::ofCompound(sign));
    NbtCompound chunk;
    chunk.set("block_entities", NbtTag::ofList(blockEntities));

    basalt::server::datafix::upgrade(Domain::Chunk, chunk, 4189);

    const NbtCompound* fixed =
        chunk.getList("block_entities")->elements()[0].get<NbtCompound>()->getCompound("front_text");
    const NbtCompound* first = fixed->getList("messages")->elements()[0].get<NbtCompound>();
    REQUIRE(first != nullptr);
    CHECK(first->getString("text") == "hi");
    CHECK(first->getBoolean("bold"));
    CHECK(first->getCompound("click_event")->getString("command") == "/say hi");
    CHECK_FALSE(first->contains("clickEvent"));
    CHECK(first->getCompound("hover_event")->getCompound("value")->getString("text") == "tip");
    CHECK(*fixed->getList("messages")->elements()[1].get<std::string>() == "plain");
}
