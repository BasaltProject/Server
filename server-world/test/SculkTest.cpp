#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/SculkDecoration.hpp>

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

constexpr std::array<std::int32_t, 3> kCavernBottoms{-40, -16, 8};
constexpr std::int32_t kCavernHeight = 6;

[[nodiscard]] std::int32_t floorMod(std::int32_t value, std::int32_t modulus) {
    const std::int32_t rest = value % modulus;
    return rest < 0 ? rest + modulus : rest;
}

/// Shared by the chunk fill and the neighbour reader: a patch spreads well past its own chunk.
[[nodiscard]] std::optional<std::int32_t> terrainAt(std::int32_t x, std::int32_t y, std::int32_t z) {
    if (y < -64 || y >= 32) {
        return std::nullopt;
    }
    for (const std::int32_t bottom : kCavernBottoms) {
        if (y < bottom || y >= bottom + kCavernHeight) {
            continue;
        }
        // A flooded strip so the waterlogged vein and the drowned sensor both get placed.
        return bottom == -16 && y == bottom ? registry::blocks::water().stateId : registry::blocks::air().stateId;
    }
    // Single-block seams: a cursor in one has rock right above it, which is the only way the charge
    // decays by distance instead of being spent on a sensor.
    if (y == -28 || y == -4 || y == 20) {
        return registry::blocks::air().stateId;
    }
    // Deepslate bricks are replaceable by world generation only, which is the one difference between
    // the two replaceable sets.
    if (floorMod(x * 13 + y * 7 + z, 23) == 0) {
        return stateIdOf("deepslate_bricks");
    }
    return floorMod(x * 3 + z, 7) == 0 ? registry::blocks::stone().stateId : stateIdOf("deepslate");
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
    static const std::set<std::string, std::less<>> counted = {"sculk", "sculk_vein", "sculk_catalyst", "sculk_sensor",
                                                               "sculk_shrieker"};
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
    std::int64_t checksum = 0;
};

[[nodiscard]] Run run(std::string_view biome, std::int32_t index, bool outside) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    const FixedBiomeEnvironment environment{std::string{biome}};
    const ExternalStateReader external = outside ? terrainAt : nullptr;
    for (std::int32_t chunkIndex = 0; chunkIndex < 2; ++chunkIndex) {
        Chunk chunk = caveChunk(chunkIndex * 4 - 6, chunkIndex * -3 + 9);
        if (index < 0) {
            sculkDecoration::decorate(chunk, 2287LL + chunkIndex, environment, external);
        } else {
            sculkDecoration::decorateFeature(chunk, 2287LL + chunkIndex, environment, index, external);
        }
        const auto [chunkChecksum, chunkPlaced] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.placed += chunkPlaced;
        result.deferred += chunk.takeGeneratedBlockWrites().size();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

} // namespace

TEST_CASE("the sculk steps place what the frozen kotlin places") {
    SUBCASE("bothSteps") {
        const Run result = run("minecraft:deep_dark", -1, false);
        CHECK(result.placed == 4951);
        CHECK(result.deferred == 2818);
        CHECK(result.checksum == -6492857735299813194LL);
    }

    SUBCASE("veinIndex") {
        const Run result = run("minecraft:deep_dark", 0, false);
        CHECK(result.placed == 45);
        CHECK(result.deferred == 3);
        CHECK(result.checksum == 8444179288895066844LL);
    }

    SUBCASE("patchIndex") {
        const Run result = run("minecraft:deep_dark", 1, false);
        CHECK(result.placed == 3848);
        CHECK(result.deferred == 2413);
        CHECK(result.checksum == 528712234429032955LL);
    }

    SUBCASE("patchOutside") {
        const Run result = run("minecraft:deep_dark", 1, true);
        CHECK(result.placed == 2734);
        CHECK(result.deferred == 4662);
        CHECK(result.checksum == -8603868603317952065LL);
    }

    SUBCASE("plainsStep") {
        const Run result = run("minecraft:plains", -1, false);
        CHECK(result.placed == 0);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -1155287872633369129LL);
    }
}

TEST_CASE("one sculk feature at one position matches the frozen kotlin") {
    constexpr std::array<std::string_view, 4> kFeatures{"sculk_patch_ancient_city", "sculk_patch_deep_dark",
                                                        "sculk_vein", "not_a_feature"};
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t placed = 0;
    std::string known;
    std::int32_t after = 0;
    for (const std::string_view feature : kFeatures) {
        for (std::int32_t chunkIndex = 0; chunkIndex < 3; ++chunkIndex) {
            Chunk chunk = caveChunk(chunkIndex, -chunkIndex);
            rng::RandomSource random =
                rng::RandomSource::worldgen(rng::RandomSource::legacy(1471LL + chunkIndex * 2617LL));
            // The floor of a different cavern each time: one dry, one flooded, one higher up.
            const DecorationPos origin{.x = chunk.chunkX() * 16 + 8,
                                       .y = kCavernBottoms[static_cast<std::size_t>(chunkIndex)],
                                       .z = chunk.chunkZ() * 16 + 8};
            known += sculkDecoration::placeAt(chunk, random, origin, feature) ? "1" : "0";
            const auto [chunkChecksum, chunkPlaced] = fold(chunk);
            checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
            placed += chunkPlaced;
            after = random.nextInt();
        }
    }
    CHECK(known == "111111111000");
    CHECK(placed == 1344);
    CHECK(static_cast<std::int64_t>(checksum) == -3801488731580343875LL);
    CHECK(after == 1004880498);
}
