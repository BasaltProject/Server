#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/SyntheticRouter.hpp"

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/VillageStructureGenerator.hpp>

#include <doctest.h>

#include <array>
#include <bit>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain;
using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtCompound;
using basalt::wire::nbt::NbtList;
using basalt::wire::nbt::NbtTag;
using basalt::wire::nbt::NbtType;
namespace registry = basalt::server::registry;
namespace df = basalt::server::world::densityfunction;

namespace {

constexpr std::array<std::string_view, 5> kKinds{"plains", "desert", "savanna", "snowy", "taiga"};

/// A different wall block per kind, so which variant was drawn shows in the blocks.
[[nodiscard]] std::string_view wallOf(std::string_view kind) {
    if (kind == "plains") {
        return "minecraft:oak_planks";
    }
    if (kind == "desert") {
        return "minecraft:cut_sandstone";
    }
    if (kind == "savanna") {
        return "minecraft:acacia_planks";
    }
    return kind == "snowy" ? "minecraft:spruce_planks" : "minecraft:cobblestone";
}

struct Connector {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::string_view orientation;
    std::string_view name;
    std::string pool;
    std::string_view target;
};

constexpr std::array<std::string_view, 6> kOrientations{"north_up", "south_up", "west_up",
                                                        "east_up",  "up_north", "down_north"};

/// A synthetic village piece: a box of planks with a chest and one jigsaw block per connector.
/// Nothing here comes from a Mojang structure file; what is under test is the generator.
[[nodiscard]] structure::StructureTemplate piece(std::int32_t sizeX, std::int32_t sizeY, std::int32_t sizeZ,
                                                 std::string_view wall, const std::vector<Connector>& connectors) {
    NbtList palette{NbtType::Compound};
    for (const std::string_view name : {wall, std::string_view{"minecraft:chest"}}) {
        NbtCompound entry;
        entry.set("Name", NbtTag::ofString(std::string{name}));
        palette.push(NbtTag::ofCompound(std::move(entry)));
    }
    for (const std::string_view orientation : kOrientations) {
        NbtCompound properties;
        properties.set("orientation", NbtTag::ofString(std::string{orientation}));
        NbtCompound entry;
        entry.set("Name", NbtTag::ofString("minecraft:jigsaw"));
        entry.set("Properties", NbtTag::ofCompound(std::move(properties)));
        palette.push(NbtTag::ofCompound(std::move(entry)));
    }
    const auto isConnector = [&connectors](std::int32_t x, std::int32_t y, std::int32_t z) {
        for (const Connector& connector : connectors) {
            if (connector.x == x && connector.y == y && connector.z == z) {
                return true;
            }
        }
        return false;
    };
    NbtList blocks{NbtType::Compound};
    for (std::int32_t x = 0; x < sizeX; ++x) {
        for (std::int32_t y = 0; y < sizeY; ++y) {
            for (std::int32_t z = 0; z < sizeZ; ++z) {
                if (isConnector(x, y, z)) {
                    continue;
                }
                NbtCompound block;
                block.set("pos", NbtTag::ofList(
                                     NbtList{NbtType::Int, {NbtTag::ofInt(x), NbtTag::ofInt(y), NbtTag::ofInt(z)}}));
                const bool chestHere = x == 1 && y == 0 && z == 1;
                block.set("state", NbtTag::ofInt(chestHere ? 1 : 0));
                if (chestHere) {
                    NbtCompound data;
                    data.set("id", NbtTag::ofString("minecraft:chest"));
                    data.set("LootTable", NbtTag::ofString("minecraft:chests/village"));
                    block.set("nbt", NbtTag::ofCompound(std::move(data)));
                }
                blocks.push(NbtTag::ofCompound(std::move(block)));
            }
        }
    }
    for (const Connector& connector : connectors) {
        std::int32_t state = 0;
        for (std::size_t index = 0; index < kOrientations.size(); ++index) {
            if (kOrientations[index] == connector.orientation) {
                state = 2 + static_cast<std::int32_t>(index);
            }
        }
        NbtCompound data;
        data.set("name", NbtTag::ofString(std::string{connector.name}));
        data.set("pool", NbtTag::ofString(connector.pool));
        data.set("target", NbtTag::ofString(std::string{connector.target}));
        data.set("final_state", NbtTag::ofString("minecraft:air"));
        data.set("joint", NbtTag::ofString("rollable"));
        data.set("placement_priority", NbtTag::ofInt(0));
        data.set("selection_priority", NbtTag::ofInt(0));
        NbtCompound block;
        block.set("pos", NbtTag::ofList(NbtList{
                             NbtType::Int,
                             {NbtTag::ofInt(connector.x), NbtTag::ofInt(connector.y), NbtTag::ofInt(connector.z)}}));
        block.set("state", NbtTag::ofInt(state));
        block.set("nbt", NbtTag::ofCompound(std::move(data)));
        blocks.push(NbtTag::ofCompound(std::move(block)));
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

[[nodiscard]] const std::map<std::string, structure::StructureTemplate, std::less<>>& templates() {
    static const std::map<std::string, structure::StructureTemplate, std::less<>> built = [] {
        std::map<std::string, structure::StructureTemplate, std::less<>> all;
        for (const std::string_view kind : kKinds) {
            const std::string prefix = "minecraft:village/" + std::string{kind};
            const std::string houses = prefix + "/houses";
            const std::string_view wall = wallOf(kind);
            all.emplace(prefix + "/town_center", piece(9, 5, 9, wall,
                                                       {Connector{0, 1, 4, "west_up", "west", houses, "house"},
                                                        Connector{8, 1, 4, "east_up", "east", houses, "house"},
                                                        Connector{4, 1, 0, "north_up", "north", houses, "house"},
                                                        Connector{4, 1, 8, "south_up", "south", houses, "house"},
                                                        // Points back into its own box, which is the only shape the
                                                        // expansion hack grows a piece for.
                                                        Connector{4, 1, 4, "east_up", "inner", houses, "house"}}));
            all.emplace(prefix + "/house", piece(7, 5, 7, wall,
                                                 {Connector{0, 1, 3, "west_up", "house", "minecraft:empty", "empty"},
                                                  Connector{6, 1, 3, "east_up", "house_out", houses, "house"}}));
            all.emplace(prefix + "/hut",
                        piece(5, 9, 5, wall, {Connector{0, 1, 2, "west_up", "house", "minecraft:empty", "empty"}}));
        }
        return all;
    }();
    return built;
}

[[nodiscard]] const structure::StructureTemplate* loader(std::string_view name) {
    const auto found = templates().find(name);
    return found == templates().end() ? nullptr : &found->second;
}

[[nodiscard]] structure::StructurePoolElement
single(std::string location, structure::StructureProjection projection = structure::StructureProjection::Rigid,
       float integrity = 1.0F) {
    structure::StructureProcessorList processors;
    if (integrity < 1.0F) {
        processors = structure::StructureProcessorList{{}, structure::BlockRot{.integrity = integrity}};
    }
    return structure::StructurePoolElement::legacySingle(std::move(location), std::move(processors), projection);
}

[[nodiscard]] const structure::StructureTemplatePool* poolLookup(std::string_view name) {
    static const std::map<std::string, structure::StructureTemplatePool, std::less<>> pools = [] {
        std::map<std::string, structure::StructureTemplatePool, std::less<>> all;
        all.emplace("minecraft:empty", structure::StructureTemplatePool{
                                           "minecraft:empty", {{structure::StructurePoolElement::empty(), 1}}});
        for (const std::string_view kind : kKinds) {
            const std::string prefix = "minecraft:village/" + std::string{kind};
            all.emplace(prefix + "/town_centers",
                        structure::StructureTemplatePool{"minecraft:empty", {{single(prefix + "/town_center"), 1}}});
            all.emplace(prefix + "/houses",
                        structure::StructureTemplatePool{
                            "minecraft:empty",
                            {{single(prefix + "/house"), 3},
                             {single(prefix + "/hut", structure::StructureProjection::TerrainMatching, 0.7F), 1}}});
        }
        return all;
    }();
    const auto found = pools.find(name);
    return found == pools.end() ? nullptr : &found->second;
}

/// A rolling surface around y 60, so which column a height is read from is observable.
[[nodiscard]] NoiseRouter router(std::int64_t seed) {
    return basalt::server::world::testsupport::syntheticRouter(
        seed, basalt::server::world::testsupport::rollingFloor(50, 78, 1.0, 0.0));
}

[[nodiscard]] std::int32_t surfaceOf(std::int64_t seed, std::int32_t x, std::int32_t z) {
    DensityChunkSampler sampler{router(seed), x >> 4, z >> 4};
    std::int32_t y = 319;
    while (y >= -64 && sampler.sample(x & 15, y, z & 15) <= 0.0) {
        --y;
    }
    return std::max(y + 1, 63);
}

[[nodiscard]] structure::StructurePlacementGrid grid() { return structure::StructurePlacementGrid{34, 8, 10387312}; }

/// A grid tight enough that several villages fall in one sweep, so the write order shows.
[[nodiscard]] structure::StructurePlacementGrid denseGrid() {
    return structure::StructurePlacementGrid{6, 2, 10387312};
}

[[nodiscard]] Chunk stoneChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 40; y <= 62; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t written = 0;
    for (std::int32_t y = 40; y <= 110; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                if (id != registry::blocks::air().stateId && id != registry::blocks::stone().stateId) {
                    ++written;
                }
            }
        }
    }
    for (const ChunkBlockEntity& entity : chunk.blockEntities()) {
        checksum =
            (checksum ^ static_cast<std::uint64_t>(entity.data().getLong("LootTableSeed", 0))) * 1099511628211ULL;
    }
    return {static_cast<std::int64_t>(checksum), written};
}

struct Run {
    std::int32_t written = 0;
    std::int32_t commandWritten = 0;
    std::string placed;
    std::size_t pooled = 0;
    std::string located;
    std::string snowy;
    std::size_t cached = 0;
    std::int64_t cachedPieces = 0;
    std::int64_t checksum = 0;
};

[[nodiscard]] Run run(std::int64_t seed, std::string_view biome, std::string_view mixWith = {}, bool dense = false) {
    const std::string biomeName{biome};
    const std::string mixName{mixWith};
    const structure::StructurePlacementGrid placement = dense ? denseGrid() : grid();
    VillageStructureGenerator generator{
        router(seed), placement,
        // Alternating chunk columns get the other biome when one is given, so two variants stand
        // near each other and the order they are written in is observable.
        [seed, biomeName, mixName](std::int32_t x, std::int32_t y, std::int32_t z) -> std::string {
            // Only at the surface, so the height a biome is read at stays observable. parity: the
            // column is the start piece's centre, which is where vanilla puts the generation stub.
            const std::string& here = (!mixName.empty() && ((x >> 4) & 1) == 1) ? mixName : biomeName;
            return y == surfaceOf(seed, x, z) ? here : std::string{"minecraft:ocean"};
        },
        6, 80, poolLookup, loader};

    const std::optional<LocatedVillage> found = generator.locate(seed, 0, 0, nullptr, 6);
    const std::pair<std::int32_t, std::int32_t> centre =
        found.has_value() ? std::pair{found->chunkX, found->chunkZ} : placement.candidateChunk(seed, 0, 0);
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t index = 0; index < 36; ++index) {
        const std::int32_t chunkX = centre.first + index % 6 - 3;
        const std::int32_t chunkZ = centre.second + index / 6 - 3;
        Chunk chunk = stoneChunk(chunkX, chunkZ);
        generator.generateInto(chunk, chunkX, chunkZ, seed);
        const auto [chunkChecksum, written] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.written += written;
    }
    StructureBeard beard;
    generator.addBeard(beard, centre.first, centre.second, seed);
    const DensityChunkSampler beardified{router(seed), centre.first, centre.second,
                                         structureBeardifier(std::move(beard))};
    for (std::int32_t y = 48; y <= 80; y += 4) {
        for (std::int32_t x = 0; x < 16; x += 4) {
            for (std::int32_t z = 0; z < 16; z += 4) {
                checksum = (checksum ^ std::bit_cast<std::uint64_t>(beardified.sample(x, y, z))) * 1099511628211ULL;
            }
        }
    }
    // The two command paths, which skip the grid and the biome entirely.
    const std::optional<std::vector<structure::PlacedStructurePiece>> placed =
        generator.assembleAt(Identifier::vanilla("village_taiga"), centre.first, centre.second, seed);
    const std::vector<structure::PlacedStructurePiece> pooled =
        generator.assemblePool("minecraft:village/desert/houses", centre.first, centre.second, 3, seed);
    Chunk placedChunk = stoneChunk(centre.first, centre.second);
    if (placed.has_value()) {
        generator.placeInto(placedChunk, *placed, seed);
    }
    generator.placeInto(placedChunk, pooled, seed);
    const auto [commandChecksum, commandWritten] = fold(placedChunk);
    checksum = (checksum ^ static_cast<std::uint64_t>(commandChecksum)) * 1099511628211ULL;
    const Identifier snowy = Identifier::vanilla("village_snowy");
    const std::optional<LocatedVillage> wanted = generator.locate(seed, 0, 0, &snowy, 6);
    const auto [cached, cachedPieces] = generator.assemblyStats();

    result.commandWritten = commandWritten;
    result.placed = placed.has_value() ? std::to_string(placed->size()) : std::string{"null"};
    result.pooled = pooled.size();
    result.located = found.has_value() ? std::string{found->structure.path()} + "," + std::to_string(found->chunkX) +
                                             "," + std::to_string(found->chunkZ)
                                       : std::string{"null,null,null"};
    result.snowy = wanted.has_value() ? std::to_string(wanted->chunkX) + "," + std::to_string(wanted->chunkZ)
                                      : std::string{"null,null"};
    result.cached = cached;
    result.cachedPieces = cachedPieces;
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

void check(const Run& result, std::int32_t written, std::int32_t commandWritten, std::string_view placed,
           std::size_t pooled, std::string_view located, std::string_view snowy, std::size_t cached,
           std::int64_t cachedPieces, std::int64_t checksum) {
    CHECK(result.written == written);
    CHECK(result.commandWritten == commandWritten);
    CHECK(result.placed == placed);
    CHECK(result.pooled == pooled);
    CHECK(result.located == located);
    CHECK(result.snowy == snowy);
    CHECK(result.cached == cached);
    CHECK(result.cachedPieces == cachedPieces);
    CHECK(result.checksum == checksum);
}

} // namespace

TEST_CASE("the villages settle where the frozen kotlin settles them") {
    SUBCASE("plains") {
        check(run(1234, "minecraft:plains"), 3184, 438, "14", 3, "village_plains,3,5", "null,null", 1, 14,
              -196860553577654477LL);
    }

    SUBCASE("desert") {
        check(run(1234, "minecraft:desert"), 3184, 438, "14", 3, "village_desert,3,5", "null,null", 1, 14,
              -6749051946456810776LL);
    }

    SUBCASE("savanna") {
        check(run(1234, "minecraft:savanna"), 3184, 438, "14", 3, "village_savanna,3,5", "null,null", 1, 14,
              7049679602001583793LL);
    }

    SUBCASE("snowy") {
        check(run(1234, "minecraft:snowy_plains"), 3184, 438, "14", 3, "village_snowy,3,5", "3,5", 1, 14,
              8682671500369845829LL);
    }

    SUBCASE("taiga") {
        check(run(1234, "minecraft:taiga"), 3184, 438, "14", 3, "village_taiga,3,5", "null,null", 1, 14,
              -8216128194307540567LL);
    }

    SUBCASE("meadow") {
        check(run(1234, "minecraft:meadow"), 3184, 438, "14", 3, "village_plains,3,5", "null,null", 1, 14,
              -196860553577654477LL);
    }

    SUBCASE("none") {
        check(run(1234, "minecraft:ocean"), 0, 438, "14", 3, "null,null,null", "null,null", 1, 0,
              8823090479833660112LL);
    }

    SUBCASE("otherSeed") {
        check(run(77, "minecraft:plains"), 3737, 5, "16", 4, "village_plains,18,6", "null,null", 1, 16,
              5790473476667058360LL);
    }

    // A tight grid puts several villages in one sweep, two variants among them, which is the only
    // shape in which the order they are written in can be seen.
    SUBCASE("dense") {
        check(run(1234, "minecraft:plains", "minecraft:desert", true), 5863, 592, "20", 1, "village_plains,1,2",
              "null,null", 9, 133, 6467505898491498425LL);
    }

    SUBCASE("denseOther") {
        check(run(77, "minecraft:taiga", "minecraft:savanna", true), 2490, 566, "11", 3, "village_taiga,2,1",
              "null,null", 9, 134, -1046519904409833445LL);
    }
}
