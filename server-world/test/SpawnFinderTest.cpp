#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/SyntheticRouter.hpp"

#include <basalt/server/world/climate/SpawnFinder.hpp>
#include <basalt/server/world/terrain/NoiseRouterData.hpp>

#include <doctest.h>

#include <cstdint>
#include <utility>
#include <vector>

using namespace basalt::server::world;
using namespace basalt::server::world::climate;

TEST_CASE("the spawn search spirals to the fittest column") {
    const terrain::NoiseRouter router =
        testsupport::syntheticRouter(0, "0.0", {.fields = testsupport::climateFields()});
    const Sampler sampler{*router.state,  router.temperature, router.vegetation, router.continents,
                          router.erosion, router.depth,       router.ridges};

    SUBCASE("overworld") {
        const auto [x, z] = spawnFinder::findSpawnPosition(sampler);
        CHECK(x == 0);
        CHECK(z == 0);
    }

    SUBCASE("single") {
        // One target only: the second weirdness band is what the two-entry list adds.
        const std::vector<ParameterPoint> single{spawnFinder::overworldTarget().front()};
        const auto [x, z] = spawnFinder::findSpawnPosition(sampler, single);
        CHECK(x == -75);
        CHECK(z == 127);
    }

    SUBCASE("empty") {
        const auto [x, z] = spawnFinder::findSpawnPosition(sampler, {});
        CHECK(x == 0);
        CHECK(z == 0);
    }
}
