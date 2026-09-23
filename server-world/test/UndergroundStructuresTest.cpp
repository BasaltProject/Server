#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/structure/StructureTemplate.hpp>
#include <basalt/server/world/terrain/decoration/OverworldUndergroundStructures.hpp>

#include <doctest.h>

#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <utility>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using basalt::server::world::testsupport::FixedBiomeEnvironment;
using basalt::server::world::testsupport::stateIdOf;
using basalt::wire::nbt::NbtCompound;
using basalt::wire::nbt::NbtList;
using basalt::wire::nbt::NbtTag;
using basalt::wire::nbt::NbtType;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t floorMod(std::int32_t value, std::int32_t modulus) {
    const std::int32_t rest = value % modulus;
    return rest < 0 ? rest + modulus : rest;
}

/// A stand-in fossil template: bone blocks in a line, and coal ore in the overlay. No Mojang asset
/// goes near the repository, and what is under test is the burial, not the bones.
[[nodiscard]] structure::StructureTemplate fossilTemplate(bool overlay) {
    NbtCompound entry;
    entry.set("Name", NbtTag::ofString(overlay ? "minecraft:coal_ore" : "minecraft:bone_block"));
    NbtList palette{NbtType::Compound};
    palette.push(NbtTag::ofCompound(std::move(entry)));
    NbtList blocks{NbtType::Compound};
    for (std::int32_t x = 0; x < 5; ++x) {
        for (std::int32_t y = 0; y < 2; ++y) {
            for (std::int32_t z = 0; z < 3; ++z) {
                NbtCompound block;
                block.set("pos", NbtTag::ofList(
                                     NbtList{NbtType::Int, {NbtTag::ofInt(x), NbtTag::ofInt(y), NbtTag::ofInt(z)}}));
                block.set("state", NbtTag::ofInt(0));
                blocks.push(NbtTag::ofCompound(std::move(block)));
            }
        }
    }
    NbtCompound nbt;
    nbt.set("size", NbtTag::ofList(NbtList{NbtType::Int, {NbtTag::ofInt(5), NbtTag::ofInt(2), NbtTag::ofInt(3)}}));
    nbt.set("palette", NbtTag::ofList(std::move(palette)));
    nbt.set("blocks", NbtTag::ofList(std::move(blocks)));
    structure::StructureTemplate built;
    built.load(nbt);
    return built;
}

[[nodiscard]] Chunk stoneChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = -64; y <= 60; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            // Narrow slots rather than an open cavern: a monster room needs a solid floor and
            // ceiling and between one and five openings in its walls, and a hall the width of the
            // chunk gives it far more than five.
            for (std::int32_t y = -56; y <= 56; ++y) {
                const bool slot = floorMod(y, 8) < 2;
                if (slot && (floorMod(x, 8) == 0 || floorMod(z, 8) == 4)) {
                    chunk.setBlockStateId(x, y, z, registry::blocks::air().stateId);
                }
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk) {
    static const std::set<std::string, std::less<>> counted = {
        "bone_block", "coal_ore", "deepslate_diamond_ore", "spawner", "mossy_cobblestone", "chest"};
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t placed = 0;
    for (std::int32_t y = -64; y <= 68; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                const basalt::wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(id);
                if (block != nullptr && counted.contains(block->path())) {
                    ++placed;
                }
            }
        }
    }
    return {static_cast<std::int64_t>(checksum), placed};
}

struct Run {
    std::int32_t placed = 0;
    std::size_t deferred = 0;
    std::int64_t checksum = 0;
};

[[nodiscard]] Run run(std::string_view biome, bool withTemplates) {
    static const structure::StructureTemplate base = fossilTemplate(false);
    static const structure::StructureTemplate overlay = fossilTemplate(true);
    const fossilFeature::TemplateLookup templates = [](std::string_view name) -> const structure::StructureTemplate* {
        return name.ends_with("_coal") ? &overlay : &base;
    };
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    const FixedBiomeEnvironment environment{std::string{biome}};
    for (std::int32_t chunkIndex = 0; chunkIndex < 48; ++chunkIndex) {
        Chunk chunk = stoneChunk(chunkIndex * 3 - 8, chunkIndex * -2 + 5);
        overworldUndergroundStructures::decorate(chunk, 101LL + chunkIndex, environment,
                                                 withTemplates ? templates : nullptr);
        const auto [chunkChecksum, chunkPlaced] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.placed += chunkPlaced;
        result.deferred += chunk.takeGeneratedBlockWrites().size();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

} // namespace

TEST_CASE("the underground structures step places what the frozen kotlin places") {
    SUBCASE("desert") {
        const Run result = run("minecraft:desert", true);
        CHECK(result.placed == 251);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 6081400787217114224LL);
    }

    SUBCASE("noTemplates") {
        const Run result = run("minecraft:desert", false);
        CHECK(result.placed == 168);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 2269218341021994694LL);
    }

    SUBCASE("plains") {
        const Run result = run("minecraft:plains", true);
        CHECK(result.placed == 168);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 2269218341021994694LL);
    }

    SUBCASE("nether") {
        const Run result = run("minecraft:nether_wastes", true);
        CHECK(result.placed == 0);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 4490923617970086549LL);
    }
}
