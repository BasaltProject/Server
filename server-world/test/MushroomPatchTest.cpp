#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/MushroomPatchDecoration.hpp>

#include <doctest.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using basalt::server::world::testsupport::stateIdOf;
namespace registry = basalt::server::registry;

namespace {

/// A stripe of non-solid-render top blocks beside solid ones: mushrooms test isSolidRender, so a
/// uniformly solid floor would never exercise the rejection half of that check.
[[nodiscard]] Chunk groundChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 66; y < 70; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            chunk.setBlockStateId(x, 70, z, x % 4 == 0 ? stateIdOf("glass") : registry::blocks::stone().stateId);
        }
    }
    return chunk;
}

[[nodiscard]] std::tuple<std::int64_t, std::int32_t, std::int32_t> fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t brown = 0;
    std::int32_t red = 0;
    for (std::int32_t y = 69; y <= 76; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                if (id == stateIdOf("brown_mushroom")) {
                    ++brown;
                }
                if (id == stateIdOf("red_mushroom")) {
                    ++red;
                }
            }
        }
    }
    return {static_cast<std::int64_t>(checksum), brown, red};
}

/// The chunk count is per case on purpose: the normal specs are 1/256 and 1/512, so they need a far
/// wider sample than the rest before they fire at all, and making every case pay for that made the
/// suite six times slower for no extra coverage.
[[nodiscard]] std::tuple<std::int32_t, std::int32_t, std::size_t, std::int64_t>
run(std::string_view biome, std::int32_t chunks,
    const std::function<void(Chunk&, const testsupport::FixedBiomeEnvironment&)>& body) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t brown = 0;
    std::int32_t red = 0;
    std::size_t deferred = 0;
    for (std::int32_t index = 0; index < chunks; ++index) {
        Chunk chunk = groundChunk(index * 3 - 70, index * -5 + 25);
        const testsupport::FixedBiomeEnvironment environment{std::string{biome}};
        body(chunk, environment);
        const auto [chunkChecksum, chunkBrown, chunkRed] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        brown += chunkBrown;
        red += chunkRed;
        deferred += chunk.takeGeneratedBlockWrites().size();
    }
    return {brown, red, deferred, static_cast<std::int64_t>(checksum)};
}

} // namespace

TEST_CASE("the mushroom patches place what the frozen kotlin places") {
    SUBCASE("swamp") {
        const auto [brown, red, deferred, checksum] =
            run("minecraft:swamp", 150, [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                mushroomPatchDecoration::decorate(chunk, 424242LL, environment);
            });
        CHECK(brown == 3403);
        CHECK(red == 46);
        CHECK(deferred == 0);
        CHECK(checksum == 5659365285847962345LL);
    }

    SUBCASE("oldGrowth") {
        const auto [brown, red, deferred, checksum] =
            run("minecraft:old_growth_pine_taiga", 150,
                [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                    mushroomPatchDecoration::decorate(chunk, 424242LL, environment);
                });
        CHECK(brown == 1458);
        CHECK(red == 0);
        CHECK(deferred == 0);
        CHECK(checksum == 8883574831473757227LL);
    }

    SUBCASE("taiga") {
        const auto [brown, red, deferred, checksum] =
            run("minecraft:taiga", 150, [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                mushroomPatchDecoration::decorate(chunk, 424242LL, environment);
            });
        CHECK(brown == 408);
        CHECK(red == 0);
        CHECK(deferred == 0);
        CHECK(checksum == 8856817558561670891LL);
    }

    SUBCASE("normal") {
        const auto [brown, red, deferred, checksum] =
            run("minecraft:plains", 900, [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                mushroomPatchDecoration::decorate(chunk, 424242LL, environment);
            });
        CHECK(brown == 57);
        CHECK(red == 14);
        CHECK(deferred == 0);
        CHECK(checksum == 4944419373562959303LL);
    }

    SUBCASE("wrong") {
        // lush_caves is in none of the four lists, so this is the real rejection case.
        const auto [brown, red, deferred, checksum] =
            run("minecraft:lush_caves", 900, [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                mushroomPatchDecoration::decorate(chunk, 424242LL, environment);
            });
        CHECK(brown == 0);
        CHECK(red == 0);
        CHECK(deferred == 0);
        CHECK(checksum == 4823481526664831441LL);
    }

    SUBCASE("before") {
        const auto [brown, red, deferred, checksum] =
            run("minecraft:swamp", 150, [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                mushroomPatchDecoration::decorateBeforeMushroomIsland(chunk, 424242LL, environment);
            });
        CHECK(brown == 3403);
        CHECK(red == 46);
        CHECK(deferred == 0);
        CHECK(checksum == 5659365285847962345LL);
    }

    SUBCASE("after") {
        const auto [brown, red, deferred, checksum] =
            run("minecraft:taiga", 150, [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                mushroomPatchDecoration::decorateAfterMushroomIsland(chunk, 424242LL, environment);
            });
        CHECK(brown == 408);
        CHECK(red == 0);
        CHECK(deferred == 0);
        CHECK(checksum == 8856817558561670891LL);
    }

    SUBCASE("swampBrownOnly") {
        const auto [brown, red, deferred, checksum] =
            run("minecraft:swamp", 150, [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                mushroomPatchDecoration::decorateFeature(chunk, 424242LL, environment, "brown_mushroom_swamp");
            });
        CHECK(brown == 3403);
        CHECK(red == 0);
        CHECK(deferred == 0);
        CHECK(checksum == -5873941266383804341LL);
    }

    SUBCASE("regional") {
        const auto [brown, red, deferred, checksum] =
            run("minecraft:swamp", 150, [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                mushroomPatchDecoration::decorate(
                    chunk, 424242LL, environment,
                    [](std::int32_t, std::int32_t y, std::int32_t) -> std::optional<std::int32_t> {
                        return y >= 66 && y <= 70 ? registry::blocks::stone().stateId : registry::blocks::air().stateId;
                    });
            });
        CHECK(brown == 3403);
        CHECK(red == 46);
        CHECK(deferred == 2009);
        CHECK(checksum == 5659365285847962345LL);
    }
}

TEST_CASE("one mushroom patch at an origin matches the frozen kotlin") {
    Chunk chunk = groundChunk(0, 0);
    basalt::server::random::RandomSource random =
        basalt::server::random::RandomSource::worldgen(basalt::server::random::RandomSource::legacy(777LL));
    CHECK(mushroomPatchDecoration::placeAt(chunk, random, DecorationPos{8, 71, 8}, "red_mushroom_taiga") == true);
    CHECK(mushroomPatchDecoration::placeAt(chunk, random, DecorationPos{8, 71, 8}, "nope") == false);
    const auto [checksum, brown, red] = fold(chunk);
    CHECK(brown == 0);
    CHECK(red == 18);
    CHECK(checksum == 3979032266984473801LL);
    CHECK(random.nextInt() == -289515299);
}

TEST_CASE("the placed feature names match the frozen kotlin") {
    std::string rendered;
    for (const std::string& id : mushroomPatchDecoration::placedIds()) {
        if (!rendered.empty()) {
            rendered.push_back(',');
        }
        rendered.append(id);
    }
    CHECK(rendered == "brown_mushroom_normal,brown_mushroom_old_growth,brown_mushroom_swamp,brown_mushroom_taiga,red_"
                      "mushroom_normal,red_mushroom_old_growth,red_mushroom_swamp,red_mushroom_taiga");
}
