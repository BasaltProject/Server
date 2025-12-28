#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/noise/NormalNoise.hpp>
#include <basalt/server/world/terrain/JigsawStructureGenerator.hpp>

#include <doctest.h>

#include <array>
#include <bit>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
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
    std::string pool;
    std::string_view target;
};

constexpr std::array<std::string_view, 6> kOrientations{"north_up", "south_up", "west_up",
                                                        "east_up",  "up_north", "down_north"};

/// A synthetic piece: a box of stone with a chest and one jigsaw block per connector. Nothing here
/// comes from a Mojang structure file; what is under test is the generator, not the buildings.
[[nodiscard]] structure::StructureTemplate piece(std::int32_t sizeX, std::int32_t sizeY, std::int32_t sizeZ,
                                                 const std::vector<Connector>& connectors) {
    NbtList palette{NbtType::Compound};
    for (const std::string_view name : {"minecraft:deepslate_bricks", "minecraft:chest"}) {
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
                // One chest per piece, whose loot seed comes off the feature stream: without it
                // nothing a piece places depends on that stream at all.
                const bool chestHere = x == 1 && y == 0 && z == 1;
                block.set("state", NbtTag::ofInt(chestHere ? 1 : 0));
                if (chestHere) {
                    NbtCompound data;
                    data.set("id", NbtTag::ofString("minecraft:chest"));
                    data.set("LootTable", NbtTag::ofString("minecraft:chests/deep"));
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

using Templates = std::map<std::string, structure::StructureTemplate, std::less<>>;
using Pools = std::map<std::string, structure::StructureTemplatePool, std::less<>>;

/// One family of pieces per structure, all the same shape so the three cases differ only in the
/// settings under test. The anchor is the jigsaw an assembly may be told to start on.
[[nodiscard]] Templates family(std::string_view prefix, std::string_view anchor) {
    const std::string rooms = std::string{prefix} + "/rooms";
    Templates all;
    all.emplace(std::string{prefix} + "/start", piece(9, 5, 9,
                                                      {Connector{0, 1, 4, "west_up", anchor, rooms, "room"},
                                                       Connector{8, 1, 4, "east_up", "east", rooms, "room"},
                                                       Connector{4, 1, 0, "north_up", "north", rooms, "room"},
                                                       Connector{4, 1, 8, "south_up", "south", rooms, "room"}}));
    // Seven links of seven blocks reach further than half the allowed radius but not all of it, so
    // how deep the assembly may go and how far it may reach both bite.
    all.emplace(std::string{prefix} + "/room", piece(7, 5, 7,
                                                     {Connector{0, 1, 3, "west_up", "room", "minecraft:empty", "empty"},
                                                      Connector{6, 1, 3, "east_up", "room_out", rooms, "room"}}));
    all.emplace(std::string{prefix} + "/cell",
                piece(5, 4, 5, {Connector{0, 1, 2, "west_up", "room", "minecraft:empty", "empty"}}));
    return all;
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

[[nodiscard]] Pools poolsOf(std::string_view prefix, std::string_view startPool) {
    Pools all;
    all.emplace("minecraft:empty",
                structure::StructureTemplatePool{"minecraft:empty", {{structure::StructurePoolElement::empty(), 1}}});
    all.emplace(std::string{startPool},
                structure::StructureTemplatePool{"minecraft:empty", {{single(std::string{prefix} + "/start"), 1}}});
    all.emplace(
        std::string{prefix} + "/rooms",
        structure::StructureTemplatePool{
            "minecraft:empty",
            {{single(std::string{prefix} + "/room"), 3},
             // The one piece that is neither rigid nor whole: it settles on the chunk's own
             // heightmap, the beard skips it, and the rot makes the placement draw from the
             // feature stream.
             {single(std::string{prefix} + "/cell", structure::StructureProjection::TerrainMatching, 0.7F), 1}}});
    return all;
}

/// A rolling surface around y 60, so which column a height is read from is observable.
[[nodiscard]] OverworldNoiseRouter router(std::int64_t seed) {
    const df::Function zero = df::functions::zero();
    basalt::server::random::RandomSource source = basalt::server::random::RandomSource::xoroshiro(seed);
    static constexpr std::array<double, 2> kAmplitudes{1.0, 1.0};
    auto built = std::make_shared<const noise::NormalNoise>(noise::NormalNoise::create(source, -6, kAmplitudes));
    const df::Function density =
        df::functions::add(df::functions::yClampedGradient(50, 78, 1.0, -1.0),
                           df::functions::mul(df::functions::constant(2.0), df::functions::noise(built, 1.0, 0.0)));
    return OverworldNoiseRouter{.barrierNoise = zero,
                                .fluidLevelFloodednessNoise = zero,
                                .fluidLevelSpreadNoise = zero,
                                .lavaNoise = zero,
                                .shiftX = zero,
                                .shiftZ = zero,
                                .temperature = zero,
                                .vegetation = zero,
                                .continents = zero,
                                .erosion = zero,
                                .depth = zero,
                                .ridges = zero,
                                .preliminarySurfaceLevel = zero,
                                .finalDensity = density,
                                .veinToggle = zero,
                                .veinRidged = zero,
                                .veinGap = zero};
}

/// The same surface walk the generators do, so a biome can be pinned to it.
[[nodiscard]] std::int32_t surfaceOf(std::int64_t seed, std::int32_t x, std::int32_t z) {
    DensityChunkSampler sampler{router(seed).finalDensity, x >> 4, z >> 4, -64, 384, 4, 8};
    std::int32_t y = 319;
    while (y >= -64 && sampler.sample(x & 15, y, z & 15) <= 0.0) {
        --y;
    }
    return std::max(y + 1, 63);
}

[[nodiscard]] Chunk stoneChunk(std::int32_t chunkX, std::int32_t chunkZ, std::int32_t lowY, std::int32_t terrainTop) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = lowY; y <= terrainTop; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk, std::int32_t lowY, std::int32_t highY) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t written = 0;
    for (std::int32_t y = lowY; y <= highY; ++y) {
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

struct Case {
    std::string_view prefix;
    std::string_view anchor;
    std::string_view startPool;
    std::int32_t lowY = 0;
    std::int32_t highY = 0;
    std::int32_t terrainTop = 0;
    /// The y the biome probe answers at, or nothing when it reads the surface instead.
    std::optional<std::int32_t> probeY;
    bool searches = false;
};

[[nodiscard]] Run run(const Case& shape, const JigsawStructureSettings& settings,
                      const structure::StructurePlacementGrid& grid, std::int64_t seed, std::string_view biome,
                      std::set<std::string, std::less<>> biomes) {
    const auto templates = std::make_shared<const Templates>(family(shape.prefix, shape.anchor));
    const auto pools = std::make_shared<const Pools>(poolsOf(shape.prefix, shape.startPool));
    const auto loader = [templates](std::string_view name) -> const structure::StructureTemplate* {
        const auto found = templates->find(name);
        return found == templates->end() ? nullptr : &found->second;
    };
    const auto poolLookup = [pools](std::string_view name) -> const structure::StructureTemplatePool* {
        const auto found = pools->find(name);
        return found == pools->end() ? nullptr : &found->second;
    };
    const std::string biomeName{biome};
    const std::optional<std::int32_t> probeY = shape.probeY;
    JigsawStructureGenerator generator{
        settings,
        router(seed),
        grid,
        [seed, biomeName, probeY](std::int32_t x, std::int32_t y, std::int32_t z) -> std::string {
            if ((x & 15) != 8 || (z & 15) != 8) {
                return "minecraft:ocean";
            }
            const std::int32_t wanted = probeY.value_or(surfaceOf(seed, x, z));
            return y == wanted ? biomeName : std::string{"minecraft:ocean"};
        },
        std::move(biomes),
        poolLookup,
        loader};

    Run result;
    std::optional<std::pair<std::int32_t, std::int32_t>> found;
    if (shape.searches) {
        found = generator.locate(seed, 0, 0, 6);
    }
    const std::pair<std::int32_t, std::int32_t> centre = found.value_or(grid.candidateChunk(seed, 0, 0));
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t index = 0; index < 36; ++index) {
        const std::int32_t chunkX = centre.first + index % 6 - 3;
        const std::int32_t chunkZ = centre.second + index / 6 - 3;
        Chunk chunk = stoneChunk(chunkX, chunkZ, shape.lowY, shape.terrainTop);
        generator.generateInto(chunk, chunkX, chunkZ, seed);
        const auto [chunkChecksum, written] = fold(chunk, shape.lowY, shape.highY);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.written += written;
        result.starts += chunk.structures().starts().size();
    }
    const df::Function beard = generator.beardified(df::functions::zero(), centre.first, centre.second, seed);
    for (std::int32_t y = shape.lowY; y <= shape.highY; y += 4) {
        for (std::int32_t x = 0; x < 16; x += 4) {
            for (std::int32_t z = 0; z < 16; z += 4) {
                df::FunctionContext context{centre.first * 16 + x, y, centre.second * 16 + z};
                checksum = (checksum ^ std::bit_cast<std::uint64_t>(beard->compute(context))) * 1099511628211ULL;
            }
        }
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    if (!shape.searches) {
        result.located = "none";
    } else {
        result.located = found.has_value() ? std::to_string(found->first) + "," + std::to_string(found->second)
                                           : std::string{"null,null"};
    }
    const auto [cached, cachedPieces] = generator.assemblyStats();
    result.cached = cached;
    result.cachedPieces = cachedPieces;
    return result;
}

[[nodiscard]] const Case& trailRuinsCase() {
    static const Case shape{.prefix = "minecraft:trail_ruins",
                            .anchor = "anchor",
                            .startPool = "minecraft:trail_ruins/tower",
                            .lowY = 40,
                            .highY = 110,
                            .terrainTop = 62,
                            .searches = true};
    return shape;
}

[[nodiscard]] const Case& ancientCityCase() {
    static const Case shape{.prefix = "minecraft:ancient_city",
                            .anchor = "minecraft:city_anchor",
                            .startPool = "minecraft:ancient_city/city_center",
                            .lowY = -50,
                            .highY = 0,
                            .terrainTop = -10,
                            .probeY = -27};
    return shape;
}

[[nodiscard]] const Case& trialChamberCase() {
    static const Case shape{.prefix = "minecraft:trial_chambers",
                            .anchor = "anchor",
                            .startPool = "minecraft:trial_chambers/chamber/end",
                            .lowY = -60,
                            .highY = -5,
                            .terrainTop = -15,
                            .probeY = -40};
    return shape;
}

[[nodiscard]] Run runRuins(std::int64_t seed, std::string_view biome) {
    return run(trailRuinsCase(), jigsawStructures::trailRuins(), jigsawStructures::trailRuinsGrid(), seed, biome,
               {"minecraft:jungle", "minecraft:taiga"});
}

[[nodiscard]] Run runCity(std::int64_t seed, std::string_view biome) {
    return run(ancientCityCase(), jigsawStructures::ancientCity(), jigsawStructures::ancientCityGrid(), seed, biome,
               {"minecraft:deep_dark"});
}

[[nodiscard]] Run runChamber(std::int64_t seed, std::string_view biome) {
    return run(trialChamberCase(), jigsawStructures::trialChamber(), jigsawStructures::trialChamberGrid(), seed, biome,
               {"minecraft:dripstone_caves", "minecraft:lush_caves"});
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

TEST_CASE("the deep jigsaw structures assemble where the frozen kotlin assembles them") {
    SUBCASE("ruins") { check(runRuins(1234, "minecraft:jungle"), 5022, 11, 6646192249432615265LL, "19,23", 1, 22); }

    SUBCASE("ruinsElsewhere") { check(runRuins(77, "minecraft:taiga"), 3899, 9, 4192801240433696798LL, "9,23", 1, 19); }

    SUBCASE("noRuins") { check(runRuins(1234, "minecraft:ocean"), 0, 0, -1045953369363326287LL, "null,null", 10, 0); }

    SUBCASE("city") { check(runCity(1234, "minecraft:deep_dark"), 2860, 7, -7098982429302431984LL, "none", 1, 14); }

    SUBCASE("cityElsewhere") {
        check(runCity(77, "minecraft:deep_dark"), 3951, 12, 6839323969532419222LL, "none", 1, 18);
    }

    SUBCASE("noCity") { check(runCity(1234, "minecraft:ocean"), 0, 0, -6294788893584798351LL, "none", 1, 0); }

    // The chamber records its start like every other jigsaw structure, which is what the jar does and
    // what the server has always done. The zero these two used to expect came from a settings object
    // that only the tests ever read.
    SUBCASE("chamber") {
        check(runChamber(1234, "minecraft:dripstone_caves"), 2976, 8, -7428120839514114081LL, "none", 1, 15);
    }

    SUBCASE("chamberElsewhere") {
        check(runChamber(77, "minecraft:lush_caves"), 3460, 7, 5873367261463590498LL, "none", 1, 19);
    }

    SUBCASE("noChamber") { check(runChamber(1234, "minecraft:ocean"), 0, 0, -3883247817186501711LL, "none", 1, 0); }
}
