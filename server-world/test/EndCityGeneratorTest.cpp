#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/SyntheticRouter.hpp"

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/EndCityStructureGenerator.hpp>
#include <basalt/server/world/terrain/NoiseRouterData.hpp>

#include <doctest.h>

#include <array>
#include <bit>
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
namespace df = basalt::server::world::densityfunction;

namespace {

/// Every template the recursion can ask for, in the order that sizes the stand-ins.
constexpr std::array<std::string_view, 19> kNames{
    "base_floor",    "base_roof",      "second_floor_1",   "second_floor_2",      "second_roof",
    "third_floor_1", "third_floor_2",  "third_roof",       "tower_base",          "tower_piece",
    "tower_top",     "bridge_end",     "bridge_piece",     "bridge_steep_stairs", "bridge_gentle_stairs",
    "ship",          "fat_tower_base", "fat_tower_middle", "fat_tower_top"};

[[nodiscard]] std::int32_t floorMod(std::int32_t value, std::int32_t divisor) {
    const std::int32_t rest = value % divisor;
    return rest < 0 ? rest + divisor : rest;
}

/// A stand-in city piece: purpur with a chest under a `Chest` marker and the two entity markers the
/// jar reads. Nothing here comes from a Mojang structure file.
[[nodiscard]] structure::StructureTemplate piece(std::int32_t index) {
    const std::int32_t sizeX = 9 + index % 4;
    const std::int32_t sizeY = 5 + index % 5;
    const std::int32_t sizeZ = 9 + index % 3;
    NbtList palette{NbtType::Compound};
    for (const std::string_view name :
         {"minecraft:purpur_block", "minecraft:chest", "minecraft:structure_block", "minecraft:air"}) {
        NbtCompound entry;
        entry.set("Name", NbtTag::ofString(std::string{name}));
        palette.push(NbtTag::ofCompound(std::move(entry)));
    }
    const auto markerAt = [](std::int32_t x, std::int32_t y, std::int32_t z) -> std::string_view {
        if (x == 2 && y == 2 && z == 2) {
            return "Chest";
        }
        if (x == 4 && y == 2 && z == 4) {
            return "Sentry";
        }
        return (x == 6 && y == 2 && z == 6) ? "Elytra" : std::string_view{};
    };
    NbtList blocks{NbtType::Compound};
    for (std::int32_t x = 0; x < sizeX; ++x) {
        for (std::int32_t y = 0; y < sizeY; ++y) {
            for (std::int32_t z = 0; z < sizeZ; ++z) {
                const std::string_view marker = markerAt(x, y, z);
                const bool chestHere = x == 2 && y == 1 && z == 2;
                NbtCompound block;
                block.set("pos", NbtTag::ofList(
                                     NbtList{NbtType::Int, {NbtTag::ofInt(x), NbtTag::ofInt(y), NbtTag::ofInt(z)}}));
                // A hollow shell, so `overwrite` decides whether the air is written.
                const bool hollow = y > 0 && x >= 1 && x < sizeX - 1 && z >= 1 && z < sizeZ - 1;
                block.set("state", NbtTag::ofInt(!marker.empty() ? 2 : chestHere ? 1 : hollow ? 3 : 0));
                if (!marker.empty()) {
                    NbtCompound data;
                    data.set("mode", NbtTag::ofString("DATA"));
                    data.set("metadata", NbtTag::ofString(std::string{marker}));
                    block.set("nbt", NbtTag::ofCompound(std::move(data)));
                }
                if (chestHere) {
                    NbtCompound data;
                    data.set("id", NbtTag::ofString("minecraft:chest"));
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

[[nodiscard]] const structure::StructureTemplate* loader(const basalt::wire::common::Identifier& name) {
    static const std::map<std::string, structure::StructureTemplate, std::less<>> built = [] {
        std::map<std::string, structure::StructureTemplate, std::less<>> all;
        for (std::size_t index = 0; index < kNames.size(); ++index) {
            all.emplace("minecraft:end_city/" + std::string{kNames[index]}, piece(static_cast<std::int32_t>(index)));
        }
        return all;
    }();
    const auto found = built.find(name.toString());
    return found == built.end() ? nullptr : &found->second;
}

[[nodiscard]] NoiseRouter router() { return basalt::server::world::testsupport::endRouter(0x5DEECE66DLL); }

[[nodiscard]] Chunk endChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 40; y <= 70; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
        }
    }
    return chunk;
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
    for (std::int32_t y = 40; y <= 140; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                const std::int32_t base =
                    (y >= 40 && y <= 70) ? registry::blocks::stone().stateId : registry::blocks::air().stateId;
                if (id != base) {
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
    for (const ChunkGeneratedEntity& entity : chunk.generatedEntities()) {
        checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(javaHash(entity.type.path())))) *
                   1099511628211ULL;
        for (const double value : {entity.x, entity.y, entity.z}) {
            checksum = (checksum ^ std::bit_cast<std::uint64_t>(value)) * 1099511628211ULL;
        }
        checksum = (checksum ^
                    static_cast<std::uint64_t>(static_cast<std::int64_t>(std::bit_cast<std::int32_t>(entity.yaw)))) *
                   1099511628211ULL;
    }
    return {static_cast<std::int64_t>(checksum), written, chunk.generatedEntities().size()};
}

struct Run {
    std::int32_t written = 0;
    std::size_t entities = 0;
    std::size_t pieces = 0;
    std::int64_t checksum = 0;
};

[[nodiscard]] Run run(std::int64_t seed, std::int32_t aroundChunkX, std::int32_t aroundChunkZ, std::int32_t surface) {
    const NoiseRouter end = router();
    const TheEndBiomeSource biomeSource{end};
    EndCityStructureGenerator generator{
        end, biomeSource, loader,
        // A sloped shelf, so which of the four corners the start
        // takes is observable.
        [surface](std::int32_t x, std::int32_t z) { return surface + floorMod(x + z, 4); }};
    const auto centre = EndCityStructureGenerator::grid().candidateChunk(seed, aroundChunkX, aroundChunkZ);
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    // Twelve chunks across, so a candidate can sit further from a chunk than half the reference
    // range: with a narrower sweep, halving that range would change nothing.
    for (std::int32_t index = 0; index < 144; ++index) {
        Chunk chunk = endChunk(centre.first + index % 12 - 6, centre.second + index / 12 - 6);
        generator.generateInto(chunk, seed);
        const auto [chunkChecksum, written, entities] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.written += written;
        result.entities += entities;
    }
    result.pieces = generator.cityAt(seed, centre.first, centre.second).size();
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

void check(const Run& result, std::int32_t written, std::size_t entities, std::size_t pieces, std::int64_t checksum) {
    CHECK(result.written == written);
    CHECK(result.entities == entities);
    CHECK(result.pieces == pieces);
    CHECK(result.checksum == checksum);
}

} // namespace

TEST_CASE("the end cities grow where the frozen kotlin grows them") {
    SUBCASE("highlands") { check(run(1234, 80, 60, 70), 6439, 66, 33, -5993109918369013086LL); }

    SUBCASE("midlands") { check(run(1234, 60, 60, 70), 15322, 146, 73, -7475852285910694705LL); }

    // Too low for a city to start at all, whatever the biome says.
    SUBCASE("low") { check(run(1234, 80, 60, 40), 0, 0, 0, -4086841023705650507LL); }

    // The central island is `the_end`, which no city takes.
    SUBCASE("centre") { check(run(1234, 0, 0, 70), 0, 0, 0, -4086841023705650507LL); }

    SUBCASE("otherSeed") { check(run(1234, 120, 120, 70), 8558, 86, 43, 5425275140771176155LL); }

    SUBCASE("thirdSeed") { check(run(1234, 120, 140, 70), 1920, 16, 8, 7587113362192412219LL); }

    // Small end islands, which no city takes either.
    SUBCASE("islands") { check(run(1234, 3200, 3200, 70), 0, 0, 0, -4086841023705650507LL); }
}
