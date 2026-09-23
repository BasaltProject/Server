#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/IglooStructureGenerator.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <tuple>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain;
using basalt::wire::nbt::NbtCompound;
using basalt::wire::nbt::NbtList;
using basalt::wire::nbt::NbtTag;
using basalt::wire::nbt::NbtType;
namespace registry = basalt::server::registry;

namespace {

/// A stand-in igloo piece: snow with a chest and a structure block over it, so the marker has a
/// chest below to fill. Nothing here comes from a Mojang structure file.
[[nodiscard]] structure::StructureTemplate piece(std::int32_t sizeX, std::int32_t sizeY, std::int32_t sizeZ,
                                                 bool withChest, bool withLadder) {
    NbtList palette{NbtType::Compound};
    for (const std::string_view name : {"minecraft:snow_block", "minecraft:structure_block", "minecraft:chest",
                                        "minecraft:ladder", "minecraft:air"}) {
        NbtCompound entry;
        entry.set("Name", NbtTag::ofString(std::string{name}));
        palette.push(NbtTag::ofCompound(std::move(entry)));
    }
    NbtList blocks{NbtType::Compound};
    for (std::int32_t x = 0; x < sizeX; ++x) {
        for (std::int32_t y = 0; y < sizeY; ++y) {
            for (std::int32_t z = 0; z < sizeZ; ++z) {
                const bool chestHere = withChest && x == 1 && y == 0 && z == 1;
                const bool markerHere = withChest && x == 1 && y == 1 && z == 1;
                const bool ladderHere = withLadder && x == 1 && y == 0 && z == 1;
                // An open floor under the dome, so the snow that closes a shaftless trapdoor lands
                // on something other than more snow.
                const bool floorHere = !withChest && !withLadder && y == 0 && x >= 2 && x <= 4 && z >= 3 && z <= 6;
                NbtCompound block;
                block.set("pos", NbtTag::ofList(
                                     NbtList{NbtType::Int, {NbtTag::ofInt(x), NbtTag::ofInt(y), NbtTag::ofInt(z)}}));
                block.set("state", NbtTag::ofInt(markerHere ? 1 : chestHere ? 2 : ladderHere ? 3 : floorHere ? 4 : 0));
                if (markerHere) {
                    NbtCompound data;
                    data.set("mode", NbtTag::ofString("DATA"));
                    data.set("metadata", NbtTag::ofString("chest"));
                    block.set("nbt", NbtTag::ofCompound(std::move(data)));
                }
                blocks.push(NbtTag::ofCompound(std::move(block)));
            }
        }
    }
    NbtCompound nbt;
    nbt.set("size",
            NbtTag::ofList(NbtList{NbtType::Int, {NbtTag::ofInt(sizeX), NbtTag::ofInt(sizeY), NbtTag::ofInt(sizeZ)}}));
    nbt.set("palette", NbtTag::ofList(std::move(palette)));
    nbt.set("blocks", NbtTag::ofList(std::move(blocks)));
    structure::StructureTemplate built;
    built.load(nbt);
    return built;
}

[[nodiscard]] const structure::StructureTemplate* templateOf(std::string_view name) {
    static const structure::StructureTemplate top = piece(7, 6, 8, false, false);
    static const structure::StructureTemplate middle = piece(3, 4, 3, false, true);
    static const structure::StructureTemplate bottom = piece(7, 8, 9, true, false);
    if (name == "igloo/top") {
        return &top;
    }
    if (name == "igloo/middle") {
        return &middle;
    }
    return name == "igloo/bottom" ? &bottom : nullptr;
}

[[nodiscard]] Chunk snowChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = -16; y <= 70; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::tuple<std::int64_t, std::int32_t, std::size_t> fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t written = 0;
    for (std::int32_t y = -16; y <= 100; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                if (id != registry::blocks::stone().stateId && id != registry::blocks::air().stateId) {
                    ++written;
                }
            }
        }
    }
    for (const ChunkBlockEntity& entity : chunk.blockEntities()) {
        checksum =
            (checksum ^ static_cast<std::uint64_t>(entity.data().getLong("LootTableSeed", 0))) * 1099511628211ULL;
    }
    return {static_cast<std::int64_t>(checksum), written, chunk.blockEntities().size()};
}

struct Run {
    std::int32_t written = 0;
    std::size_t blockEntities = 0;
    std::size_t starts = 0;
    std::int64_t checksum = 0;
};

[[nodiscard]] Run run(std::int64_t seed, std::string_view biome, std::int32_t surface, std::int32_t chunks) {
    const std::string biomeName{biome};
    IglooStructureGenerator generator{[biomeName](std::int32_t, std::int32_t, std::int32_t) { return biomeName; },
                                      {"minecraft:snowy_plains", "minecraft:snowy_slopes"},
                                      templateOf,
                                      // A slope, so which column the entrance settles on is observable.
                                      [surface](std::int32_t x, std::int32_t z) {
                                          const std::int32_t rest = (x + z) % 4;
                                          return surface + (rest < 0 ? rest + 4 : rest);
                                      },
                                      [surface](std::int32_t x, std::int32_t z, HeightmapType) {
                                          const std::int32_t rest = (x + z) % 4;
                                          return surface + (rest < 0 ? rest + 4 : rest);
                                      }};
    // Centred on the candidate the grid picks, which is 32 chunks wide.
    const auto centre = IglooStructureGenerator::grid().candidateChunk(seed, 0, 0);
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t index = 0; index < chunks; ++index) {
        Chunk chunk = snowChunk(centre.first + index % 6 - 3, centre.second + index / 6 - 3);
        generator.generateInto(chunk, seed);
        const auto [chunkChecksum, written, blockEntities] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.written += written;
        result.blockEntities += blockEntities;
        result.starts += chunk.structures().starts().size();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

void check(const Run& result, std::int32_t written, std::size_t blockEntities, std::size_t starts,
           std::int64_t checksum) {
    CHECK(result.written == written);
    CHECK(result.blockEntities == blockEntities);
    CHECK(result.starts == starts);
    CHECK(result.checksum == checksum);
}

} // namespace

TEST_CASE("the igloos stand where the frozen kotlin puts them") {
    SUBCASE("snowy") { check(run(1234, "minecraft:snowy_plains", 71, 36), 324, 0, 1, 1909464990203033713LL); }

    SUBCASE("plains") { check(run(1234, "minecraft:plains", 71, 36), 0, 0, 0, 6717651259440025137LL); }

    SUBCASE("basement") {
        // A seed whose coin flip drew a basement, so the shaft, the laboratory and its chest run.
        check(run(3, "minecraft:snowy_plains", 71, 36), 1053, 1, 1, -2030513675610861548LL);
    }

    SUBCASE("shallowBasement") {
        // A shallower shaft from a different draw of the eight-deep range.
        check(run(5, "minecraft:snowy_plains", 71, 36), 945, 1, 1, -453986296067728306LL);
    }

    SUBCASE("otherSeed") { check(run(8, "minecraft:snowy_plains", 71, 36), 719, 0, 1, -1643830816775094671LL); }
}
