#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/HugeMushroomFeature.hpp>
#include <basalt/server/world/terrain/decoration/TreeDecoration.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using basalt::server::world::testsupport::stateIdOf;
namespace rng = basalt::server::random;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] Chunk groundChunk(std::int32_t chunkX, std::int32_t chunkZ, std::string_view surface) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = -64; y < 70; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            chunk.setBlockStateId(x, 70, z, stateIdOf(surface));
        }
    }
    return chunk;
}

/// Powder snow over snow blocks, which is the only ground the grove selector accepts.
[[nodiscard]] Chunk groveChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk = groundChunk(chunkX, chunkZ, "snow_block");
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 71; y <= 71 + (x + z) % 3; ++y) {
                chunk.setBlockStateId(x, y, z, stateIdOf("powder_snow"));
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::int64_t checksumOf(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t y = 60; y <= 140; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                checksum =
                    (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(chunk.blockStateId(x, y, z)))) *
                    1099511628211ULL;
            }
        }
    }
    return static_cast<std::int64_t>(checksum);
}

[[nodiscard]] std::int32_t changedBlocks(const Chunk& chunk, const Chunk& reference) {
    std::int32_t changed = 0;
    for (std::int32_t y = 60; y <= 140; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                if (chunk.blockStateId(x, y, z) != reference.blockStateId(x, y, z)) {
                    ++changed;
                }
            }
        }
    }
    return changed;
}

constexpr std::array<std::int64_t, 8> kSeeds{11LL,      907LL,      31337LL,     654321LL,
                                             8675309LL, 42424242LL, 987654321LL, 5150LL};

struct Run {
    std::string known;
    std::int32_t changed = 0;
    std::size_t deferred = 0;
    std::int64_t checksum = 0;
    std::int32_t after = 0;
};

[[nodiscard]] Run run(std::string_view selector, std::string_view surface, bool grove) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::size_t index = 0; index < kSeeds.size(); ++index) {
        const std::int32_t step = static_cast<std::int32_t>(index);
        const std::int32_t chunkX = step * 5 - 30;
        const std::int32_t chunkZ = step * -3 + 12;
        Chunk chunk = grove ? groveChunk(chunkX, chunkZ) : groundChunk(chunkX, chunkZ, surface);
        const Chunk reference = grove ? groveChunk(chunkX, chunkZ) : groundChunk(chunkX, chunkZ, surface);
        rng::RandomSource generator = rng::RandomSource::worldgen(rng::RandomSource::legacy(kSeeds[index]));
        const bool known = treeDecoration::placeAt(
            chunk, generator, DecorationPos{.x = chunkX * 16 + 8, .y = 71, .z = chunkZ * 16 + 8}, selector);
        result.known += known ? "1" : "0";
        result.changed += changedBlocks(chunk, reference);
        checksum = (checksum ^ static_cast<std::uint64_t>(checksumOf(chunk))) * 1099511628211ULL;
        result.deferred += chunk.takeGeneratedBlockWrites().size();
        result.after = generator.nextInt();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

} // namespace

TEST_CASE("the named tree selectors roll what the frozen kotlin rolls") {
    SUBCASE("trees_plains") {
        const Run result = run("trees_plains", "grass_block", false);
        CHECK(result.known == "11111111");
        CHECK(result.changed == 949);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -7616558408401481093LL);
        CHECK(result.after == 647488715);
    }

    SUBCASE("trees_birch_and_oak_leaf_litter") {
        const Run result = run("trees_birch_and_oak_leaf_litter", "grass_block", false);
        CHECK(result.known == "11111111");
        CHECK(result.changed == 729);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 2105109635837693179LL);
        CHECK(result.after == 116325522);
    }

    SUBCASE("trees_birch") {
        const Run result = run("trees_birch", "grass_block", false);
        CHECK(result.known == "11111111");
        CHECK(result.changed == 434);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 2975031954946439335LL);
        CHECK(result.after == -1988451487);
    }

    SUBCASE("birch_tall") {
        const Run result = run("birch_tall", "grass_block", false);
        CHECK(result.known == "11111111");
        CHECK(result.changed == 495);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 1868388358090014183LL);
        CHECK(result.after == 918253638);
    }

    SUBCASE("trees_taiga") {
        const Run result = run("trees_taiga", "grass_block", false);
        CHECK(result.known == "11111111");
        CHECK(result.changed == 540);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -4260937173446570308LL);
        CHECK(result.after == 146845651);
    }

    SUBCASE("trees_snowy") {
        const Run result = run("trees_snowy", "grass_block", false);
        CHECK(result.known == "11111111");
        CHECK(result.changed == 494);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 1209003463487621676LL);
        CHECK(result.after == -1988451487);
    }

    SUBCASE("trees_old_growth_spruce_taiga") {
        const Run result = run("trees_old_growth_spruce_taiga", "grass_block", false);
        CHECK(result.known == "11111111");
        CHECK(result.changed == 2476);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -7610696507350976204LL);
        CHECK(result.after == 1198019078);
    }

    SUBCASE("trees_old_growth_pine_taiga") {
        const Run result = run("trees_old_growth_pine_taiga", "grass_block", false);
        CHECK(result.known == "11111111");
        CHECK(result.changed == 1701);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 2913563362951533668LL);
        CHECK(result.after == 1198019078);
    }

    SUBCASE("trees_savanna") {
        const Run result = run("trees_savanna", "grass_block", false);
        CHECK(result.known == "11111111");
        CHECK(result.changed == 682);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -4139024809188141617LL);
        CHECK(result.after == -1337678787);
    }

    SUBCASE("trees_jungle") {
        const Run result = run("trees_jungle", "grass_block", false);
        CHECK(result.known == "11111111");
        CHECK(result.changed == 1051);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -7258561410678710327LL);
        CHECK(result.after == -1337678787);
    }

    SUBCASE("trees_sparse_jungle") {
        const Run result = run("trees_sparse_jungle", "grass_block", false);
        CHECK(result.known == "11111111");
        CHECK(result.changed == 1061);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -6624480636908435584LL);
        CHECK(result.after == -1337678787);
    }

    SUBCASE("trees_windswept_hills") {
        const Run result = run("trees_windswept_hills", "grass_block", false);
        CHECK(result.known == "11111111");
        CHECK(result.changed == 459);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -7337256096575185304LL);
        CHECK(result.after == -1988451487);
    }

    SUBCASE("trees_grove") {
        const Run result = run("trees_grove", "snow_block", true);
        CHECK(result.known == "11111111");
        CHECK(result.changed == 535);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 1760783559485143251LL);
        CHECK(result.after == 146845651);
    }

    SUBCASE("trees_badlands") {
        const Run result = run("trees_badlands", "coarse_dirt", false);
        CHECK(result.known == "11111111");
        CHECK(result.changed == 632);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -3503640851504538688LL);
        CHECK(result.after == -1814055817);
    }

    SUBCASE("trees_meadow") {
        const Run result = run("trees_meadow", "grass_block", false);
        CHECK(result.known == "11111111");
        CHECK(result.changed == 983);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -1813879100266970667LL);
        CHECK(result.after == -1988451487);
    }

    SUBCASE("trees_water") {
        const Run result = run("trees_water", "grass_block", false);
        CHECK(result.known == "11111111");
        CHECK(result.changed == 829);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -7693210298166386365LL);
        CHECK(result.after == -1337678787);
    }

    SUBCASE("trees_flower_forest") {
        const Run result = run("trees_flower_forest", "grass_block", false);
        CHECK(result.known == "11111111");
        CHECK(result.changed == 482);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 3878132744702174713LL);
        CHECK(result.after == 918253638);
    }

    SUBCASE("trees_mangrove") {
        const Run result = run("trees_mangrove", "mud", false);
        CHECK(result.known == "11111111");
        CHECK(result.changed == 4931);
        CHECK(result.deferred == 50);
        CHECK(result.checksum == -4539330670823945458LL);
        CHECK(result.after == -1335941321);
    }

    SUBCASE("trees_cherry") {
        const Run result = run("trees_cherry", "grass_block", false);
        CHECK(result.known == "11111111");
        CHECK(result.changed == 1701);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 3499672078415025782LL);
        CHECK(result.after == -242315500);
    }

    SUBCASE("trees_swamp") {
        const Run result = run("trees_swamp", "grass_block", false);
        CHECK(result.known == "11111111");
        CHECK(result.changed == 1648);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -7831411242672360142LL);
        CHECK(result.after == 148710611);
    }

    SUBCASE("pale_garden_vegetation") {
        const Run result = run("pale_garden_vegetation", "grass_block", false);
        CHECK(result.known == "11111111");
        CHECK(result.changed == 2130);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 4117546124734329109LL);
        CHECK(result.after == -1956739131);
    }

    SUBCASE("dark_forest_vegetation") {
        const Run result = run("dark_forest_vegetation", "grass_block", false);
        CHECK(result.known == "11111111");
        CHECK(result.changed == 960);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 1738603354265559623LL);
        CHECK(result.after == -699117507);
    }

    SUBCASE("trees_birch_on_sand") {
        const Run result = run("trees_birch", "sand", false);
        CHECK(result.known == "11111111");
        CHECK(result.changed == 0);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 7373235885500413069LL);
        CHECK(result.after == -1072559115);
    }

    SUBCASE("unknown_selector") {
        const Run result = run("unknown_selector", "grass_block", false);
        CHECK(result.known == "00000000");
        CHECK(result.changed == 0);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 1762174095308272781LL);
        CHECK(result.after == 28772538);
    }
}

TEST_CASE("the huge mushrooms match the frozen kotlin") {
    SUBCASE("brown") {
        std::uint64_t checksum = 14695981039346656037ULL;
        std::int32_t placedCount = 0;
        rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(24680LL));
        for (std::int32_t index = 0; index < 16; ++index) {
            Chunk chunk = groundChunk(index, -index, index % 4 == 0 ? "stone" : "mycelium");
            if (hugeMushroomFeature::place(chunk, random, index * 16 + 8, 71, -index * 16 + 8,
                                           hugeMushroomFeature::Kind::Brown)) {
                ++placedCount;
            }
            checksum = (checksum ^ static_cast<std::uint64_t>(checksumOf(chunk))) * 1099511628211ULL;
        }
        CHECK(placedCount == 12);
        CHECK(static_cast<std::int64_t>(checksum) == -7861798665196664549LL);
        CHECK(random.nextInt() == 1831488649);
    }

    SUBCASE("red") {
        std::uint64_t checksum = 14695981039346656037ULL;
        std::int32_t placedCount = 0;
        rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(24680LL));
        for (std::int32_t index = 0; index < 16; ++index) {
            Chunk chunk = groundChunk(index, -index, index % 4 == 0 ? "stone" : "mycelium");
            if (hugeMushroomFeature::place(chunk, random, index * 16 + 8, 71, -index * 16 + 8,
                                           hugeMushroomFeature::Kind::Red)) {
                ++placedCount;
            }
            checksum = (checksum ^ static_cast<std::uint64_t>(checksumOf(chunk))) * 1099511628211ULL;
        }
        CHECK(placedCount == 12);
        CHECK(static_cast<std::int64_t>(checksum) == -4969371246147405361LL);
        CHECK(random.nextInt() == 1831488649);
    }
}
