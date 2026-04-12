#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/StructureBlocks.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <doctest.h>

using basalt::server::runtime::world::WorldState;
using basalt::server::world::Chunk;
using basalt::server::world::ChunkBlockEntity;
using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtCompound;
namespace blocks = basalt::server::runtime::world::structureBlocks;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t state(const char* name) { return registry::blocks::named(name)->stateId; }

} // namespace

TEST_CASE("a saved structure lists full blocks, then the rest, then block entities, and skips structure void") {
    WorldState world{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
    Chunk& chunk = world.getOrGenerateChunk(0, 0);
    // One row along x: a chest, a torch, stone and structure void, in that order.
    REQUIRE(world.setBlockState(1, 70, 1, state("chest")));
    chunk.setBlockEntity(ChunkBlockEntity{1, 70, 1, Identifier::vanilla("chest"), NbtCompound{}});
    REQUIRE(world.setBlockState(2, 70, 1, state("torch")));
    REQUIRE(world.setBlockState(3, 70, 1, state("stone")));
    REQUIRE(world.setBlockState(4, 70, 1, state("structure_void")));
    const NbtCompound saved = blocks::capture(world, {1, 70, 1}, {4, 1, 1}, false);
    const auto* list = saved.getList("blocks");
    REQUIRE(list != nullptr);
    REQUIRE(list->size() == 3);
    const auto entry = [&](std::size_t index) { return (*list)[index].get<NbtCompound>(); };
    const auto xOf = [&](std::size_t index) {
        const std::int32_t* x = (*entry(index)->getList("pos"))[0].get<std::int32_t>();
        return x == nullptr ? -1 : *x;
    };
    CHECK(xOf(0) == 2);
    CHECK(xOf(1) == 1);
    CHECK(xOf(2) == 0);
    CHECK(entry(2)->getCompound("nbt")->getString("id") == "minecraft:chest");
    // parity: the palette numbers states by first appearance in that order.
    CHECK(entry(0)->getInt("state") == 0);
    CHECK(saved.getList("palette")->size() == 3);
}

TEST_CASE("a structure block's settings read back clamped as the jar clamps them") {
    NbtCompound data;
    data.putInt("posX", 90);
    data.putInt("sizeY", -4);
    data.putString("mode", "CORNER");
    data.putString("rotation", "CLOCKWISE_180");
    const blocks::Settings settings = blocks::read(data);
    CHECK(settings.offset[0] == 48);
    CHECK(settings.offset[1] == 1);
    CHECK(settings.size[1] == 0);
    CHECK(settings.mode == blocks::Mode::Corner);
    CHECK(settings.rotation == 2);
    CHECK(settings.ignoreEntities);
    CHECK(settings.showBoundingBox);
}
