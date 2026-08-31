#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/SurfacePatches.hpp>

#include <doctest.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using basalt::server::world::testsupport::stateIdOf;
namespace registry = basalt::server::registry;

namespace {

/// Grass on land, water in a basin, so both patches have ground they accept.
[[nodiscard]] Chunk surfaceChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = -64; y < 70; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            const bool basin = x >= 4 && x <= 11 && z >= 4 && z <= 11;
            chunk.setBlockStateId(x, 70, z, basin ? registry::blocks::water().stateId : stateIdOf("grass_block"));
        }
    }
    return chunk;
}

[[nodiscard]] std::tuple<std::int32_t, std::size_t, std::int64_t>
run(std::string_view biome, const std::function<void(Chunk&, const testsupport::FixedBiomeEnvironment&)>& body) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t placed = 0;
    std::size_t deferred = 0;
    for (std::int32_t index = 0; index < 24; ++index) {
        Chunk chunk = surfaceChunk(index * 3 - 20, 7 - index * 2);
        const testsupport::FixedBiomeEnvironment environment{std::string{biome}};
        body(chunk, environment);
        for (std::int32_t y = 69; y <= 74; ++y) {
            for (std::int32_t z = 0; z < 16; ++z) {
                for (std::int32_t x = 0; x < 16; ++x) {
                    const std::int32_t id = chunk.blockStateId(x, y, z);
                    checksum =
                        (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                    if (!registry::blocks::isAir(id) && id != registry::blocks::stone().stateId &&
                        id != stateIdOf("grass_block") && id != registry::blocks::water().stateId) {
                        ++placed;
                    }
                }
            }
        }
        deferred += chunk.takeGeneratedBlockWrites().size();
    }
    return {placed, deferred, static_cast<std::int64_t>(checksum)};
}

} // namespace

TEST_CASE("the step-9 surface patches place what the frozen kotlin places") {
    SUBCASE("waterlily") {
        const auto [placed, deferred, checksum] =
            run("minecraft:swamp", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                waterlilyPatchDecoration::decorate(chunk, 918273LL, environment);
            });
        CHECK(placed == 44);
        CHECK(deferred == 0);
        CHECK(checksum == -5883585629045972875LL);
    }

    SUBCASE("waterlilyWrongBiome") {
        const auto [placed, deferred, checksum] =
            run("minecraft:plains", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                waterlilyPatchDecoration::decorate(chunk, 918273LL, environment);
            });
        CHECK(placed == 0);
        CHECK(deferred == 0);
        CHECK(checksum == -7911423744619397851LL);
    }

    SUBCASE("leafLitter") {
        const auto [placed, deferred, checksum] =
            run("minecraft:dark_forest", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                leafLitterPatchDecoration::decorate(chunk, 918273LL, environment);
            });
        CHECK(placed == 145);
        CHECK(deferred == 0);
        CHECK(checksum == -3524764549178857153LL);
    }

    SUBCASE("waterlilyRegional") {
        const auto [placed, deferred, checksum] =
            run("minecraft:swamp", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                waterlilyPatchDecoration::decorate(
                    chunk, 918273LL, environment,
                    [](std::int32_t, std::int32_t y, std::int32_t) -> std::optional<std::int32_t> {
                        if (y < 70) {
                            return registry::blocks::stone().stateId;
                        }
                        return y == 70 ? registry::blocks::water().stateId : registry::blocks::air().stateId;
                    });
            });
        CHECK(placed == 44);
        CHECK(deferred == 76);
        CHECK(checksum == -5883585629045972875LL);
    }

    SUBCASE("leafLitterRegional") {
        const auto [placed, deferred, checksum] =
            run("minecraft:dark_forest", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                leafLitterPatchDecoration::decorate(
                    chunk, 918273LL, environment,
                    [](std::int32_t, std::int32_t y, std::int32_t) -> std::optional<std::int32_t> {
                        if (y < 70) {
                            return registry::blocks::stone().stateId;
                        }
                        return y == 70 ? stateIdOf("grass_block") : registry::blocks::air().stateId;
                    });
            });
        CHECK(placed == 151);
        CHECK(deferred == 125);
        CHECK(checksum == -3303329974070132379LL);
    }
}

TEST_CASE("one waterlily patch at an origin matches the frozen kotlin") {
    Chunk chunk = surfaceChunk(0, 0);
    basalt::server::random::RandomSource random =
        basalt::server::random::RandomSource::worldgen(basalt::server::random::RandomSource::legacy(4242LL));
    waterlilyPatchDecoration::placeAt(chunk, random, DecorationPos{8, 71, 8});
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t y = 69; y <= 74; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                checksum =
                    (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(chunk.blockStateId(x, y, z)))) *
                    1099511628211ULL;
            }
        }
    }
    CHECK(static_cast<std::int64_t>(checksum) == -6559026058476006275LL);
    CHECK(random.nextInt() == -937986852);
}
