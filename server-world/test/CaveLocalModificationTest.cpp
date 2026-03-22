#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/CaveLocalModificationDecoration.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using basalt::server::world::testsupport::FixedBiomeEnvironment;
using basalt::server::world::testsupport::stateIdOf;
namespace rng = basalt::server::random;
namespace registry = basalt::server::registry;

namespace {

constexpr std::array<std::int32_t, 1> kCavernBottoms{-52};
constexpr std::int32_t kCavernHeight = 28;

[[nodiscard]] std::int32_t floorMod(std::int32_t value, std::int32_t modulus) {
    const std::int32_t rest = value % modulus;
    return rest < 0 ? rest + modulus : rest;
}

/// The block this terrain has at one position, shared by the chunk fill and the reader for the
/// neighbours: a geode is 33 blocks across, so most of one lands outside its own chunk.
[[nodiscard]] std::optional<std::int32_t> terrainAt(std::int32_t x, std::int32_t y, std::int32_t z) {
    if (y < -64 || y >= 32) {
        return std::nullopt;
    }
    for (const std::int32_t bottom : kCavernBottoms) {
        if (y < bottom || y >= bottom + kCavernHeight) {
            continue;
        }
        // Lava on the floor of one cavern, which is where a dripstone refuses to root.
        if (y == bottom && bottom == -52 && floorMod(x + z, 5) == 0) {
            return stateIdOf("lava");
        }
        return registry::blocks::air().stateId;
    }
    if (y >= -64 && y <= -61) {
        return stateIdOf("bedrock");
    }
    // Single wet and molten blocks rather than a body of either: a geode is refused outright by two
    // invalid sample points, so a flooded cavern next door means it never generates at all.
    if (floorMod(x * 7 + y * 3 + z, 29) == 0) {
        return registry::blocks::water().stateId;
    }
    if (floorMod(x * 5 + y * 11 + z, 31) == 0) {
        return stateIdOf("lava");
    }
    return floorMod(x * 3 + z, 11) == 0 ? stateIdOf("deepslate") : registry::blocks::stone().stateId;
}

[[nodiscard]] Chunk caveChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = -64; y < 32; ++y) {
                chunk.setBlockStateId(x, y, z, *terrainAt(chunkX * 16 + x, y, chunkZ * 16 + z));
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk) {
    static const std::set<std::string, std::less<>> counted = {
        "amethyst_block",      "budding_amethyst",   "calcite",          "smooth_basalt",  "small_amethyst_bud",
        "medium_amethyst_bud", "large_amethyst_bud", "amethyst_cluster", "dripstone_block"};
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t placed = 0;
    for (std::int32_t y = -64; y < 32; ++y) {
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
    std::size_t ticks = 0;
    std::int64_t checksum = 0;
};

[[nodiscard]] Run run(std::string_view biome, std::int32_t index, std::int32_t count, bool outside, std::int64_t base) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    const FixedBiomeEnvironment environment{std::string{biome}};
    const ExternalStateReader external = outside ? terrainAt : nullptr;
    const ExternalSurfaceHeightReader surface =
        outside ? [](std::int32_t, std::int32_t) { return std::optional<std::int32_t>{32}; } : nullptr;
    for (std::int32_t chunkIndex = 0; chunkIndex < count; ++chunkIndex) {
        Chunk chunk = caveChunk(chunkIndex * 3 - 11, chunkIndex * -2 + 4);
        if (index < 0) {
            caveLocalModificationDecoration::decorate(chunk, base + chunkIndex, environment, surface, external);
        } else {
            caveLocalModificationDecoration::decorateFeatures(chunk, base + chunkIndex, environment, index, index,
                                                              surface, external);
        }
        const auto [chunkChecksum, chunkPlaced] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.placed += chunkPlaced;
        result.deferred += chunk.takeGeneratedBlockWrites().size();
        result.ticks += chunk.scheduledFluidTicks().size();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

} // namespace

TEST_CASE("the local modification steps place what the frozen kotlin places") {
    SUBCASE("bothSteps") {
        const Run result = run("minecraft:dripstone_caves", -1, 24, false, 5507);
        CHECK(result.placed == 1145);
        CHECK(result.deferred == 7);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == -5484601712830456174LL);
    }

    SUBCASE("geodeOutside") {
        const Run result = run("minecraft:plains", 2, 64, true, 6607);
        CHECK(result.placed == 188);
        CHECK(result.deferred == 1868);
        CHECK(result.ticks == 2);
        CHECK(result.checksum == -3891974237154470976LL);
    }

    SUBCASE("netherGeode") {
        const Run result = run("minecraft:nether_wastes", 2, 64, false, 6607);
        CHECK(result.placed == 0);
        CHECK(result.deferred == 0);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == -4809084022557803335LL);
    }

    SUBCASE("dripstoneIndex") {
        const Run result = run("minecraft:dripstone_caves", 3, 8, false, 5507);
        CHECK(result.placed == 306);
        CHECK(result.deferred == 4);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == -5903008619050191360LL);
    }

    SUBCASE("dripstoneOutside") {
        const Run result = run("minecraft:dripstone_caves", 3, 8, true, 5507);
        CHECK(result.placed == 8323);
        CHECK(result.deferred == 2788);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == -1876923547493120254LL);
    }

    SUBCASE("plainsDripstone") {
        const Run result = run("minecraft:plains", 3, 8, false, 5507);
        CHECK(result.placed == 0);
        CHECK(result.deferred == 0);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == -8470524865361231206LL);
    }
}

TEST_CASE("one local modification feature at one position matches the frozen kotlin") {
    constexpr std::array<std::string_view, 3> kFeatures{"amethyst_geode", "large_dripstone", "not_a_feature"};
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t placed = 0;
    std::size_t ticks = 0;
    std::string known;
    std::int32_t after = 0;
    for (const std::string_view feature : kFeatures) {
        for (std::int32_t chunkIndex = 0; chunkIndex < 3; ++chunkIndex) {
            Chunk chunk = caveChunk(chunkIndex, -chunkIndex);
            rng::RandomSource random =
                rng::RandomSource::worldgen(rng::RandomSource::legacy(6151LL + chunkIndex * 3319LL));
            const DecorationPos origin{
                .x = chunk.chunkX() * 16 + 8, .y = kCavernBottoms[0] + 4, .z = chunk.chunkZ() * 16 + 8};
            known += caveLocalModificationDecoration::placeAt(chunk, random, origin, feature) ? "1" : "0";
            const auto [chunkChecksum, chunkPlaced] = fold(chunk);
            checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
            placed += chunkPlaced;
            ticks += chunk.scheduledFluidTicks().size();
            after = random.nextInt();
        }
    }
    CHECK(known == "111111000");
    CHECK(placed == 308);
    CHECK(ticks == 0);
    CHECK(static_cast<std::int64_t>(checksum) == 4087204779981588003LL);
    CHECK(after == 1678591146);
}
