#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/nether/NetherVegetalDecoration.hpp>

#include <doctest.h>

#include <cstdint>
#include <set>
#include <string>
#include <string_view>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration::nether;
using basalt::server::world::testsupport::stateIdOf;
namespace rng = basalt::server::random;
namespace registry = basalt::server::registry;

namespace {

/// Netherrack with a nylium cap and a roof above the cavern, which is what these grow on, plus a
/// pocketed stone slab: the Nether lava spring reuses the overworld rock list, so netherrack on its
/// own never satisfies it. A mushroom needs a surface and a support and nothing else, and it is rare
/// enough to need a thousand chunks, so it gets the thin one.
[[nodiscard]] Chunk netherChunk(std::int32_t chunkX, std::int32_t chunkZ, std::string_view nylium, bool thin) {
    Chunk chunk{chunkX, chunkZ};
    if (thin) {
        for (std::int32_t x = 0; x < 16; ++x) {
            for (std::int32_t z = 0; z < 16; ++z) {
                chunk.setBlockStateId(x, 40, z, stateIdOf(nylium));
            }
        }
        return chunk;
    }
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 18; y <= 40; ++y) {
                chunk.setBlockStateId(x, y, z, stateIdOf("netherrack"));
            }
            for (std::int32_t y = 20; y <= 30; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            if ((x * 3 + z) % 5 == 0) {
                chunk.setBlockStateId(x, 25, z, registry::blocks::air().stateId);
            }
            chunk.setBlockStateId(x, 40, z, stateIdOf(nylium));
            for (std::int32_t y = 60; y <= 75; ++y) {
                chunk.setBlockStateId(x, y, z, stateIdOf("netherrack"));
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk, bool thin) {
    static const std::set<std::string, std::less<>> ignored = {"netherrack", "air", "stone", "crimson_nylium",
                                                               "warped_nylium"};
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t placed = 0;
    for (std::int32_t y = thin ? 36 : 0; y <= (thin ? 46 : 80); ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                const basalt::wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(id);
                if (block != nullptr && !ignored.contains(block->path())) {
                    ++placed;
                }
            }
        }
    }
    return {static_cast<std::int64_t>(checksum), placed};
}

[[nodiscard]] terrain::NetherBiomeLookup fixedBiome(std::string_view biome) {
    return [biome](std::int32_t, std::int32_t, std::int32_t, densityfunction::SamplerContext*) { return biome; };
}

struct Run {
    std::int32_t placed = 0;
    std::size_t deferred = 0;
    std::int64_t checksum = 0;
};

[[nodiscard]] Run runStep(std::string_view biome, std::string_view nylium, std::int32_t count, std::int32_t index,
                          bool thin) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    const terrain::NetherBiomeLookup lookup = fixedBiome(biome);
    for (std::int32_t chunkIndex = 0; chunkIndex < count; ++chunkIndex) {
        Chunk chunk = netherChunk(chunkIndex * 5 - 30, chunkIndex * -3 + 12, nylium, thin);
        const std::int64_t seed = 8080LL + chunkIndex;
        if (index < 0) {
            netherVegetalDecoration::decorate(chunk, seed, lookup);
        } else {
            netherVegetalDecoration::decorateFeature(chunk, seed, lookup, index);
        }
        const auto [chunkChecksum, chunkPlaced] = fold(chunk, thin);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.placed += chunkPlaced;
        result.deferred += chunk.takeGeneratedBlockWrites().size();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

struct RunAt {
    std::string known;
    std::int32_t placed = 0;
    std::int64_t checksum = 0;
    std::int32_t after = 0;
};

[[nodiscard]] RunAt runAt(std::string_view feature, std::string_view nylium, std::int32_t originY, std::int32_t count) {
    RunAt result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t chunkIndex = 0; chunkIndex < count; ++chunkIndex) {
        Chunk chunk = netherChunk(chunkIndex * 5 - 30, chunkIndex * -3 + 12, nylium, false);
        rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(4321LL + chunkIndex * 5501LL));
        const bool known = netherVegetalDecoration::placeAt(chunk, random, chunk.chunkX() * 16 + 8, originY,
                                                            chunk.chunkZ() * 16 + 8, feature);
        result.known += known ? "1" : "0";
        const auto [chunkChecksum, chunkPlaced] = fold(chunk, false);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.placed += chunkPlaced;
        result.after = random.nextInt();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

} // namespace

TEST_CASE("the nether vegetal step places what the frozen kotlin places") {
    SUBCASE("crimsonStep") {
        const Run result = runStep("minecraft:crimson_forest", "crimson_nylium", 12, -1, false);
        CHECK(result.placed == 7389);
        CHECK(result.deferred == 1361);
        CHECK(result.checksum == 5161766070090220282LL);
    }

    SUBCASE("warpedStep") {
        const Run result = runStep("minecraft:warped_forest", "warped_nylium", 12, -1, false);
        CHECK(result.placed == 6922);
        CHECK(result.deferred == 1099);
        CHECK(result.checksum == -5983515147374863062LL);
    }

    SUBCASE("wastesStep") {
        const Run result = runStep("minecraft:nether_wastes", "netherrack", 12, -1, false);
        CHECK(result.placed == 6);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -9198142754283494455LL);
    }

    SUBCASE("step_spring_lava") {
        const Run result = runStep("minecraft:crimson_forest", "crimson_nylium", 12, 0, false);
        CHECK(result.placed == 6);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -8480335081749464119LL);
    }

    SUBCASE("step_brown_mushroom_normal") {
        const Run result = runStep("minecraft:crimson_forest", "crimson_nylium", 1200, 1, true);
        CHECK(result.placed == 38);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -1911390607015680227LL);
    }

    SUBCASE("step_red_mushroom_normal") {
        const Run result = runStep("minecraft:crimson_forest", "crimson_nylium", 1200, 2, true);
        CHECK(result.placed == 19);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 6332282559537409956LL);
    }

    SUBCASE("step_warped_fungi") {
        const Run result = runStep("minecraft:warped_forest", "warped_nylium", 12, 3, false);
        CHECK(result.placed == 6043);
        CHECK(result.deferred == 1093);
        CHECK(result.checksum == -5252296925272484546LL);
    }

    SUBCASE("step_warped_forest_vegetation") {
        const Run result = runStep("minecraft:warped_forest", "warped_nylium", 12, 4, false);
        CHECK(result.placed == 588);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -8470500114296704403LL);
    }

    SUBCASE("step_nether_sprouts") {
        const Run result = runStep("minecraft:warped_forest", "warped_nylium", 12, 5, false);
        CHECK(result.placed == 458);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 2879588653666756185LL);
    }

    SUBCASE("step_twisting_vines") {
        const Run result = runStep("minecraft:warped_forest", "warped_nylium", 12, 6, false);
        CHECK(result.placed == 115);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 6709706997153085155LL);
    }

    SUBCASE("step_weeping_vines") {
        const Run result = runStep("minecraft:crimson_forest", "crimson_nylium", 12, 7, false);
        CHECK(result.placed == 98);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -7279039764634202994LL);
    }

    SUBCASE("step_crimson_fungi") {
        const Run result = runStep("minecraft:crimson_forest", "crimson_nylium", 12, 8, false);
        CHECK(result.placed == 6510);
        CHECK(result.deferred == 1430);
        CHECK(result.checksum == 3159413726163373380LL);
    }

    SUBCASE("step_crimson_forest_vegetation") {
        const Run result = runStep("minecraft:crimson_forest", "crimson_nylium", 12, 9, false);
        CHECK(result.placed == 669);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -2855606969463430986LL);
    }
}

TEST_CASE("one nether feature at one position matches the frozen kotlin") {
    SUBCASE("crimsonFungiAt") {
        const RunAt result = runAt("crimson_fungi", "crimson_nylium", 41, 12);
        CHECK(result.known == "111111111111");
        CHECK(result.placed == 1134);
        CHECK(result.checksum == 8589721459090670465LL);
        CHECK(result.after == 696925691);
    }

    SUBCASE("warpedFungiAt") {
        const RunAt result = runAt("warped_fungi", "warped_nylium", 41, 12);
        CHECK(result.known == "111111111111");
        CHECK(result.placed == 1044);
        CHECK(result.checksum == -5094036391827178774LL);
        CHECK(result.after == 1337090657);
    }

    SUBCASE("crimsonVegetationAt") {
        const RunAt result = runAt("crimson_forest_vegetation", "crimson_nylium", 41, 12);
        CHECK(result.known == "111111111111");
        CHECK(result.placed == 185);
        CHECK(result.checksum == 641878330216705717LL);
        CHECK(result.after == -616600724);
    }

    SUBCASE("warpedVegetationAt") {
        const RunAt result = runAt("warped_forest_vegetation", "warped_nylium", 41, 12);
        CHECK(result.known == "111111111111");
        CHECK(result.placed == 185);
        CHECK(result.checksum == -875224842052121517LL);
        CHECK(result.after == -616600724);
    }

    SUBCASE("netherSproutsAt") {
        const RunAt result = runAt("nether_sprouts", "warped_nylium", 41, 12);
        CHECK(result.known == "111111111111");
        CHECK(result.placed == 175);
        CHECK(result.checksum == -7591036934833252433LL);
        CHECK(result.after == 1223358263);
    }

    SUBCASE("twistingVinesAt") {
        const RunAt result = runAt("twisting_vines", "warped_nylium", 41, 12);
        CHECK(result.known == "111111111111");
        CHECK(result.placed == 1588);
        CHECK(result.checksum == -1038494724387600849LL);
        CHECK(result.after == -1617430253);
    }

    SUBCASE("weepingVinesAt") {
        const RunAt result = runAt("weeping_vines", "crimson_nylium", 59, 12);
        CHECK(result.known == "111111111111");
        CHECK(result.placed == 1509);
        CHECK(result.checksum == 7503691896476610472LL);
        CHECK(result.after == -1412709652);
    }

    SUBCASE("lavaSpringAt") {
        const RunAt result = runAt("spring_lava_nether", "crimson_nylium", 25, 40);
        CHECK(result.known == "1111111111111111111111111111111111111111");
        CHECK(result.placed == 40);
        CHECK(result.checksum == -7349856536442553843LL);
        CHECK(result.after == 458676622);
    }

    SUBCASE("unknownAt") {
        const RunAt result = runAt("not_a_feature", "crimson_nylium", 41, 12);
        CHECK(result.known == "000000000000");
        CHECK(result.placed == 0);
        CHECK(result.checksum == 3105216870131864297LL);
        CHECK(result.after == 300097134);
    }
}
