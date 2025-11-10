#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/ShipwreckStructureGenerator.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain;
using basalt::wire::nbt::NbtCompound;
using basalt::wire::nbt::NbtList;
using basalt::wire::nbt::NbtTag;
using basalt::wire::nbt::NbtType;
namespace registry = basalt::server::registry;

namespace {

/// The ocean list is a superset of the beached one, so it indexes every distinct hull.
constexpr std::array<std::string_view, 20> kHulls{"shipwreck/with_mast",
                                                  "shipwreck/upsidedown_full",
                                                  "shipwreck/upsidedown_fronthalf",
                                                  "shipwreck/upsidedown_backhalf",
                                                  "shipwreck/sideways_full",
                                                  "shipwreck/sideways_fronthalf",
                                                  "shipwreck/sideways_backhalf",
                                                  "shipwreck/rightsideup_full",
                                                  "shipwreck/rightsideup_fronthalf",
                                                  "shipwreck/rightsideup_backhalf",
                                                  "shipwreck/with_mast_degraded",
                                                  "shipwreck/upsidedown_full_degraded",
                                                  "shipwreck/upsidedown_fronthalf_degraded",
                                                  "shipwreck/upsidedown_backhalf_degraded",
                                                  "shipwreck/sideways_full_degraded",
                                                  "shipwreck/sideways_fronthalf_degraded",
                                                  "shipwreck/sideways_backhalf_degraded",
                                                  "shipwreck/rightsideup_full_degraded",
                                                  "shipwreck/rightsideup_fronthalf_degraded",
                                                  "shipwreck/rightsideup_backhalf_degraded"};

[[nodiscard]] std::int32_t floorMod(std::int32_t value, std::int32_t divisor) {
    const std::int32_t rest = value % divisor;
    return rest < 0 ? rest + divisor : rest;
}

/// A stand-in hull whose size and planking depend on which name it answers to, so the draw that
/// picks one of the twenty is observable. The three markers sit over a chest, a barrel and a plank,
/// which is what tells the container test from a marker that simply fires.
[[nodiscard]] structure::StructureTemplate hull(std::int32_t index) {
    const std::int32_t sizeX = 20 + index % 3;
    const std::int32_t sizeY = 6 + index % 4;
    const std::int32_t sizeZ = 28 + index % 5;
    NbtList palette{NbtType::Compound};
    for (const std::string_view name : {"minecraft:oak_planks", "minecraft:structure_block", "minecraft:chest",
                                        "minecraft:barrel", "minecraft:air"}) {
        NbtCompound entry;
        entry.set("Name", NbtTag::ofString(std::string{name}));
        palette.push(NbtTag::ofCompound(std::move(entry)));
    }
    const auto markerAt = [](std::int32_t x, std::int32_t y, std::int32_t z) -> std::string_view {
        if (x == 2 && y == 2 && z == 5) {
            return "map_chest";
        }
        if (x == 6 && y == 2 && z == 9) {
            return "treasure_chest";
        }
        return (x == 10 && y == 2 && z == 13) ? "supply_chest" : std::string_view{};
    };
    const auto containerAt = [](std::int32_t x, std::int32_t y, std::int32_t z) {
        if (x == 2 && y == 1 && z == 5) {
            return 2;
        }
        return (x == 6 && y == 1 && z == 9) ? 3 : 0;
    };
    NbtList blocks{NbtType::Compound};
    for (std::int32_t x = 0; x < sizeX; ++x) {
        for (std::int32_t y = 0; y < sizeY; ++y) {
            for (std::int32_t z = 0; z < sizeZ; ++z) {
                const std::string_view marker = markerAt(x, y, z);
                const std::int32_t container = containerAt(x, y, z);
                NbtCompound block;
                block.set("pos", NbtTag::ofList(
                                     NbtList{NbtType::Int, {NbtTag::ofInt(x), NbtTag::ofInt(y), NbtTag::ofInt(z)}}));
                // A hollow hull, so most of it is air the processor drops.
                const bool hollow = y > 0 && x >= 1 && x < sizeX - 1 && z >= 1 && z < sizeZ - 1;
                block.set("state", NbtTag::ofInt(!marker.empty() ? 1 : container != 0 ? container : hollow ? 4 : 0));
                if (!marker.empty()) {
                    NbtCompound data;
                    data.set("mode", NbtTag::ofString("DATA"));
                    data.set("metadata", NbtTag::ofString(std::string{marker}));
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
    static const std::vector<structure::StructureTemplate> built = [] {
        std::vector<structure::StructureTemplate> all;
        all.reserve(kHulls.size());
        for (std::size_t index = 0; index < kHulls.size(); ++index) {
            all.push_back(hull(static_cast<std::int32_t>(index)));
        }
        return all;
    }();
    for (std::size_t index = 0; index < kHulls.size(); ++index) {
        if (kHulls[index] == name) {
            return &built[index];
        }
    }
    return nullptr;
}

/// The oracle folds the loot table's name through `String.hashCode`, so the two must agree.
[[nodiscard]] std::int32_t javaHash(std::string_view text) {
    std::int32_t hash = 0;
    for (const char letter : text) {
        hash = static_cast<std::int32_t>(static_cast<std::uint32_t>(hash) * 31U +
                                         static_cast<std::uint32_t>(static_cast<unsigned char>(letter)));
    }
    return hash;
}

/// Stone up to the beach, so the air a hull carries carves it instead of landing on air.
[[nodiscard]] Chunk stoneChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 34; y <= 66; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::tuple<std::int64_t, std::int32_t, std::size_t> fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t written = 0;
    for (std::int32_t y = 34; y <= 78; ++y) {
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
        checksum = (checksum ^ static_cast<std::uint64_t>(
                                   static_cast<std::int64_t>(javaHash(entity.data().getString("LootTable", ""))))) *
                   1099511628211ULL;
    }
    return {static_cast<std::int64_t>(checksum), written, chunk.blockEntities().size()};
}

struct Run {
    std::int32_t written = 0;
    std::size_t blockEntities = 0;
    std::size_t starts = 0;
    std::int64_t checksum = 0;
};

[[nodiscard]] Run run(std::int64_t seed, bool ocean, bool beach) {
    // Two slopes, so the mean of the footprint differs from its lowest column and from the column
    // the wreck started on.
    const auto oceanFloorAt = [](std::int32_t x, std::int32_t z) { return 45 + floorMod(x * 3 + z, 5); };
    const auto surfaceHeightAt = [](std::int32_t x, std::int32_t z) { return 62 + floorMod(x + z * 2, 7); };
    ShipwreckStructureGenerator generator{
        // The biome is only the one block under each heightmap, so which heightmap the variant
        // reads, and the block below it, are both observable.
        // Only the centre column of a chunk answers anything but plains, so where the variant
        // probe stands is observable too.
        [ocean, beach, oceanFloorAt, surfaceHeightAt](std::int32_t x, std::int32_t y, std::int32_t z) -> std::string {
            if (floorMod(x, 16) != 8 || floorMod(z, 16) != 8) {
                return "minecraft:plains";
            }
            if (y == oceanFloorAt(x, z) - 1) {
                return ocean ? "minecraft:ocean" : "minecraft:plains";
            }
            if (y == surfaceHeightAt(x, z) - 1) {
                return beach ? "minecraft:beach" : "minecraft:plains";
            }
            return "minecraft:plains";
        },
        {"minecraft:ocean", "minecraft:deep_ocean"},
        {"minecraft:beach", "minecraft:snowy_beach"},
        templateOf,
        surfaceHeightAt,
        oceanFloorAt};
    const auto centre = ShipwreckStructureGenerator::grid().candidateChunk(seed, 0, 0);
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t index = 0; index < 64; ++index) {
        Chunk chunk = stoneChunk(centre.first + index % 8 - 4, centre.second + index / 8 - 4);
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

TEST_CASE("the shipwrecks sink where the frozen kotlin sinks them") {
    // Seed 1234 draws the beached one first, so the ocean-only case is the rejection path.
    SUBCASE("sunk") { check(run(1234, true, false), 1248, 2, 9, -5492804444073661496LL); }

    SUBCASE("beached") { check(run(1234, false, true), 1194, 2, 9, 6374327342091595417LL); }

    SUBCASE("both") { check(run(1234, true, true), 1194, 2, 9, 6374327342091595417LL); }

    // Seed 2 draws the sunk one first, which is the other half of both orders.
    SUBCASE("bothSunk") { check(run(2, true, true), 1242, 2, 6, 1975696698024398962LL); }

    SUBCASE("beachedAfterReject") { check(run(2, false, true), 1490, 2, 6, 3939666762326817481LL); }

    SUBCASE("neither") { check(run(1234, false, false), 0, 0, 0, -218632340393939867LL); }

    SUBCASE("otherSeed") { check(run(77, true, true), 1240, 2, 4, -6099322691228064550LL); }
}
