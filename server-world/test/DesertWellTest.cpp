#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/structure/StructureTemplate.hpp>
#include <basalt/server/world/terrain/decoration/DesertWellDecoration.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <doctest.h>

#include <cstdint>
#include <set>
#include <string>
#include <tuple>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using basalt::server::world::testsupport::stateIdOf;
using basalt::wire::nbt::NbtCompound;
using basalt::wire::nbt::NbtList;
using basalt::wire::nbt::NbtTag;
using basalt::wire::nbt::NbtType;
namespace registry = basalt::server::registry;
namespace rng = basalt::server::random;

namespace {

[[nodiscard]] NbtTag intList(std::int32_t x, std::int32_t y, std::int32_t z) {
    return NbtTag::ofList(NbtList{NbtType::Int, {NbtTag::ofInt(x), NbtTag::ofInt(y), NbtTag::ofInt(z)}});
}

/// A template of one block kind filling its box, written here rather than read from any jar.
[[nodiscard]] structure::StructureTemplate filled(std::int32_t sizeX, std::int32_t sizeY, std::int32_t sizeZ,
                                                  const std::string& block, const NbtCompound* blockEntity) {
    NbtCompound paletteEntry;
    paletteEntry.set("Name", NbtTag::ofString(block));
    NbtList blocks{NbtType::Compound};
    for (std::int32_t x = 0; x < sizeX; ++x) {
        for (std::int32_t y = 0; y < sizeY; ++y) {
            for (std::int32_t z = 0; z < sizeZ; ++z) {
                NbtCompound entry;
                entry.set("pos", intList(x, y, z));
                entry.set("state", NbtTag::ofInt(0));
                if (blockEntity != nullptr) {
                    entry.set("nbt", NbtTag::ofCompound(*blockEntity));
                }
                blocks.push(NbtTag::ofCompound(std::move(entry)));
            }
        }
    }
    NbtCompound nbt;
    nbt.set("size", intList(sizeX, sizeY, sizeZ));
    nbt.set("blocks", NbtTag::ofList(std::move(blocks)));
    nbt.set("palette", NbtTag::ofList(NbtList{NbtType::Compound, {NbtTag::ofCompound(std::move(paletteEntry))}}));
    nbt.set("entities", NbtTag::ofList(NbtList{NbtType::Compound, {}}));
    structure::StructureTemplate built;
    built.load(nbt);
    return built;
}

/// The two templates the feature asks for, by the names the 26.3 data gives them.
[[nodiscard]] structure::TemplateLoader loader() {
    static const structure::StructureTemplate well = filled(5, 2, 3, "minecraft:sandstone", nullptr);
    static const structure::StructureTemplate sand = [] {
        NbtCompound brushable;
        brushable.set("id", NbtTag::ofString("minecraft:brushable_block"));
        return filled(1, 1, 1, "minecraft:suspicious_sand", &brushable);
    }();
    return [](const basalt::wire::common::Identifier& id) -> const structure::StructureTemplate* {
        if (id.path() == "desert_well/well") {
            return &well;
        }
        return id.path() == "desert_well/suspicious_sand" ? &sand : nullptr;
    };
}

[[nodiscard]] Chunk sandChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 60; y < 70; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            chunk.setBlockStateId(x, 70, z, stateIdOf("sand"));
        }
    }
    return chunk;
}

/// The lowest corner of every block of `stateId`, with how many there are.
[[nodiscard]] std::tuple<std::int32_t, std::int32_t, std::int32_t, std::int32_t> extent(const Chunk& chunk,
                                                                                        std::int32_t stateId) {
    std::int32_t count = 0;
    std::int32_t lowX = 16;
    std::int32_t lowY = 400;
    std::int32_t lowZ = 16;
    for (std::int32_t y = 60; y <= 80; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                if (chunk.blockStateId(x, y, z) == stateId) {
                    ++count;
                    lowX = std::min(lowX, x);
                    lowY = std::min(lowY, y);
                    lowZ = std::min(lowZ, z);
                }
            }
        }
    }
    return {count, lowX, lowY, lowZ};
}

} // namespace

TEST_CASE("the 26.3 desert well is its templates, centred and rotated, with two buried sand blocks") {
    for (std::int64_t seed = 0; seed < 8; ++seed) {
        CAPTURE(seed);
        Chunk chunk = sandChunk(0, 0);
        rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(seed));
        desertWellDecoration::placeAt(chunk, random, 8, 70, 8, loader());

        // parity: `TemplateFeature` draws its one-entry template list, a rotation and the one palette,
        // and the two sand blocks each draw their spot first.
        rng::RandomSource expected = rng::RandomSource::worldgen(rng::RandomSource::legacy(seed));
        static_cast<void>(expected.nextInt(1));
        const std::int32_t rotation = expected.nextInt(4);
        static_cast<void>(expected.nextInt(1));
        for (std::int32_t sand = 0; sand < 2; ++sand) {
            static_cast<void>(expected.nextInt(5));
            static_cast<void>(expected.nextInt(1));
            static_cast<void>(expected.nextInt(4));
            static_cast<void>(expected.nextInt(1));
        }
        CHECK(random.nextInt() == expected.nextInt());

        // Centred by half its size along the rotated west and north: 5 by 3 unrotated, 3 by 5 turned.
        const auto [wells, lowX, lowY, lowZ] = extent(chunk, stateIdOf("sandstone"));
        CHECK(lowY == 68);
        const bool quarterTurn = rotation == 1 || rotation == 3;
        CHECK(lowX == (quarterTurn ? 7 : 6));
        CHECK(lowZ == (quarterTurn ? 6 : 7));
        CHECK(wells + std::get<0>(extent(chunk, stateIdOf("suspicious_sand"))) >= 29);

        std::set<std::int32_t> depths;
        for (const ChunkBlockEntity& entity : chunk.blockEntities()) {
            depths.insert(entity.y());
            CHECK(entity.data().getString("LootTable") == "minecraft:archaeology/desert_well");
        }
        CHECK(depths.size() <= 2);
        CHECK(!depths.empty());
    }
}

TEST_CASE("the desert well places nothing without its templates") {
    Chunk chunk = sandChunk(0, 0);
    rng::RandomSource random = rng::RandomSource::legacy(1);
    desertWellDecoration::placeAt(chunk, random, 8, 70, 8, nullptr);
    CHECK(std::get<0>(extent(chunk, stateIdOf("sandstone"))) == 0);
    CHECK(random.nextInt() == rng::RandomSource::legacy(1).nextInt());
}
