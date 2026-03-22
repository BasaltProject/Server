#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/structure/StructureTemplate.hpp>
#include <basalt/server/world/terrain/decoration/OverworldLakeDecoration.hpp>
#include <basalt/server/world/terrain/decoration/PlacedFeatures.hpp>

#include <doctest.h>

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using basalt::server::world::testsupport::FixedBiomeEnvironment;
using basalt::server::world::testsupport::stateIdOf;
using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtCompound;
using basalt::wire::nbt::NbtList;
using basalt::wire::nbt::NbtTag;
using basalt::wire::nbt::NbtType;
namespace registry = basalt::server::registry;
namespace rng = basalt::server::random;

namespace {

/// The terrain's floor. A shallower world than vanilla's keeps the fold from dwarfing the run.
constexpr std::int32_t kFloorY = -8;

/// A stand-in spring template: a sulfur cube, big enough that a rotation moves its blocks. No Mojang
/// asset goes near the repository, and what is under test is the placement, not the shape.
[[nodiscard]] structure::StructureTemplate springTemplate(std::int32_t size) {
    NbtCompound entry;
    entry.set("Name", NbtTag::ofString("minecraft:sulfur"));
    NbtList palette{NbtType::Compound};
    palette.push(NbtTag::ofCompound(std::move(entry)));
    NbtList blocks{NbtType::Compound};
    for (std::int32_t x = 0; x < size; ++x) {
        for (std::int32_t y = 0; y < 2; ++y) {
            for (std::int32_t z = 0; z < size + 1; ++z) {
                NbtCompound block;
                block.set("pos", NbtTag::ofList(
                                     NbtList{NbtType::Int, {NbtTag::ofInt(x), NbtTag::ofInt(y), NbtTag::ofInt(z)}}));
                block.set("state", NbtTag::ofInt(0));
                blocks.push(NbtTag::ofCompound(std::move(block)));
            }
        }
    }
    NbtCompound nbt;
    nbt.set("size",
            NbtTag::ofList(NbtList{NbtType::Int, {NbtTag::ofInt(size), NbtTag::ofInt(2), NbtTag::ofInt(size + 1)}}));
    nbt.set("palette", NbtTag::ofList(std::move(palette)));
    nbt.set("blocks", NbtTag::ofList(std::move(blocks)));
    structure::StructureTemplate built;
    built.load(nbt);
    return built;
}

/// The four sizes differ, so which variant the weighted draw picked is visible in the world.
[[nodiscard]] const structure::StructureTemplate* springTemplateFor(const Identifier& name) {
    static const structure::StructureTemplate small = springTemplate(2);
    static const structure::StructureTemplate medium = springTemplate(3);
    static const structure::StructureTemplate large = springTemplate(4);
    static const structure::StructureTemplate extraLarge = springTemplate(5);
    const std::string_view path = name.path();
    if (path.find("extra_large") != std::string_view::npos) {
        return &extraLarge;
    }
    if (path.find("large") != std::string_view::npos) {
        return &large;
    }
    if (path.find("medium") != std::string_view::npos) {
        return &medium;
    }
    return &small;
}

/// Stone to 60 with a sulfur cap, so `sulfur_pool`'s final predicate can pass, and a flat cavern at
/// 20..27 wide enough for the spring's four level probes eight blocks out.
[[nodiscard]] Chunk terrainChunk(std::int32_t chunkX, std::int32_t chunkZ, bool cavern, std::int32_t floor) {
    Chunk chunk{chunkX, chunkZ};
    const std::int32_t stone = registry::blocks::stone().stateId;
    const std::int32_t sulfur = stateIdOf("sulfur");
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = floor; y <= 60; ++y) {
                chunk.setBlockStateId(x, y, z, stone);
            }
            chunk.setBlockStateId(x, 60, z, sulfur);
            if (cavern) {
                for (std::int32_t y = 20; y <= 27; ++y) {
                    chunk.setBlockStateId(x, y, z, registry::blocks::air().stateId);
                }
            }
        }
    }
    // Only the origin block itself, so the root column climbs through stone to the cavern instead of
    // finding a hollow floor one step up and giving up.
    if (cavern) {
        chunk.setBlockStateId(7, 5, 9, registry::blocks::air().stateId);
    }
    return chunk;
}

/// The undecorated neighbour terrain, so a body that crosses the chunk edge is not refused.
[[nodiscard]] std::int32_t neighbourState(std::int32_t y, bool cavern, std::int32_t floor) {
    if (y > 60 || y < floor) {
        return registry::blocks::air().stateId;
    }
    if (y == 60) {
        return stateIdOf("sulfur");
    }
    if (cavern && y >= 20 && y <= 27) {
        return registry::blocks::air().stateId;
    }
    return registry::blocks::stone().stateId;
}

/// Memoised per state id: the fold reads millions of blocks, and building the path string for each
/// one is most of the run.
[[nodiscard]] bool isCounted(std::int32_t stateId) {
    static const std::set<std::string, std::less<>> counted = {"lava",          "sulfur", "sulfur_spike",
                                                               "potent_sulfur", "tuff",   "water"};
    static std::vector<signed char> known;
    const auto index = static_cast<std::size_t>(stateId);
    if (index >= known.size()) {
        known.resize(index + 1, -1);
    }
    if (known[index] < 0) {
        const Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
        known[index] = static_cast<signed char>(block != nullptr && counted.contains(block->path()) ? 1 : 0);
    }
    return known[index] == 1;
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk, std::int32_t foldFrom,
                                                         std::int32_t foldTo) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t placed = 0;
    for (std::int32_t y = foldFrom; y <= foldTo; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                if (isCounted(id)) {
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
    std::int32_t draws = 0;
    std::int64_t checksum = 0;
};

/// A rare feature needs many chunks, so only the band it can reach is built and folded: filling a
/// whole column 900 times over costs more than the feature itself.
[[nodiscard]] Run runLakes(std::string_view biome, std::int32_t count, std::string_view feature = {},
                           std::int32_t foldFrom = kFloorY, std::int32_t foldTo = 64) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    const FixedBiomeEnvironment environment{std::string{biome}};
    for (std::int32_t chunkIndex = 0; chunkIndex < count; ++chunkIndex) {
        Chunk chunk = terrainChunk(chunkIndex * 3 - 8, chunkIndex * -2 + 5, false, foldFrom);
        const std::int64_t seed = 77LL + chunkIndex;
        {
            const Chunk::ExternalStateScope neighbours{chunk, [foldFrom](std::int32_t, std::int32_t y, std::int32_t) {
                                                           return std::optional{neighbourState(y, false, foldFrom)};
                                                       }};
            if (feature.empty()) {
                overworldLakeDecoration::decorate(chunk, seed, environment);
            } else {
                overworldLakeDecoration::decorateFeature(chunk, seed, environment, feature);
            }
        }
        const auto [chunkChecksum, chunkPlaced] = fold(chunk, foldFrom, foldTo);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.placed += chunkPlaced;
        result.deferred += chunk.takeGeneratedBlockWrites().size();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

[[nodiscard]] Run runSpring(bool withTemplates, std::int32_t count) {
    const placedFeatures::ContextualPlacement* placement =
        placedFeatures::contextual(Identifier::vanilla("rooted_sulfur_spring"));
    REQUIRE(placement != nullptr);
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    std::uint32_t draws = 0;
    for (std::int32_t chunkIndex = 0; chunkIndex < count; ++chunkIndex) {
        Chunk chunk = terrainChunk(chunkIndex * 3 - 8, chunkIndex * -2 + 5, true, kFloorY);
        rng::RandomSource worldgen = rng::RandomSource::worldgen(rng::RandomSource::xoroshiro(500LL + chunkIndex));
        placedFeatures::FeatureContext context;
        if (withTemplates) {
            context.templates = springTemplateFor;
        }
        context.blockStateAt = [&chunk](std::int32_t x, std::int32_t y, std::int32_t z) {
            if ((x >> 4) == chunk.chunkX() && (z >> 4) == chunk.chunkZ() && y >= -64 && y <= 319) {
                return chunk.blockStateId(x & 15, y, z & 15);
            }
            return neighbourState(y, true, kFloorY);
        };
        {
            const Chunk::ExternalStateScope neighbours{chunk, [](std::int32_t, std::int32_t y, std::int32_t) {
                                                           return std::optional{neighbourState(y, true, kFloorY)};
                                                       }};
            if ((*placement)(chunk, worldgen, 7, 5, 9, context)) {
                ++result.placed;
            }
        }
        const auto [chunkChecksum, chunkPlaced] = fold(chunk, kFloorY, 64);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.placed += chunkPlaced;
        result.deferred += chunk.takeGeneratedBlockWrites().size();
        // A trailing draw pins how much of the random each case consumed.
        draws = draws * 31U + static_cast<std::uint32_t>(worldgen.nextInt(1 << 20));
    }
    result.draws = static_cast<std::int32_t>(draws);
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

} // namespace

TEST_CASE("the lakes step and the sulfur spring place what the frozen kotlin places") {
    SUBCASE("lakeUnderground") {
        const Run result = runLakes("minecraft:plains", 150, "lake_lava_underground");
        CHECK(result.placed == 38780);
        CHECK(result.deferred == 381);
        CHECK(result.checksum == 4272562641936543490LL);
    }

    SUBCASE("lakeSurface") {
        const Run result = runLakes("minecraft:plains", 900, "lake_lava_surface", 50);
        CHECK(result.placed == 230399);
        CHECK(result.deferred == 128);
        CHECK(result.checksum == 7261584562030715561LL);
    }

    SUBCASE("sulfurPool") {
        const Run result = runLakes("minecraft:sulfur_caves", 30, "sulfur_pool");
        CHECK(result.placed == 17442);
        CHECK(result.deferred == 23584);
        CHECK(result.checksum == 7258998429875258203LL);
    }

    SUBCASE("springNode") {
        const Run result = runLakes("minecraft:sulfur_caves", 30, "rooted_sulfur_spring");
        CHECK(result.placed == 7680);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 3036780461455879443LL);
    }

    SUBCASE("lakesPlains") {
        const Run result = runLakes("minecraft:plains", 60);
        CHECK(result.placed == 15424);
        CHECK(result.deferred == 29);
        CHECK(result.checksum == -7473059217675198841LL);
    }

    SUBCASE("lakesSulfur") {
        const Run result = runLakes("minecraft:sulfur_caves", 60);
        CHECK(result.placed == 35544);
        CHECK(result.deferred == 45835);
        CHECK(result.checksum == -8526133637202311476LL);
    }

    SUBCASE("lakesDeepDark") {
        const Run result = runLakes("minecraft:deep_dark", 60);
        CHECK(result.placed == 15360);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -3531853649559082839LL);
    }

    SUBCASE("spring") {
        const Run result = runSpring(true, 24);
        CHECK(result.placed == 11919);
        CHECK(result.deferred == 65);
        CHECK(result.draws == -655195496);
        CHECK(result.checksum == -8105106595843827433LL);
    }

    SUBCASE("springNoTemplates") {
        const Run result = runSpring(false, 24);
        CHECK(result.placed == 6144);
        CHECK(result.deferred == 0);
        CHECK(result.draws == -1946882916);
        CHECK(result.checksum == -8948362395136162523LL);
    }
}
