#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/WoodlandMansionStructureGenerator.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain;
using basalt::wire::nbt::NbtCompound;
using basalt::wire::nbt::NbtList;
using basalt::wire::nbt::NbtTag;
using basalt::wire::nbt::NbtType;
namespace registry = basalt::server::registry;

namespace {

/// A stand-in mansion room: planks with one structure block carrying a marker. No Mojang asset goes
/// near the repository, and what is under test is the placing, not the architecture.
[[nodiscard]] structure::StructureTemplate roomTemplate(std::string_view metadata) {
    NbtList palette{NbtType::Compound};
    {
        NbtCompound planks;
        planks.set("Name", NbtTag::ofString("minecraft:oak_planks"));
        palette.push(NbtTag::ofCompound(std::move(planks)));
        NbtCompound marker;
        marker.set("Name", NbtTag::ofString("minecraft:structure_block"));
        palette.push(NbtTag::ofCompound(std::move(marker)));
    }
    NbtList blocks{NbtType::Compound};
    for (std::int32_t x = 0; x < 7; ++x) {
        for (std::int32_t y = 0; y < 6; ++y) {
            for (std::int32_t z = 0; z < 7; ++z) {
                const bool isMarker = x == 3 && y == 1 && z == 3;
                NbtCompound block;
                block.set("pos", NbtTag::ofList(
                                     NbtList{NbtType::Int, {NbtTag::ofInt(x), NbtTag::ofInt(y), NbtTag::ofInt(z)}}));
                block.set("state", NbtTag::ofInt(isMarker ? 1 : 0));
                if (isMarker) {
                    NbtCompound data;
                    data.set("mode", NbtTag::ofString("DATA"));
                    data.set("metadata", NbtTag::ofString(std::string{metadata}));
                    block.set("nbt", NbtTag::ofCompound(std::move(data)));
                }
                blocks.push(NbtTag::ofCompound(std::move(block)));
            }
        }
    }
    NbtCompound nbt;
    nbt.set("size", NbtTag::ofList(NbtList{NbtType::Int, {NbtTag::ofInt(7), NbtTag::ofInt(6), NbtTag::ofInt(7)}}));
    nbt.set("palette", NbtTag::ofList(std::move(palette)));
    nbt.set("blocks", NbtTag::ofList(std::move(blocks)));
    structure::StructureTemplate built;
    built.load(nbt);
    return built;
}

/// Every template answers, and the marker depends on the name so the four kinds all appear.
[[nodiscard]] const structure::StructureTemplate* templateOf(std::string_view name) {
    static std::map<std::string, structure::StructureTemplate, std::less<>> held;
    const auto found = held.find(name);
    if (found != held.end()) {
        return &found->second;
    }
    std::string_view metadata = "ChestEast";
    if (name.ends_with("entrance")) {
        metadata = "ChestWest";
    } else if (name.find("1x1_a") != std::string_view::npos) {
        metadata = "Mage";
    } else if (name.find("1x2_a") != std::string_view::npos) {
        metadata = "Warrior";
    } else if (name.find("2x2_a") != std::string_view::npos) {
        metadata = "Group of Allays";
    } else if (name.find("corridor") != std::string_view::npos || name.find("carpet") != std::string_view::npos) {
        // A chest on the common pieces, so the facing turns with the house; the corridors keep the
        // marker that only leaves air behind.
        metadata = "";
    }
    return &held.emplace(std::string{name}, roomTemplate(metadata)).first->second;
}

/// Stone under a lake: the foundation has to walk down through the water to reach the rock.
[[nodiscard]] Chunk groundChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = -8; y <= 59; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            for (std::int32_t y = 60; y <= 69; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::water().stateId);
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::tuple<std::int64_t, std::int32_t, std::size_t> fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t written = 0;
    for (std::int32_t y = -8; y <= 110; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                if (id != registry::blocks::stone().stateId && id != registry::blocks::water().stateId &&
                    id != registry::blocks::air().stateId) {
                    ++written;
                }
            }
        }
    }
    return {static_cast<std::int64_t>(checksum), written, chunk.blockEntities().size()};
}

struct Run {
    std::int32_t written = 0;
    std::size_t blockEntities = 0;
    std::size_t entities = 0;
    std::size_t starts = 0;
    std::int64_t checksum = 0;
};

[[nodiscard]] Run run(std::int64_t seed, std::string_view biome, std::int32_t surface, std::int32_t chunks) {
    const std::string biomeName{biome};
    WoodlandMansionStructureGenerator generator{
        [biomeName](std::int32_t, std::int32_t, std::int32_t) { return biomeName; },
        {"minecraft:dark_forest"},
        templateOf,
        // A gentle slope, so the four corners of the 5x5 probe box disagree and which offsets it
        // uses is observable.
        [surface](std::int32_t x, std::int32_t z) {
            const std::int32_t rest = (x + z) % 8;
            return surface + (rest < 0 ? rest + 8 : rest);
        }};
    const auto centre = WoodlandMansionStructureGenerator::grid().candidateChunk(seed, 0, 0);
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t index = 0; index < chunks; ++index) {
        // Offset so the window reaches seven chunks out on one side, which is the mansion's own
        // reach: a shorter reach would drop those chunks.
        Chunk chunk = groundChunk(centre.first + index % 10 - 7, centre.second + index / 10 - 7);
        generator.generateInto(chunk, seed);
        const auto [chunkChecksum, written, blockEntities] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.written += written;
        result.blockEntities += blockEntities;
        result.entities += chunk.generatedEntities().size();
        result.starts += chunk.structures().starts().size();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

void check(const Run& result, std::int32_t written, std::size_t blockEntities, std::size_t entities, std::size_t starts,
           std::int64_t checksum) {
    CHECK(result.written == written);
    CHECK(result.blockEntities == blockEntities);
    CHECK(result.entities == entities);
    CHECK(result.starts == starts);
    CHECK(result.checksum == checksum);
}

} // namespace

TEST_CASE("the woodland mansions stand where the frozen kotlin puts them") {
    SUBCASE("darkForest") {
        check(run(555, "minecraft:dark_forest", 75, 100), 151924, 227, 15, 24, -8492544333590560280LL);
    }

    SUBCASE("plains") { check(run(555, "minecraft:plains", 75, 100), 0, 0, 0, 0, -7239569130427570831LL); }

    SUBCASE("lowGround") { check(run(555, "minecraft:dark_forest", 50, 100), 0, 0, 0, 0, -7239569130427570831LL); }

    SUBCASE("otherSeed") {
        check(run(31, "minecraft:dark_forest", 75, 100), 108934, 172, 9, 20, 6825058404666269365LL);
    }
}
