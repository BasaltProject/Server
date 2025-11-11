#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/RuinedPortalStructureGenerator.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
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

[[nodiscard]] std::int32_t floorMod(std::int32_t value, std::int32_t divisor) {
    const std::int32_t rest = value % divisor;
    return rest < 0 ? rest + divisor : rest;
}

[[nodiscard]] const std::vector<std::string>& portalNames() {
    static const std::vector<std::string> built = [] {
        std::vector<std::string> all;
        for (std::int32_t index = 1; index <= 10; ++index) {
            all.push_back("ruined_portal/portal_" + std::to_string(index));
        }
        for (std::int32_t index = 1; index <= 3; ++index) {
            all.push_back("ruined_portal/giant_portal_" + std::to_string(index));
        }
        return all;
    }();
    return built;
}

[[nodiscard]] std::int32_t state(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    REQUIRE(found != nullptr);
    return found->stateId;
}

/// A stand-in portal frame: obsidian round a hollow, a structure void where the air pocket shows,
/// and a slab of stone brick the ager can rot. Nothing here comes from a Mojang structure file.
[[nodiscard]] structure::StructureTemplate frame(std::int32_t index) {
    const std::int32_t sizeX = 5 + index % 4;
    const std::int32_t sizeY = 5 + index % 3;
    const std::int32_t sizeZ = 3 + index % 2;
    NbtList palette{NbtType::Compound};
    for (const std::string_view name :
         {"minecraft:obsidian", "minecraft:stone_bricks", "minecraft:structure_void", "minecraft:air", "minecraft:lava",
          // Cobblestone is what the blackstone swap looks for and the ager leaves alone.
          "minecraft:cobblestone",
          // A slab is not a full block, so lava flows into its cell instead of it.
          "minecraft:stone_brick_slab"}) {
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
                const bool edge = x == 0 || x == sizeX - 1 || y == 0 || y == sizeY - 1;
                const std::int32_t stateIndex = (edge && (x + y + z) % 3 == 0)   ? 1
                                                : (edge && (x + y + z) % 3 == 1) ? 5
                                                : (edge && z == sizeZ - 1)       ? 6
                                                : edge                           ? 0
                                                : z == 0                         ? 2
                                                : (y == 1 && x == 2)             ? 4
                                                                                 : 3;
                block.set("state", NbtTag::ofInt(stateIndex));
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
    static const std::map<std::string, structure::StructureTemplate, std::less<>> built = [] {
        std::map<std::string, structure::StructureTemplate, std::less<>> all;
        for (std::size_t index = 0; index < portalNames().size(); ++index) {
            all.emplace(portalNames()[index], frame(static_cast<std::int32_t>(index)));
        }
        return all;
    }();
    const auto found = built.find(name);
    return found == built.end() ? nullptr : &found->second;
}

/// The seven variants, each with the placements and trimmings its real counterpart has.
[[nodiscard]] std::vector<RuinedPortalVariant> variants() {
    return {
        RuinedPortalVariant{"ruined_portal",
                            {"minecraft:plains", "minecraft:forest"},
                            {RuinedPortalSetup{.placement = RuinedPortalPlacement::OnLandSurface,
                                               .mossiness = 0.2F,
                                               .canBeCold = true,
                                               .weight = 0.8F},
                             // Deliberately unlike the first: which of the two is drawn has to show.
                             RuinedPortalSetup{.placement = RuinedPortalPlacement::PartlyBuried,
                                               .mossiness = 0.9F,
                                               .overgrown = true,
                                               .vines = true,
                                               .replaceWithBlackstone = true,
                                               .weight = 0.2F}}},
        RuinedPortalVariant{"ruined_portal_desert",
                            {"minecraft:desert"},
                            {RuinedPortalSetup{.placement = RuinedPortalPlacement::PartlyBuried, .weight = 1.0F}}},
        RuinedPortalVariant{"ruined_portal_jungle",
                            {"minecraft:jungle"},
                            {RuinedPortalSetup{.placement = RuinedPortalPlacement::OnLandSurface,
                                               .airPocketProbability = 0.5F,
                                               .mossiness = 0.8F,
                                               .overgrown = true,
                                               .vines = true,
                                               .weight = 1.0F}}},
        RuinedPortalVariant{"ruined_portal_mountain",
                            {"minecraft:snowy_slopes", "minecraft:grove"},
                            {RuinedPortalSetup{.placement = RuinedPortalPlacement::InMountain,
                                               .airPocketProbability = 0.5F,
                                               .mossiness = 0.2F,
                                               .canBeCold = true,
                                               .weight = 1.0F}}},
        RuinedPortalVariant{"ruined_portal_nether",
                            {"minecraft:nether_wastes"},
                            {RuinedPortalSetup{.placement = RuinedPortalPlacement::InNether,
                                               .airPocketProbability = 0.5F,
                                               .replaceWithBlackstone = true,
                                               .weight = 1.0F}}},
        RuinedPortalVariant{
            "ruined_portal_ocean",
            {"minecraft:ocean"},
            {RuinedPortalSetup{.placement = RuinedPortalPlacement::OnOceanFloor, .mossiness = 0.8F, .weight = 1.0F}}},
        RuinedPortalVariant{"ruined_portal_swamp",
                            {"minecraft:swamp"},
                            {RuinedPortalSetup{.placement = RuinedPortalPlacement::Underground,
                                               .airPocketProbability = 0.5F,
                                               .mossiness = 0.5F,
                                               .vines = true,
                                               .weight = 1.0F}}},
    };
}

[[nodiscard]] std::int32_t surfaceAt(std::int32_t x, std::int32_t z) { return 70 + floorMod(x * 2 + z * 3, 6); }

[[nodiscard]] std::int32_t oceanFloorAt(std::int32_t x, std::int32_t z) { return 52 + floorMod(x + z * 2, 4); }

/// Lava lakes that reach the surface, so a frame can land inside one: that is the only shape in
/// which the submerged processor and the lava branch of the apron have anything to act on.
[[nodiscard]] bool lavaColumn(std::int32_t x, std::int32_t z) { return floorMod(x, 9) < 5 && floorMod(z, 11) < 5; }

[[nodiscard]] std::int32_t terrainAt(std::int32_t x, std::int32_t y, std::int32_t z) {
    static const std::int32_t water = state("water");
    static const std::int32_t lava = state("lava");
    if (lavaColumn(x, z) && y < surfaceAt(x, z)) {
        return lava;
    }
    if (y < oceanFloorAt(x, z)) {
        return registry::blocks::stone().stateId;
    }
    return y < surfaceAt(x, z) ? water : registry::blocks::air().stateId;
}

[[nodiscard]] Chunk portalChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 20; y <= 100; ++y) {
                chunk.setBlockStateId(x, y, z, terrainAt(chunkX * 16 + x, y, chunkZ * 16 + z));
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::tuple<std::int64_t, std::int32_t, std::size_t> fold(Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t written = 0;
    for (std::int32_t y = 20; y <= 110; ++y) {
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
    const std::vector<ChunkGeneratedBlockWrite> deferred = chunk.takeGeneratedBlockWrites();
    for (const ChunkGeneratedBlockWrite& write : deferred) {
        checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(write.x)) ^
                    (static_cast<std::uint64_t>(static_cast<std::int64_t>(write.y)) << 20) ^
                    (static_cast<std::uint64_t>(static_cast<std::int64_t>(write.z)) << 40)) *
                   1099511628211ULL;
        checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(write.stateId))) * 1099511628211ULL;
    }
    return {static_cast<std::int64_t>(checksum), written, deferred.size()};
}

/// The generator keeps its grid to itself, so the sweep rebuilds the same one to centre on.
[[nodiscard]] structure::StructurePlacementGrid grid() { return structure::StructurePlacementGrid{40, 15, 34222645}; }

struct Run {
    std::int32_t written = 0;
    std::size_t deferred = 0;
    std::int64_t checksum = 0;
};

[[nodiscard]] Run run(std::int64_t seed, std::string_view biome) {
    const std::string biomeName{biome};
    RuinedPortalStructureGenerator generator{
        variants(), templateOf,
        // Only the column the portal starts on answers, so where the biome is read matters.
        [biomeName](std::int32_t x, std::int32_t, std::int32_t z) -> std::string {
            return (x & 15) == 0 && (z & 15) == 0 ? biomeName : std::string{"minecraft:the_void"};
        },
        surfaceAt, oceanFloorAt, [](std::int32_t x, std::int32_t y, std::int32_t z) { return y < oceanFloorAt(x, z); }};
    const auto centre = grid().candidateChunk(seed, 0, 0);
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t index = 0; index < 64; ++index) {
        Chunk chunk = portalChunk(centre.first + index % 8 - 4, centre.second + index / 8 - 4);
        generator.generateInto(chunk, seed);
        const auto [chunkChecksum, written, deferred] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.written += written;
        result.deferred += deferred;
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

void check(const Run& result, std::int32_t written, std::size_t deferred, std::int64_t checksum) {
    CHECK(result.written == written);
    CHECK(result.deferred == deferred);
    CHECK(result.checksum == checksum);
}

} // namespace

TEST_CASE("the ruined portals settle where the frozen kotlin settles them") {
    SUBCASE("plains") { check(run(1234, "minecraft:plains"), 91, 0, -5353366049642127048LL); }

    SUBCASE("desert") { check(run(1234, "minecraft:desert"), 272, 0, 6268340605083905118LL); }

    SUBCASE("jungle") { check(run(1234, "minecraft:jungle"), 142, 0, 6190134296698152711LL); }

    SUBCASE("mountain") { check(run(1234, "minecraft:snowy_slopes"), 444, 0, -9098698840730127577LL); }

    SUBCASE("nether") { check(run(1234, "minecraft:nether_wastes"), 430, 0, 1523947682084561741LL); }

    SUBCASE("ocean") { check(run(1234, "minecraft:ocean"), 297, 0, -1172833535802672086LL); }

    SUBCASE("swamp") { check(run(1234, "minecraft:swamp"), 317, 0, -5526998960708213934LL); }

    SUBCASE("none") { check(run(1234, "minecraft:the_void"), 0, 0, -6940171599639708799LL); }

    SUBCASE("otherSeed") { check(run(77, "minecraft:jungle"), 128, 0, -677917400889296606LL); }

    // More plains, which is the one variant with two setups to draw between.
    SUBCASE("plainsOther") { check(run(77, "minecraft:plains"), 83, 0, -6402515342726521637LL); }

    SUBCASE("plainsThird") { check(run(9001, "minecraft:plains"), 228, 0, 5120646109316553337LL); }

    SUBCASE("plainsFourth") { check(run(4321, "minecraft:plains"), 92, 0, 1810545395592424294LL); }

    SUBCASE("plainsFifth") { check(run(5, "minecraft:plains"), 48, 0, 3943902589117980866LL); }

    SUBCASE("plainsSixth") { check(run(13, "minecraft:plains"), 62, 0, 3044532325320702344LL); }

    SUBCASE("plainsSeventh") { check(run(21, "minecraft:plains"), 88, 0, 1219394395355481428LL); }

    SUBCASE("plainsEighth") { check(run(34, "minecraft:plains"), 90, 0, 8185573560323184425LL); }
}
