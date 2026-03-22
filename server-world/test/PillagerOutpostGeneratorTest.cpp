#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/SyntheticRouter.hpp"

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/densityfunction/SamplerContext.hpp>
#include <basalt/server/world/terrain/JigsawStructureGenerator.hpp>

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
using basalt::wire::nbt::NbtCompound;
using basalt::wire::nbt::NbtList;
using basalt::wire::nbt::NbtTag;
using basalt::wire::nbt::NbtType;
namespace registry = basalt::server::registry;
namespace df = basalt::server::world::densityfunction;

namespace {

struct Connector {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::string_view orientation;
    std::string_view name;
    std::string_view pool;
    std::string_view target;
};

constexpr std::array<std::string_view, 6> kOrientations{"north_up", "south_up", "west_up",
                                                        "east_up",  "up_north", "down_north"};

/// A synthetic outpost piece: a box of stone with one jigsaw block per connector. Nothing here comes
/// from a Mojang structure file; what is under test is the generator, not the buildings.
[[nodiscard]] structure::StructureTemplate piece(std::int32_t sizeX, std::int32_t sizeY, std::int32_t sizeZ,
                                                 const std::vector<Connector>& connectors) {
    NbtList palette{NbtType::Compound};
    for (const std::string_view name : {"minecraft:cobblestone", "minecraft:chest"}) {
        NbtCompound entry;
        entry.set("Name", NbtTag::ofString(std::string{name}));
        palette.push(NbtTag::ofCompound(std::move(entry)));
    }
    constexpr std::int32_t kChestState = 1;
    for (const std::string_view orientation : kOrientations) {
        NbtCompound properties;
        properties.set("orientation", NbtTag::ofString(std::string{orientation}));
        NbtCompound entry;
        entry.set("Name", NbtTag::ofString("minecraft:jigsaw"));
        entry.set("Properties", NbtTag::ofCompound(std::move(properties)));
        palette.push(NbtTag::ofCompound(std::move(entry)));
    }
    const auto connectorAt = [&connectors](std::int32_t x, std::int32_t y, std::int32_t z) -> const Connector* {
        for (const Connector& connector : connectors) {
            if (connector.x == x && connector.y == y && connector.z == z) {
                return &connector;
            }
        }
        return nullptr;
    };
    NbtList blocks{NbtType::Compound};
    for (std::int32_t x = 0; x < sizeX; ++x) {
        for (std::int32_t y = 0; y < sizeY; ++y) {
            for (std::int32_t z = 0; z < sizeZ; ++z) {
                if (connectorAt(x, y, z) != nullptr) {
                    continue;
                }
                NbtCompound block;
                block.set("pos", NbtTag::ofList(
                                     NbtList{NbtType::Int, {NbtTag::ofInt(x), NbtTag::ofInt(y), NbtTag::ofInt(z)}}));
                // One chest per piece, whose loot seed comes off the feature stream: without it
                // nothing a piece places depends on that stream at all.
                const bool chestHere = x == 1 && y == 0 && z == 1;
                block.set("state", NbtTag::ofInt(chestHere ? kChestState : 0));
                if (chestHere) {
                    NbtCompound data;
                    data.set("id", NbtTag::ofString("minecraft:chest"));
                    data.set("LootTable", NbtTag::ofString("minecraft:chests/pillager_outpost"));
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
        data.set("pool", NbtTag::ofString(std::string{connector.pool}));
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
        // Four arms, so the outpost crosses its chunk's border on both axes.
        all.emplace("minecraft:pillager_outpost/base_plate",
                    piece(9, 1, 9,
                          {Connector{4, 0, 4, "up_north", "plate", "minecraft:pillager_outpost/towers", "tower"},
                           Connector{0, 0, 4, "west_up", "west", "minecraft:pillager_outpost/features", "feature"},
                           Connector{8, 0, 4, "east_up", "east", "minecraft:pillager_outpost/features", "feature"},
                           Connector{4, 0, 0, "north_up", "north", "minecraft:pillager_outpost/features", "feature"},
                           Connector{4, 0, 8, "south_up", "south", "minecraft:pillager_outpost/features", "feature"}}));
        all.emplace("minecraft:pillager_outpost/watchtower",
                    piece(7, 11, 7,
                          {Connector{3, 0, 3, "down_north", "tower", "minecraft:empty", "empty"},
                           Connector{6, 4, 3, "east_up", "arm", "minecraft:pillager_outpost/features", "feature"},
                           // Points back into its own box, which is the only shape the expansion
                           // hack grows a piece for.
                           Connector{3, 5, 3, "east_up", "inner", "minecraft:pillager_outpost/features", "feature"}}));
        // Each tent hangs another one off itself, so how deep the assembly is allowed to go and how
        // far it may reach both decide where it stops.
        all.emplace("minecraft:pillager_outpost/tent",
                    piece(9, 13, 9,
                          {Connector{0, 1, 4, "west_up", "feature", "minecraft:empty", "empty"},
                           Connector{8, 1, 4, "east_up", "tent_out", "minecraft:pillager_outpost/posts", "post"}}));
        all.emplace("minecraft:pillager_outpost/cage",
                    piece(5, 4, 5, {Connector{0, 1, 2, "west_up", "feature", "minecraft:empty", "empty"}}));
        // Seven links of seven blocks reach further than half the allowed radius but not all of it,
        // so how deep the assembly may go and how far it may reach both bite.
        all.emplace("minecraft:pillager_outpost/post",
                    piece(7, 3, 7,
                          {Connector{0, 1, 3, "west_up", "post", "minecraft:empty", "empty"},
                           Connector{6, 1, 3, "east_up", "post_out", "minecraft:pillager_outpost/posts", "post"}}));
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
        all.emplace("minecraft:pillager_outpost/base_plates",
                    structure::StructureTemplatePool{"minecraft:empty",
                                                     {{single("minecraft:pillager_outpost/base_plate"), 1}}});
        all.emplace("minecraft:pillager_outpost/towers",
                    structure::StructureTemplatePool{"minecraft:empty",
                                                     {{single("minecraft:pillager_outpost/watchtower"), 1}}});
        // Every link settles on the terrain, so a height is read wherever the chain got to and not
        // only where it started.
        all.emplace(
            "minecraft:pillager_outpost/posts",
            structure::StructureTemplatePool{
                "minecraft:empty",
                {{single("minecraft:pillager_outpost/post", structure::StructureProjection::TerrainMatching), 1}}});
        all.emplace(
            "minecraft:pillager_outpost/features",
            structure::StructureTemplatePool{
                "minecraft:empty",
                {{single("minecraft:pillager_outpost/tent"), 3},
                 // The one piece that is neither rigid nor whole: the beard skips it and the rot
                 // makes the placement draw from the feature stream.
                 {single("minecraft:pillager_outpost/cage", structure::StructureProjection::TerrainMatching, 0.7F),
                  1}}});
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

[[nodiscard]] structure::StructurePlacementGrid grid() {
    return structure::StructurePlacementGrid{32, 8, 165745296, structure::SpreadType::Linear, 0.2F};
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t written = 0;
    for (std::int32_t y = 40; y <= 110; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                if (id != registry::blocks::air().stateId) {
                    ++written;
                }
            }
        }
    }
    for (const ChunkBlockEntity& entity : chunk.blockEntities()) {
        checksum =
            (checksum ^ static_cast<std::uint64_t>(entity.data().getLong("LootTableSeed", 0))) * 1099511628211ULL;
    }
    for (const structure::StructureStartRecord& start : chunk.structures().starts()) {
        std::vector<structure::StructureBoundingBox> boxes = start.pieces;
        boxes.push_back(start.fullBounds);
        for (const structure::StructureBoundingBox& box : boxes) {
            for (const std::int32_t bound : {box.minX, box.minY, box.minZ, box.maxX, box.maxY, box.maxZ}) {
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(bound))) * 1099511628211ULL;
            }
        }
        checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(start.chunkX)) ^
                    (static_cast<std::uint64_t>(static_cast<std::int64_t>(start.chunkZ)) << 8)) *
                   1099511628211ULL;
    }
    return {static_cast<std::int64_t>(checksum), written};
}

struct Run {
    std::int32_t written = 0;
    std::size_t starts = 0;
    std::int64_t checksum = 0;
    std::string located;
    std::size_t cached = 0;
    std::int64_t cachedPieces = 0;
};

[[nodiscard]] Run run(std::int64_t seed, std::string_view biome) {
    const std::string biomeName{biome};
    JigsawStructureGenerator generator{jigsawStructures::pillagerOutpost(),
                                       router(seed),
                                       grid(),
                                       [biomeName](std::int32_t, std::int32_t, std::int32_t) { return biomeName; },
                                       {"minecraft:plains", "minecraft:taiga"},
                                       poolLookup,
                                       loader};

    // Centred on the outpost the search finds, so the sweep always covers a real one when the biome
    // allows any at all.
    const std::optional<std::pair<std::int32_t, std::int32_t>> found = generator.locate(seed, 0, 0, 6);
    const std::pair<std::int32_t, std::int32_t> centre = found.value_or(grid().candidateChunk(seed, 0, 0));
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t index = 0; index < 36; ++index) {
        const std::int32_t chunkX = centre.first + index % 6 - 3;
        const std::int32_t chunkZ = centre.second + index / 6 - 3;
        Chunk chunk{chunkX, chunkZ};
        generator.generateInto(chunk, chunkX, chunkZ, seed);
        const auto [chunkChecksum, written] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.written += written;
        result.starts += chunk.structures().starts().size();
    }
    // The beard of the chunk the outpost starts in, sampled over its own cell grid.
    StructureBeard beard;
    generator.addBeard(beard, centre.first, centre.second, seed);
    const df::Sampler bearded = structureBeardifier(std::move(beard));
    df::SamplerContext context{df::SamplerContext::Options{}};
    for (std::int32_t y = 48; y <= 80; y += 4) {
        for (std::int32_t x = 0; x < 16; x += 4) {
            for (std::int32_t z = 0; z < 16; z += 4) {
                // A chunk no structure reaches has no beardifier at all, which samples as zero.
                const float value =
                    bearded ? bearded->sampleValue(context, centre.first * 16 + x, y, centre.second * 16 + z) : 0.0F;
                checksum = (checksum ^ std::bit_cast<std::uint32_t>(value)) * 1099511628211ULL;
            }
        }
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    result.located = found.has_value() ? std::to_string(found->first) + "," + std::to_string(found->second)
                                       : std::string{"null,null"};
    const auto [cached, cachedPieces] = generator.assemblyStats();
    result.cached = cached;
    result.cachedPieces = cachedPieces;
    return result;
}

void check(const Run& result, std::int32_t written, std::size_t starts, std::int64_t checksum, std::string_view located,
           std::size_t cached, std::int64_t cachedPieces) {
    CHECK(result.written == written);
    CHECK(result.starts == starts);
    CHECK(result.checksum == checksum);
    CHECK(result.located == located);
    CHECK(result.cached == cached);
    CHECK(result.cachedPieces == cachedPieces);
}

} // namespace

TEST_CASE("the pillager outposts stand where the frozen kotlin stands them") {
    SUBCASE("plains") { check(run(1234, "minecraft:plains"), 3765, 12, -1196918007363053241LL, "-11,-32", 2, 24); }

    SUBCASE("ocean") { check(run(1234, "minecraft:ocean"), 0, 0, -1334922393077245327LL, "null,null", 10, 0); }

    SUBCASE("otherSeed") { check(run(77, "minecraft:taiga"), 2714, 7, -5548298109229239124LL, "11,11", 1, 12); }

    SUBCASE("thirdSeed") { check(run(9001, "minecraft:plains"), 2714, 7, -4862883459987911101LL, "1,0", 1, 18); }
}
