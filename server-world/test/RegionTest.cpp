#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/region/ChunkNbt.hpp>

#include <doctest.h>

#include <cstdint>

using namespace basalt::server::world;
using namespace basalt::server::world::region;
using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtCompound;
using basalt::wire::nbt::NbtList;
using basalt::wire::nbt::NbtTag;
using basalt::wire::nbt::NbtType;
namespace registry = basalt::server::registry;

TEST_CASE("a chunk survives a round trip through its own NBT") {
    Chunk chunk{3, -5};
    chunk.setBlockStateId(1, 64, 2, registry::blocks::stone().stateId);
    chunk.setBlockStateId(1, 65, 2, registry::blocks::water().stateId);
    const std::optional<std::int32_t> slab =
        registry::blocks::stateIdOf(Identifier::vanilla("stone_slab"), {{"type", "top"}, {"waterlogged", "true"}});
    REQUIRE(slab.has_value());
    chunk.setBlockStateId(3, 70, 4, *slab);
    chunk.setBiomeId(1, 5, 2, 7);
    chunk.restoreInhabitedTime(1234);

    const NbtCompound tag = chunkNbt::write(chunk);
    CHECK(tag.getInt("DataVersion") == chunkNbt::kDataVersion);
    CHECK(tag.getInt("xPos") == 3);
    CHECK(tag.getInt("zPos") == -5);

    const auto back = chunkNbt::read(tag);
    REQUIRE(back.has_value());
    CHECK(back->chunkX() == 3);
    CHECK(back->chunkZ() == -5);
    CHECK(back->inhabitedTime() == 1234);
    CHECK(back->blockStateId(1, 64, 2) == registry::blocks::stone().stateId);
    CHECK(back->blockStateId(1, 65, 2) == registry::blocks::water().stateId);
    CHECK(back->blockStateId(3, 70, 4) == *slab);
    CHECK(back->blockStateId(0, 0, 0) == registry::blocks::air().stateId);
    CHECK(back->biomeId(1, 5, 2) == 7);
    CHECK_FALSE(back->isDirty());
    CHECK(back->heightAt(1, 2) == 66);
}

TEST_CASE("block entities and ticks keep their places") {
    Chunk chunk{1, 1};
    NbtCompound data;
    data.putString("CustomName", "a chest");
    chunk.setBlockEntity(ChunkBlockEntity{4, 70, 6, Identifier::vanilla("chest"), data});
    chunk.scheduleBlockTick({Identifier::vanilla("water"), 2, 65, 3, 4, 1});
    chunk.scheduleFluidTick({Identifier::vanilla("lava"), 7, 66, 8, 9, -2});

    const auto back = chunkNbt::read(chunkNbt::write(chunk));
    REQUIRE(back.has_value());
    const ChunkBlockEntity* entity = back->blockEntityAt(4, 70, 6);
    REQUIRE(entity != nullptr);
    CHECK(entity->type() == Identifier::vanilla("chest"));
    CHECK(entity->data().getString("CustomName") == "a chest");
    CHECK_FALSE(entity->data().contains("x"));
    CHECK_FALSE(entity->data().contains("id"));

    REQUIRE(back->scheduledBlockTicks().size() == 1);
    CHECK(back->scheduledBlockTicks().front().delay == 4);
    CHECK(back->scheduledBlockTicks().front().priority == 1);
    REQUIRE(back->scheduledFluidTicks().size() == 1);
    CHECK(back->scheduledFluidTicks().front().type == Identifier::vanilla("lava"));
    CHECK(back->scheduledFluidTicks().front().priority == -2);
}

TEST_CASE("entities and structures ride with the chunk") {
    Chunk chunk{0, 0};
    ChunkGeneratedEntity cow;
    cow.type = Identifier::vanilla("cow");
    cow.x = 8.5;
    cow.y = 64.0;
    cow.z = 9.5;
    cow.yaw = 90.0F;
    ChunkGeneratedEntity rider;
    rider.type = Identifier::vanilla("chicken");
    cow.passengers.push_back(rider);
    chunk.addGeneratedEntity(cow);

    NbtCompound saved;
    saved.putString("id", "minecraft:pig");
    chunk.setSavedEntities({saved});

    chunk.addStructureStart(
        *structure::StructureStartRecord::of("fortress", 0, 0, {{0, 60, 0, 15, 70, 15}, {16, 60, 0, 31, 70, 15}}));

    const auto back = chunkNbt::read(chunkNbt::write(chunk));
    REQUIRE(back.has_value());
    REQUIRE(back->generatedEntities().size() == 1);
    const ChunkGeneratedEntity& decoded = back->generatedEntities().front();
    CHECK(decoded.type == Identifier::vanilla("cow"));
    CHECK(decoded.x == 8.5);
    CHECK(decoded.yaw == 90.0F);
    REQUIRE(decoded.passengers.size() == 1);
    CHECK(decoded.passengers.front().type == Identifier::vanilla("chicken"));

    REQUIRE(back->savedEntities().size() == 1);
    CHECK(back->savedEntities().front().getString("id") == "minecraft:pig");

    CHECK(back->structures().has("fortress"));
    REQUIRE(back->structures().starts().size() == 1);
    CHECK(back->structures().starts().front().pieces.size() == 2);
    CHECK(back->structures().starts().front().fullBounds.maxX == 31);
}

TEST_CASE("a chunk imported from vanilla keeps its structures") {
    // parity: vanilla keys `starts` by structure id and writes no `BB` on the start.
    NbtCompound start;
    start.putString("id", "minecraft:mineshaft");
    start.putInt("ChunkX", 0);
    start.putInt("ChunkZ", 0);
    start.putInt("references", 0);
    NbtCompound first;
    first.putIntArray("BB", {0, 20, 0, 15, 30, 15});
    NbtCompound second;
    second.putIntArray("BB", {16, 25, 8, 31, 40, 31});
    start.set("Children",
              NbtTag::ofList(NbtList{NbtType::Compound,
                                     {NbtTag::ofCompound(std::move(first)), NbtTag::ofCompound(std::move(second))}}));
    NbtCompound byName;
    byName.set("minecraft:mineshaft", NbtTag::ofCompound(std::move(start)));
    NbtCompound structures;
    structures.set("starts", NbtTag::ofCompound(std::move(byName)));

    NbtCompound tag = chunkNbt::write(Chunk{0, 0});
    tag.set("structures", NbtTag::ofCompound(std::move(structures)));

    const auto back = chunkNbt::read(tag);
    REQUIRE(back.has_value());
    CHECK(back->structures().has("mineshaft"));
    REQUIRE(back->structures().starts().size() == 1);
    CHECK(back->structures().starts().front().pieces.size() == 2);
    const structure::StructureBoundingBox& bounds = back->structures().starts().front().fullBounds;
    CHECK(bounds.minX == 0);
    CHECK(bounds.minY == 20);
    CHECK(bounds.maxX == 31);
    CHECK(bounds.maxY == 40);
    CHECK(bounds.maxZ == 31);
}

TEST_CASE("nonsense is refused with a reason rather than half-read") {
    NbtCompound empty;
    const auto missing = chunkNbt::read(empty);
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error().message == "missing sections list");

    Chunk chunk{0, 0};
    NbtCompound tag = chunkNbt::write(chunk);
    const auto* sections = tag.getList("sections");
    REQUIRE(sections != nullptr);
    NbtCompound broken = *(*sections)[0].get<NbtCompound>();
    NbtCompound states;
    basalt::wire::nbt::NbtList palette{basalt::wire::nbt::NbtType::Compound, {}};
    NbtCompound entry;
    entry.putString("Name", "minecraft:not_a_block");
    palette.push(basalt::wire::nbt::NbtTag::ofCompound(std::move(entry)));
    states.set("palette", basalt::wire::nbt::NbtTag::ofList(std::move(palette)));
    broken.set("block_states", basalt::wire::nbt::NbtTag::ofCompound(std::move(states)));
    basalt::wire::nbt::NbtList replaced{basalt::wire::nbt::NbtType::Compound, {}};
    replaced.push(basalt::wire::nbt::NbtTag::ofCompound(std::move(broken)));
    tag.set("sections", basalt::wire::nbt::NbtTag::ofList(std::move(replaced)));

    const auto refused = chunkNbt::read(tag);
    REQUIRE_FALSE(refused.has_value());
    CHECK(refused.error().message.find("not_a_block") != std::string::npos);
}

TEST_CASE("a palette entry reads in every spelling vanilla has written it") {
    // parity: BlockState.CODEC bare id, {id, properties}, {"": id} in mixed lists; 26.2 Name/Properties.
    const auto sectionWith = [](NbtList palette) {
        Chunk chunk{0, 0};
        NbtCompound tag = chunkNbt::write(chunk);
        NbtCompound section = *(*tag.getList("sections"))[0].get<NbtCompound>();
        NbtCompound states;
        states.set("palette", NbtTag::ofList(std::move(palette)));
        section.set("block_states", NbtTag::ofCompound(std::move(states)));
        NbtList sections{NbtType::Compound, {}};
        sections.push(NbtTag::ofCompound(std::move(section)));
        tag.set("sections", NbtTag::ofList(std::move(sections)));
        return tag;
    };
    const auto blockAt = [](const NbtCompound& tag) {
        const auto read = chunkNbt::read(tag);
        REQUIRE(read.has_value());
        return read->blockStateId(0, read->verticalBounds().minY(), 0);
    };

    NbtList bare{NbtType::String, {}};
    bare.push(NbtTag::ofString("minecraft:stone"));
    CHECK(blockAt(sectionWith(std::move(bare))) == basalt::server::registry::blocks::named("stone")->stateId);

    NbtCompound wrappedEntry;
    wrappedEntry.putString("", "minecraft:dirt");
    NbtList wrapped{NbtType::Compound, {}};
    wrapped.push(NbtTag::ofCompound(std::move(wrappedEntry)));
    CHECK(blockAt(sectionWith(std::move(wrapped))) == basalt::server::registry::blocks::named("dirt")->stateId);

    for (const auto& [idKey, propertiesKey] : {std::pair{"id", "properties"}, std::pair{"Name", "Properties"}}) {
        NbtCompound properties;
        properties.putString("axis", "x");
        NbtCompound entry;
        entry.putString(idKey, "minecraft:oak_log");
        entry.set(propertiesKey, NbtTag::ofCompound(std::move(properties)));
        NbtList full{NbtType::Compound, {}};
        full.push(NbtTag::ofCompound(std::move(entry)));
        CHECK(blockAt(sectionWith(std::move(full))) != basalt::server::registry::blocks::named("oak_log")->stateId);
    }
}
