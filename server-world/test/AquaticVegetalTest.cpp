#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/AquaticDecoration.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <utility>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using basalt::server::world::testsupport::stateIdOf;
namespace rng = basalt::server::random;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t floorMod(std::int32_t value, std::int32_t modulus) {
    const std::int32_t rest = value % modulus;
    return rest < 0 ? rest + modulus : rest;
}

/// A rolling floor under deep water, with magma and a slab so both support rules matter.
[[nodiscard]] Chunk oceanChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            const std::int32_t worldX = chunkX * 16 + x;
            const std::int32_t worldZ = chunkZ * 16 + z;
            // A shallow shelf under a third of the columns, where a kelp stalk runs out of water
            // after a block or two instead of always having room for its full height.
            const bool shallow = floorMod(worldX * 11 + worldZ * 3, 3) == 0;
            const std::int32_t floor = shallow ? 60 : 40 + floorMod(worldX * 3 + worldZ * 7, 5);
            for (std::int32_t y = 30; y <= floor; ++y) {
                chunk.setBlockStateId(x, y, z, stateIdOf("gravel"));
            }
            std::int32_t top = stateIdOf("sand");
            switch (floorMod(worldX * 5 + worldZ, 9)) {
            // Magma holds nothing at all, and a slab is the shape whose top edge alone supports a
            // sea pickle.
            case 0:
                top = stateIdOf("magma_block");
                break;
            case 1:
                top = stateIdOf("smooth_stone_slab");
                break;
            default:
                break;
            }
            chunk.setBlockStateId(x, floor, z, top);
            for (std::int32_t y = floor + 1; y <= 62; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::water().stateId);
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk) {
    static const std::set<std::string, std::less<>> counted = {"seagrass", "tall_seagrass", "kelp", "kelp_plant",
                                                               "sea_pickle"};
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t placed = 0;
    for (std::int32_t y = 30; y <= 70; ++y) {
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
    std::int32_t after = 0;
};

[[nodiscard]] Run run(const BiomeVegetalDecoration& step, std::int32_t count) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t chunkIndex = 0; chunkIndex < count; ++chunkIndex) {
        Chunk chunk = oceanChunk(chunkIndex * 2 - 7, chunkIndex * -3 + 4);
        rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::xoroshiro(6199LL + chunkIndex));
        step(chunk, random);
        const auto [chunkChecksum, chunkPlaced] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.placed += chunkPlaced;
        result.deferred += chunk.takeGeneratedBlockWrites().size();
        result.after = random.nextInt();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

} // namespace

TEST_CASE("the ocean vegetal entries match the frozen kotlin") {
    SUBCASE("ocean") {
        const Run result = run(aquaticDecoration::ocean(), 6);
        CHECK(result.placed == 389);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -7856957103204572941LL);
        CHECK(result.after == 881572358);
    }

    SUBCASE("deepOcean") {
        const Run result = run(aquaticDecoration::deepOcean(), 6);
        CHECK(result.placed == 472);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 3802444157256907270LL);
        CHECK(result.after == 881572358);
    }

    SUBCASE("coldOcean") {
        const Run result = run(aquaticDecoration::coldOcean(), 6);
        CHECK(result.placed == 352);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -1370926240979115521LL);
        CHECK(result.after == -1741392533);
    }

    SUBCASE("deepColdOcean") {
        const Run result = run(aquaticDecoration::deepColdOcean(), 6);
        CHECK(result.placed == 465);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -1980262810836548184LL);
        CHECK(result.after == 1976707368);
    }

    SUBCASE("lukewarmOcean") {
        const Run result = run(aquaticDecoration::lukewarmOcean(), 6);
        CHECK(result.placed == 529);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -2226129134789549706LL);
        CHECK(result.after == -772664592);
    }

    SUBCASE("deepLukewarmOcean") {
        const Run result = run(aquaticDecoration::deepLukewarmOcean(), 6);
        CHECK(result.placed == 650);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 1243907033239691651LL);
        CHECK(result.after == -772664592);
    }

    SUBCASE("river") {
        const Run result = run(aquaticDecoration::river(), 6);
        CHECK(result.placed == 248);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 147682549083375841LL);
        CHECK(result.after == 881572358);
    }

    SUBCASE("swamp") {
        const Run result = run(aquaticDecoration::swamp(), 6);
        CHECK(result.placed == 382);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -204145715090334304LL);
        CHECK(result.after == -2063751104);
    }

    SUBCASE("warmOcean") {
        const Run result = run(aquaticDecoration::warmOcean(), 24);
        CHECK(result.placed == 1547);
        CHECK(result.deferred == 769);
        CHECK(result.checksum == -2573967883330831720LL);
        CHECK(result.after == -897359284);
    }
}

TEST_CASE("one aquatic feature at one position matches the frozen kotlin") {
    constexpr std::array<std::string_view, 12> kFeatures{"kelp_cold",          "kelp_warm",       "sea_pickle",
                                                         "seagrass_cold",      "seagrass_deep",   "seagrass_deep_cold",
                                                         "seagrass_deep_warm", "seagrass_normal", "seagrass_river",
                                                         "seagrass_swamp",     "seagrass_warm",   "not_a_feature"};
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t placed = 0;
    std::string known;
    std::int32_t after = 0;
    for (const std::string_view feature : kFeatures) {
        for (std::int32_t chunkIndex = 0; chunkIndex < 3; ++chunkIndex) {
            Chunk chunk = oceanChunk(chunkIndex, -chunkIndex);
            rng::RandomSource random =
                rng::RandomSource::worldgen(rng::RandomSource::legacy(797LL + chunkIndex * 3821LL));
            const DecorationPos origin{
                .x = chunk.chunkX() * 16 + 8, .y = 46 + chunkIndex, .z = chunk.chunkZ() * 16 + 8};
            known += aquaticDecoration::placeAt(chunk, random, origin, feature) ? "1" : "0";
            const auto [chunkChecksum, chunkPlaced] = fold(chunk);
            checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
            placed += chunkPlaced;
            after = random.nextInt();
        }
    }
    CHECK(known == "111111111111111111111111111111111000");
    CHECK(placed == 59);
    CHECK(static_cast<std::int64_t>(checksum) == 5145286995187963922LL);
    CHECK(after == 4933430);
}
