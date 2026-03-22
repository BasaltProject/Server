#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/SyntheticRouter.hpp"

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/noise/NormalNoise.hpp>
#include <basalt/server/world/terrain/NetherStructureGenerator.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
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

/// A stand-in piece. Fossils are bone, ruined portals are obsidian with a chest and a slab the ager
/// can rot, and bastion pieces carry a jigsaw. Nothing here comes from a Mojang file.
[[nodiscard]] structure::StructureTemplate piece(std::int32_t index, std::string_view kind) {
    const std::int32_t sizeX = 4 + index % 3;
    const std::int32_t sizeY = 4 + index % 4;
    const std::int32_t sizeZ = 4 + index % 2;
    NbtList palette{NbtType::Compound};
    for (const std::string_view name :
         {"minecraft:bone_block", "minecraft:obsidian", "minecraft:chest", "minecraft:stone_brick_slab",
          "minecraft:gold_block", "minecraft:air", "minecraft:jigsaw"}) {
        NbtCompound entry;
        entry.set("Name", NbtTag::ofString(std::string{name}));
        if (name == "minecraft:jigsaw") {
            NbtCompound properties;
            properties.set("orientation", NbtTag::ofString("east_up"));
            entry.set("Properties", NbtTag::ofCompound(std::move(properties)));
        }
        palette.push(NbtTag::ofCompound(std::move(entry)));
    }
    NbtList blocks{NbtType::Compound};
    for (std::int32_t x = 0; x < sizeX; ++x) {
        for (std::int32_t y = 0; y < sizeY; ++y) {
            for (std::int32_t z = 0; z < sizeZ; ++z) {
                const bool chestHere = kind == "portal" && x == 1 && y == 1 && z == 1;
                // The portal gets one too: its second pass writes the final state even where the
                // processors refused the block.
                const bool jigsawHere = (kind == "bastion" || kind == "portal") && x == sizeX - 1 && y == 1 && z == 1;
                NbtCompound block;
                block.set("pos", NbtTag::ofList(
                                     NbtList{NbtType::Int, {NbtTag::ofInt(x), NbtTag::ofInt(y), NbtTag::ofInt(z)}}));
                std::int32_t stateIndex = 1;
                if (chestHere) {
                    stateIndex = 2;
                } else if (jigsawHere) {
                    stateIndex = 6;
                } else if (kind == "fossil") {
                    // A sparse rib cage, so the dried ghast has air to land in.
                    stateIndex = (x + y + z) % 2 == 0 ? 0 : 5;
                } else if (kind == "portal" && (x + y + z) % 4 == 0) {
                    stateIndex = 3;
                } else if (kind == "portal" && (x + y + z) % 4 == 1) {
                    stateIndex = 4;
                }
                block.set("state", NbtTag::ofInt(stateIndex));
                if (chestHere) {
                    NbtCompound data;
                    data.set("id", NbtTag::ofString("minecraft:chest"));
                    data.set("LootTable", NbtTag::ofString("minecraft:chests/ruined_portal"));
                    block.set("nbt", NbtTag::ofCompound(std::move(data)));
                }
                if (jigsawHere) {
                    NbtCompound data;
                    data.set("name", NbtTag::ofString("side"));
                    data.set("pool", NbtTag::ofString("minecraft:bastion/wings"));
                    data.set("target", NbtTag::ofString("wing"));
                    data.set("final_state",
                             NbtTag::ofString(kind == "portal" ? "minecraft:netherrack" : "minecraft:air"));
                    data.set("joint", NbtTag::ofString("rollable"));
                    data.set("placement_priority", NbtTag::ofInt(0));
                    data.set("selection_priority", NbtTag::ofInt(0));
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

[[nodiscard]] const structure::StructureTemplate* loader(const Identifier& name) {
    static const std::map<std::string, structure::StructureTemplate, std::less<>> built = [] {
        std::map<std::string, structure::StructureTemplate, std::less<>> all;
        for (std::int32_t index = 1; index <= 14; ++index) {
            all.emplace("minecraft:nether_fossils/fossil_" + std::to_string(index), piece(index, "fossil"));
        }
        for (std::int32_t index = 1; index <= 10; ++index) {
            all.emplace("minecraft:ruined_portal/portal_" + std::to_string(index), piece(index, "portal"));
        }
        for (std::int32_t index = 1; index <= 3; ++index) {
            all.emplace("minecraft:ruined_portal/giant_portal_" + std::to_string(index), piece(index + 10, "portal"));
        }
        all.emplace("minecraft:bastion/start", piece(1, "bastion"));
        all.emplace("minecraft:bastion/wing", piece(2, "bastion"));
        return all;
    }();
    const auto found = built.find(name.toString());
    return found == built.end() ? nullptr : &found->second;
}

[[nodiscard]] const structure::StructureTemplatePool* poolLookup(std::string_view name) {
    static const std::map<std::string, structure::StructureTemplatePool, std::less<>> pools = [] {
        const auto single = [](std::string location) {
            return structure::StructurePoolElement::legacySingle(
                std::move(location), structure::StructureProcessorList{}, structure::StructureProjection::Rigid);
        };
        std::map<std::string, structure::StructureTemplatePool, std::less<>> all;
        all.emplace("minecraft:empty", structure::StructureTemplatePool{
                                           "minecraft:empty", {{structure::StructurePoolElement::empty(), 1}}});
        all.emplace("minecraft:bastion/starts",
                    structure::StructureTemplatePool{"minecraft:empty", {{single("minecraft:bastion/start"), 1}}});
        all.emplace("minecraft:bastion/wings",
                    structure::StructureTemplatePool{"minecraft:empty", {{single("minecraft:bastion/wing"), 1}}});
        return all;
    }();
    const auto found = pools.find(name);
    return found == pools.end() ? nullptr : &found->second;
}

/// A rough Nether floor that meets the lava sea, cheap enough that the portal replay finishes. Low
/// on purpose: a portal has to be able to land in lava and in a carved cave.
[[nodiscard]] NoiseRouter router(std::int64_t seed, double temperature, double vegetation) {
    const std::string density =
        R"({"type": "minecraft:add", "left": {"type": "minecraft:gradient", "axis": "y", "from_coordinate": 0, )"
        R"("to_coordinate": 64, "from_value": 1.0, "to_value": -1.0}, "right": {"type": "minecraft:mul", )"
        R"("left": 3.0, "right": )" +
        basalt::server::world::testsupport::noiseNode("minecraft:test", 0.4, 0.4) + "}}";
    return basalt::server::world::testsupport::syntheticRouter(
        seed, density,
        {.minY = 0,
         .height = 128,
         .seaLevel = 32,
         .defaultBlock = "minecraft:netherrack",
         .defaultFluid = "minecraft:lava",
         .fields = {{"temperature", std::to_string(temperature)}, {"vegetation", std::to_string(vegetation)}}});
}

/// The generator keeps its grids to itself, so the sweep rebuilds them to centre on.
[[nodiscard]] structure::StructurePlacementGrid complexGrid() {
    return structure::StructurePlacementGrid{27, 4, 30084232};
}

[[nodiscard]] structure::StructurePlacementGrid portalGrid() {
    return structure::StructurePlacementGrid{40, 15, 34222645};
}

/// The oracle folds names through `String.hashCode`, so the two must agree.
[[nodiscard]] std::int32_t javaHash(std::string_view text) {
    std::int32_t hash = 0;
    for (const char letter : text) {
        hash = static_cast<std::int32_t>(static_cast<std::uint32_t>(hash) * 31U +
                                         static_cast<std::uint32_t>(static_cast<unsigned char>(letter)));
    }
    return hash;
}

[[nodiscard]] std::tuple<std::int64_t, std::int32_t, std::size_t> fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t written = 0;
    for (std::int32_t y = 0; y < 128; ++y) {
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
        checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(javaHash(entity.type().path())))) *
                   1099511628211ULL;
    }
    for (const structure::StructureStartRecord& start : chunk.structures().starts()) {
        for (const std::int32_t bound : {start.fullBounds.minX, start.fullBounds.minY, start.fullBounds.minZ,
                                         start.fullBounds.maxX, start.fullBounds.maxY, start.fullBounds.maxZ}) {
            checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(bound))) * 1099511628211ULL;
        }
    }
    for (const ChunkScheduledTick& tick : chunk.scheduledFluidTicks()) {
        checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(tick.localX)) ^
                    (static_cast<std::uint64_t>(static_cast<std::int64_t>(tick.y)) << 8)) *
                   1099511628211ULL;
    }
    return {static_cast<std::int64_t>(checksum), written, chunk.structures().starts().size()};
}

struct Run {
    std::int32_t written = 0;
    std::size_t starts = 0;
    std::size_t blockEntities = 0;
    std::string biome;
    std::int64_t checksum = 0;
};

[[nodiscard]] Run run(std::int64_t seed, double temperature, double vegetation, std::string_view around) {
    const NoiseRouter netherRouter = router(seed, temperature, vegetation);
    const NetherBiomeSource biomeSource{netherRouter};
    NetherStructureGenerator generator{netherRouter, biomeSource, loader, poolLookup};
    std::pair<std::int32_t, std::int32_t> centre{0, 0};
    if (around == "complex") {
        centre = complexGrid().candidateChunk(seed, 0, 0);
    } else if (around == "portal") {
        centre = portalGrid().candidateChunk(seed, 0, 0);
    }
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t index = 0; index < 25; ++index) {
        Chunk chunk{centre.first + index % 5 - 2, centre.second + index / 5 - 2, kNetherHeight};
        generator.generateInto(chunk, seed);
        const auto [chunkChecksum, written, starts] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.written += written;
        result.starts += starts;
        result.blockEntities += chunk.blockEntities().size();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    result.biome = std::string{biomeSource.biomeAt(0, 64, 0)};
    const std::size_t colon = result.biome.find(':');
    if (colon != std::string::npos) {
        result.biome = result.biome.substr(colon + 1);
    }
    return result;
}

void check(const Run& result, std::int32_t written, std::size_t starts, std::size_t blockEntities,
           std::string_view biome, std::int64_t checksum) {
    CHECK(result.written == written);
    CHECK(result.starts == starts);
    CHECK(result.blockEntities == blockEntities);
    CHECK(result.biome == biome);
    CHECK(result.checksum == checksum);
}

} // namespace

TEST_CASE("the nether builds what the frozen kotlin builds") {
    // The fossils sit on their own two-chunk grid, so the origin already covers them.
    SUBCASE("fossils") {
        check(run(1234, -0.6, -0.6, "origin"), 147, 0, 0, "soul_sand_valley", -1000014710720569397LL);
    }

    // The fortress and the bastion share one cell, and which of the two it holds is a draw.
    SUBCASE("complexWastes") {
        check(run(1234, 0.0, 0.0, "complex"), 76935, 20, 3, "nether_wastes", 224194287535596918LL);
    }

    SUBCASE("complexDeltas") {
        check(run(1234, -0.6, 0.0, "complex"), 76935, 20, 3, "basalt_deltas", 224194287535596918LL);
    }

    SUBCASE("complexOther") { check(run(77, 0.0, 0.0, "complex"), 124, 0, 0, "nether_wastes", -2350128045204918792LL); }

    SUBCASE("complexThird") {
        check(run(9001, 0.0, 0.0, "complex"), 124, 0, 0, "nether_wastes", -8598750915188226532LL);
    }

    SUBCASE("complexFourth") {
        check(run(5, 0.0, 0.0, "complex"), 88118, 21, 4, "nether_wastes", 2828129272907334930LL);
    }

    SUBCASE("complexFifth") { check(run(13, 0.0, 0.0, "complex"), 124, 0, 0, "nether_wastes", -5353408625150024264LL); }

    SUBCASE("complexSixth") {
        check(run(21, 0.0, 0.0, "complex"), 53572, 17, 0, "nether_wastes", -3239511716844573407LL);
    }

    // Basalt deltas takes a fortress but never a bastion, which is the one biome gate here.
    SUBCASE("complexFourthDeltas") {
        check(run(5, -0.6, 0.0, "complex"), 88118, 21, 4, "basalt_deltas", 2828129272907334930LL);
    }

    SUBCASE("complexFifthDeltas") {
        check(run(13, -0.6, 0.0, "complex"), 0, 0, 0, "basalt_deltas", -2493704899546847768LL);
    }

    SUBCASE("complexSixthDeltas") {
        check(run(21, -0.6, 0.0, "complex"), 53572, 17, 0, "basalt_deltas", -3239511716844573407LL);
    }

    // The ruined portal replays one global placement from its own grid.
    SUBCASE("portal") { check(run(1234, 0.0, 0.0, "portal"), 338, 0, 1, "nether_wastes", 1328847200587368967LL); }

    SUBCASE("portalOther") { check(run(77, 0.4, 0.0, "portal"), 489, 0, 0, "crimson_forest", 335264574150313682LL); }
}
