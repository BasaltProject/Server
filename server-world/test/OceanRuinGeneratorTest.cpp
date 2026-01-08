#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/OceanRuinStructureGenerator.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain;
using basalt::wire::nbt::NbtCompound;
using basalt::wire::nbt::NbtList;
using basalt::wire::nbt::NbtTag;
using basalt::wire::nbt::NbtType;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t floorMod(std::int32_t value, std::int32_t divisor) {
    const std::int32_t rest = value % divisor;
    return rest < 0 ? rest + divisor : rest;
}

/// Every template the generator can ask for, in the order that indexes the stand-ins.
[[nodiscard]] const std::vector<std::string>& names() {
    static const std::vector<std::string> built = [] {
        std::vector<std::string> all;
        const auto add = [&all](std::string_view prefix, std::int32_t number) {
            all.push_back("underwater_ruin/" + std::string{prefix} + "_" + std::to_string(number));
        };
        for (const std::string_view prefix : {"warm", "brick", "cracked", "mossy"}) {
            for (std::int32_t number = 1; number <= 8; ++number) {
                add(prefix, number);
            }
        }
        for (const std::string_view prefix : {"big_brick", "big_cracked", "big_mossy"}) {
            for (const std::int32_t number : {1, 2, 3, 8}) {
                add(prefix, number);
            }
        }
        for (std::int32_t number = 4; number <= 7; ++number) {
            add("big_warm", number);
        }
        return all;
    }();
    return built;
}

/// A stand-in ruin sized by which name it answers to, so the draw that picks one is observable.
/// Every block rots, which is what makes the three integrities tell each other apart.
[[nodiscard]] structure::StructureTemplate ruin(std::int32_t index) {
    // The last sixteen names are the large ruins, and a large one is wide enough to reach the third
    // ring of chunks the walk visits.
    const bool large = index >= 32;
    const std::int32_t sizeX = (large ? 14 : 5) + index % 5;
    const std::int32_t sizeY = 3 + index % 4;
    const std::int32_t sizeZ = (large ? 15 : 6) + index % 6;
    NbtList palette{NbtType::Compound};
    for (const std::string_view name : {"minecraft:stone_bricks", "minecraft:mossy_stone_bricks", "minecraft:air"}) {
        NbtCompound entry;
        entry.set("Name", NbtTag::ofString(std::string{name}));
        palette.push(NbtTag::ofCompound(std::move(entry)));
    }
    NbtList blocks{NbtType::Compound};
    for (std::int32_t x = 0; x < sizeX; ++x) {
        for (std::int32_t y = 0; y < sizeY; ++y) {
            for (std::int32_t z = 0; z < sizeZ; ++z) {
                NbtCompound block;
                block.set("pos", NbtTag::ofList(
                                     NbtList{NbtType::Int, {NbtTag::ofInt(x), NbtTag::ofInt(y), NbtTag::ofInt(z)}}));
                // A hollow shell, so the rot draw runs on the walls and the inside is air.
                const bool wall = x == 0 || z == 0 || y == 0 || x == sizeX - 1 || z == sizeZ - 1;
                block.set("state", NbtTag::ofInt(!wall ? 2 : (x + y + z) % 3 == 0 ? 1 : 0));
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
    static const std::vector<structure::StructureTemplate> built = [] {
        std::vector<structure::StructureTemplate> all;
        all.reserve(names().size());
        for (std::size_t index = 0; index < names().size(); ++index) {
            all.push_back(ruin(static_cast<std::int32_t>(index)));
        }
        return all;
    }();
    for (std::size_t index = 0; index < names().size(); ++index) {
        if (names()[index] == name) {
            return &built[index];
        }
    }
    return nullptr;
}

/// A pitted sea bed. The wide pits sink a whole footprint; the one-block strip cuts across one, so
/// how many of its columns count as hollow lands near the width it is compared against.
[[nodiscard]] bool inPit(std::int32_t x, std::int32_t z) {
    return (floorMod(x, 37) < 12 && floorMod(z, 41) < 12) || (floorMod(x, 23) < 9 && floorMod(z, 29) == 5);
}

[[nodiscard]] std::int32_t bedTop(std::int32_t x, std::int32_t z) {
    const std::int32_t floor = 45 + floorMod(x * 2 + z, 4);
    return inPit(x, z) ? floor - 7 : floor;
}

[[nodiscard]] std::int32_t block(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    REQUIRE(found != nullptr);
    return found->stateId;
}

[[nodiscard]] std::int32_t terrainAt(std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::int32_t floor = bedTop(x, z);
    if (y <= floor) {
        return registry::blocks::stone().stateId;
    }
    // A pit is capped with ice, which the walk down has to treat as open just like water.
    if (inPit(x, z) && y <= floor + 3) {
        static const std::int32_t ice = block("packed_ice");
        return ice;
    }
    static const std::int32_t water = block("water");
    return y <= 62 ? water : registry::blocks::air().stateId;
}

[[nodiscard]] Chunk seaChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 20; y <= 70; ++y) {
                chunk.setBlockStateId(x, y, z, terrainAt(chunkX * 16 + x, y, chunkZ * 16 + z));
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t written = 0;
    for (std::int32_t y = 20; y <= 70; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                if (id != terrainAt(chunk.chunkX() * 16 + x, y, chunk.chunkZ() * 16 + z)) {
                    ++written;
                }
            }
        }
    }
    return {static_cast<std::int64_t>(checksum), written};
}

struct Run {
    std::int32_t written = 0;
    std::size_t starts = 0;
    std::size_t pieces = 0;
    std::int64_t checksum = 0;
};

[[nodiscard]] Run run(std::int64_t seed, bool cold, bool warm) {
    OceanRuinStructureGenerator generator{
        // Only the centre column of a chunk answers anything but plains, so where the variant probe
        // stands is observable too.
        [cold, warm](std::int32_t x, std::int32_t y, std::int32_t z) -> std::string {
            if (floorMod(x, 16) != 8 || floorMod(z, 16) != 8 || y != bedTop(x, z)) {
                return "minecraft:plains";
            }
            if (cold) {
                return "minecraft:cold_ocean";
            }
            return warm ? "minecraft:warm_ocean" : "minecraft:plains";
        },
        {"minecraft:cold_ocean", "minecraft:frozen_ocean"},
        {"minecraft:warm_ocean", "minecraft:lukewarm_ocean"},
        templateOf,
        [](std::int32_t x, std::int32_t z) { return bedTop(x, z) + 1; }};
    const auto centre = OceanRuinStructureGenerator::grid().candidateChunk(seed, 0, 0);
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t index = 0; index < 64; ++index) {
        Chunk chunk = seaChunk(centre.first + index % 8 - 4, centre.second + index / 8 - 4);
        {
            const Chunk::ExternalStateScope scope{
                chunk, [](std::int32_t x, std::int32_t y, std::int32_t z) -> std::optional<std::int32_t> {
                    return terrainAt(x, y, z);
                }};
            generator.generateInto(chunk, seed);
        }
        const auto [chunkChecksum, written] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.written += written;
        result.starts += chunk.structures().starts().size();
        for (const structure::StructureStartRecord& start : chunk.structures().starts()) {
            result.pieces += start.pieces.size();
            std::vector<structure::StructureBoundingBox> boxes = start.pieces;
            boxes.push_back(start.fullBounds);
            for (const structure::StructureBoundingBox& box : boxes) {
                for (const std::int32_t bound : {box.minX, box.minY, box.minZ, box.maxX, box.maxY, box.maxZ}) {
                    checksum =
                        (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(bound))) * 1099511628211ULL;
                }
            }
        }
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

void check(const Run& result, std::int32_t written, std::size_t starts, std::size_t pieces, std::int64_t checksum) {
    CHECK(result.written == written);
    CHECK(result.starts == starts);
    CHECK(result.pieces == pieces);
    CHECK(result.checksum == checksum);
}

} // namespace

/// The numbers were re-taken when the rot moved to its own positional random, which is what the jar
/// does: `OceanRuinPieces.makeSettings` never sets a random on the settings, so `getRandom(pos)`
/// hands out one seeded from the block. Checked against the jar on seed 113, where the ruin's census
/// then matched it on air, ice, mossy, cracked and snow exactly and on water to one block. The old
/// numbers came from the retired Kotlin tree, which stopped being the oracle with ADR-008.
TEST_CASE("the ocean ruins rot the way the jar rots them") {
    SUBCASE("cold") { check(run(1234, true, false), 132, 2, 6, 1866715353195390021LL); }

    SUBCASE("warm") { check(run(1234, false, true), 137, 2, 2, 952536064983659329LL); }

    SUBCASE("both") { check(run(1234, true, true), 132, 2, 6, 1866715353195390021LL); }

    SUBCASE("neither") { check(run(1234, false, false), 0, 0, 0, -9113849115773802060LL); }

    // A seed whose ruin drew large and then drew its cluster of small ones around it.
    SUBCASE("cluster") { check(run(7, true, true), 1599, 11, 40, -2284153736936210576LL); }

    SUBCASE("clusterWarm") { check(run(7, false, true), 825, 9, 11, 429754515008186462LL); }

    SUBCASE("otherSeed") { check(run(3, true, true), 248, 4, 12, -3108741486969595655LL); }
}
