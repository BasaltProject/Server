#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/FlowerPatchDecoration.hpp>

#include <doctest.h>

#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using basalt::server::world::testsupport::stateIdOf;
namespace registry = basalt::server::registry;

namespace {

/// Grass with a leaf canopy over half of it, so the spec that uses the no-leaves heightmap lands
/// somewhere the others do not.
[[nodiscard]] Chunk groundChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 66; y < 70; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            chunk.setBlockStateId(x, 70, z, stateIdOf("grass_block"));
            if (x < 8) {
                chunk.setBlockStateId(x, 76, z, stateIdOf("oak_leaves"));
            }
        }
    }
    return chunk;
}

[[nodiscard]] bool isPlant(std::int32_t id) {
    return !registry::blocks::isAir(id) && id != registry::blocks::stone().stateId && id != stateIdOf("grass_block") &&
           id != stateIdOf("oak_leaves");
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk) {
    testsupport::Checksum checksum;
    std::int32_t plants = 0;
    for (std::int32_t y = 69; y <= 80; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum.feed(id);
                if (isPlant(id)) {
                    ++plants;
                }
            }
        }
    }
    return {checksum.value(), plants};
}

/// The checksum folds per chunk and then combines, matching the Kotlin oracle's shape.
[[nodiscard]] std::tuple<std::int32_t, std::size_t, std::int64_t>
run(std::string_view biome, const std::function<void(Chunk&, const testsupport::FixedBiomeEnvironment&)>& body) {
    testsupport::Checksum checksum;
    std::int32_t plants = 0;
    std::set<std::int32_t> distinct;
    for (std::int32_t index = 0; index < 80; ++index) {
        Chunk chunk = groundChunk(index * 3 - 40, index * -5 + 15);
        const testsupport::FixedBiomeEnvironment environment{std::string{biome}};
        body(chunk, environment);
        const auto [chunkChecksum, chunkPlants] = fold(chunk);
        checksum.feed(chunkChecksum);
        plants += chunkPlants;
        for (std::int32_t y = 69; y <= 80; ++y) {
            for (std::int32_t z = 0; z < 16; ++z) {
                for (std::int32_t x = 0; x < 16; ++x) {
                    const std::int32_t id = chunk.blockStateId(x, y, z);
                    if (isPlant(id)) {
                        distinct.insert(id);
                    }
                }
            }
        }
    }
    return {plants, distinct.size(), checksum.value()};
}

} // namespace

TEST_CASE("the flower dispatcher places what the frozen kotlin places") {
    SUBCASE("plains") {
        const auto [plants, distinct, checksum] =
            run("minecraft:plains", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                flowerPatchDecoration::decorate(chunk, 31415LL, environment);
            });
        CHECK(plants == 59);
        CHECK(distinct == 5);
        CHECK(checksum == -5934652879681126108LL);
    }

    SUBCASE("flower_forest") {
        const auto [plants, distinct, checksum] =
            run("minecraft:flower_forest", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                flowerPatchDecoration::decorate(chunk, 31415LL, environment);
            });
        CHECK(plants == 1234);
        CHECK(distinct == 16);
        CHECK(checksum == 5149342853149220809LL);
    }

    SUBCASE("pale_garden") {
        const auto [plants, distinct, checksum] =
            run("minecraft:pale_garden", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                flowerPatchDecoration::decorate(chunk, 31415LL, environment);
            });
        CHECK(plants == 175);
        CHECK(distinct == 1);
        CHECK(checksum == -7815929162278799054LL);
    }

    SUBCASE("meadow") {
        const auto [plants, distinct, checksum] =
            run("minecraft:meadow", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                flowerPatchDecoration::decorate(chunk, 31415LL, environment);
            });
        CHECK(plants == 1508);
        CHECK(distinct == 22);
        CHECK(checksum == -6095387782716881028LL);
    }

    SUBCASE("swamp") {
        const auto [plants, distinct, checksum] =
            run("minecraft:swamp", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                flowerPatchDecoration::decorate(chunk, 31415LL, environment);
            });
        CHECK(plants == 14);
        CHECK(distinct == 1);
        CHECK(checksum == 7514673858542546111LL);
    }

    SUBCASE("cherry_grove") {
        const auto [plants, distinct, checksum] =
            run("minecraft:cherry_grove", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                flowerPatchDecoration::decorate(chunk, 31415LL, environment);
            });
        CHECK(plants == 6438);
        CHECK(distinct == 16);
        CHECK(checksum == 4997414353607254742LL);
    }

    SUBCASE("birch_forest") {
        const auto [plants, distinct, checksum] =
            run("minecraft:birch_forest", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                flowerPatchDecoration::decorate(chunk, 31415LL, environment);
            });
        CHECK(plants == 835);
        CHECK(distinct == 19);
        CHECK(checksum == -1353130369538111414LL);
    }

    SUBCASE("savanna") {
        const auto [plants, distinct, checksum] =
            run("minecraft:savanna", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                flowerPatchDecoration::decorate(chunk, 31415LL, environment);
            });
        CHECK(plants == 59);
        CHECK(distinct == 2);
        CHECK(checksum == -4747357365955432518LL);
    }

    SUBCASE("taiga") {
        const auto [plants, distinct, checksum] =
            run("minecraft:taiga", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                flowerPatchDecoration::decorate(chunk, 31415LL, environment);
            });
        CHECK(plants == 0);
        CHECK(distinct == 0);
        CHECK(checksum == 7031911460422995829LL);
    }

    SUBCASE("wrong") {
        // the_void is in none of the thirteen biome lists, so this is the real rejection case.
        const auto [plants, distinct, checksum] =
            run("minecraft:the_void", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                flowerPatchDecoration::decorate(chunk, 31415LL, environment);
            });
        CHECK(plants == 0);
        CHECK(distinct == 0);
        CHECK(checksum == 7031911460422995829LL);
    }

    SUBCASE("only_flower_default") {
        const auto [plants, distinct, checksum] =
            run("minecraft:taiga", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                flowerPatchDecoration::decorateFeature(chunk, 31415LL, environment, "flower_default");
            });
        CHECK(plants == 0);
        CHECK(distinct == 0);
        CHECK(checksum == 7031911460422995829LL);
    }

    SUBCASE("only_flower_plains") {
        const auto [plants, distinct, checksum] =
            run("minecraft:plains", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                flowerPatchDecoration::decorateFeature(chunk, 31415LL, environment, "flower_plains");
            });
        CHECK(plants == 59);
        CHECK(distinct == 5);
        CHECK(checksum == -5934652879681126108LL);
    }

    SUBCASE("only_wildflowers_meadow") {
        const auto [plants, distinct, checksum] =
            run("minecraft:meadow", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                flowerPatchDecoration::decorateFeature(chunk, 31415LL, environment, "wildflowers_meadow");
            });
        CHECK(plants == 714);
        CHECK(distinct == 16);
        CHECK(checksum == -7754519945110039281LL);
    }

    SUBCASE("only_pale_garden_flowers") {
        const auto [plants, distinct, checksum] =
            run("minecraft:pale_garden", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                flowerPatchDecoration::decorateFeature(chunk, 31415LL, environment, "pale_garden_flowers");
            });
        CHECK(plants == 174);
        CHECK(distinct == 1);
        CHECK(checksum == -1160043390819255525LL);
    }
}
