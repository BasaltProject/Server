#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/AquaticPatchDecoration.hpp>

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

/// A sea floor under a water column: every aquatic spec plants on the ocean floor and needs water
/// above it, so a dry chunk would exercise nothing but the water test.
[[nodiscard]] Chunk seaChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 40; y < 50; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            // A magma stripe: seagrass and kelp both refuse it specifically.
            if (x % 7 == 0) {
                chunk.setBlockStateId(x, 49, z, stateIdOf("magma_block"));
            }
            for (std::int32_t y = 50; y <= 62; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::water().stateId);
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::tuple<std::int64_t, std::int32_t, std::int32_t> fold(const Chunk& chunk) {
    testsupport::Checksum checksum;
    std::int32_t plants = 0;
    std::int32_t kelp = 0;
    for (std::int32_t y = 48; y <= 66; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum.feed(id);
                if (registry::blockStateFamilies::isKelpState(id) || id == stateIdOf("kelp_plant")) {
                    ++kelp;
                } else if (id != registry::blocks::water().stateId && id != registry::blocks::stone().stateId &&
                           id != stateIdOf("magma_block") && !registry::blocks::isAir(id)) {
                    ++plants;
                }
            }
        }
    }
    return {checksum.value(), plants, kelp};
}

[[nodiscard]] std::tuple<std::int32_t, std::int32_t, std::size_t, std::int64_t>
run(std::string_view biome, const std::function<void(Chunk&, const testsupport::FixedBiomeEnvironment&)>& body) {
    testsupport::Checksum checksum;
    std::int32_t plants = 0;
    std::int32_t kelp = 0;
    std::size_t deferred = 0;
    for (std::int32_t index = 0; index < 80; ++index) {
        Chunk chunk = seaChunk(index * 3 - 40, index * -5 + 15);
        const testsupport::FixedBiomeEnvironment environment{std::string{biome}};
        body(chunk, environment);
        const auto [chunkChecksum, chunkPlants, chunkKelp] = fold(chunk);
        checksum.feed(chunkChecksum);
        plants += chunkPlants;
        kelp += chunkKelp;
        deferred += chunk.takeGeneratedBlockWrites().size();
    }
    return {plants, kelp, deferred, checksum.value()};
}

} // namespace

TEST_CASE("the aquatic patches place what the 26.3 placements place") {
    SUBCASE("warmOcean") {
        const auto [plants, kelp, deferred, checksum] =
            run("minecraft:warm_ocean", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                aquaticPatchDecoration::decorate(chunk, 5432LL, environment);
            });
        CHECK(plants == 3602);
        CHECK(kelp == 0);
        CHECK(deferred == 0);
        CHECK(checksum == -5335921802559124291LL);
    }

    SUBCASE("deepLukewarm") {
        const auto [plants, kelp, deferred, checksum] = run(
            "minecraft:deep_lukewarm_ocean", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                aquaticPatchDecoration::decorate(chunk, 5432LL, environment);
            });
        CHECK(plants == 5069);
        CHECK(kelp == 4146);
        CHECK(deferred == 0);
        CHECK(checksum == 4543588234121041998LL);
    }

    SUBCASE("ocean") {
        const auto [plants, kelp, deferred, checksum] =
            run("minecraft:ocean", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                aquaticPatchDecoration::decorate(chunk, 5432LL, environment);
            });
        CHECK(plants == 2491);
        CHECK(kelp == 6646);
        CHECK(deferred == 0);
        CHECK(checksum == -6110124866394943965LL);
    }

    SUBCASE("river") {
        const auto [plants, kelp, deferred, checksum] =
            run("minecraft:river", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                aquaticPatchDecoration::decorate(chunk, 5432LL, environment);
            });
        CHECK(plants == 2587);
        CHECK(kelp == 0);
        CHECK(deferred == 0);
        CHECK(checksum == -8038645478607242791LL);
    }

    SUBCASE("swamp") {
        const auto [plants, kelp, deferred, checksum] =
            run("minecraft:swamp", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                aquaticPatchDecoration::decorate(chunk, 5432LL, environment);
            });
        CHECK(plants == 3904);
        CHECK(kelp == 0);
        CHECK(deferred == 0);
        CHECK(checksum == 5471351756436167371LL);
    }

    SUBCASE("wrong") {
        // desert is in none of the eleven specs, so this is the real rejection case.
        const auto [plants, kelp, deferred, checksum] =
            run("minecraft:desert", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                aquaticPatchDecoration::decorate(chunk, 5432LL, environment);
            });
        CHECK(plants == 0);
        CHECK(kelp == 0);
        CHECK(deferred == 0);
        CHECK(checksum == 5702334402095245557LL);
    }

    SUBCASE("kelpColdOnly") {
        // More kelp than the full run places: there, the seagrass specs run first and take
        // floor positions the kelp would otherwise have had.
        const auto [plants, kelp, deferred, checksum] =
            run("minecraft:ocean", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                aquaticPatchDecoration::decorateFeature(chunk, 5432LL, environment, "kelp_cold");
            });
        CHECK(plants == 0);
        CHECK(kelp == 7503);
        CHECK(deferred == 0);
        CHECK(checksum == -1711302838034676914LL);
    }

    SUBCASE("seaPickleOnly") {
        const auto [plants, kelp, deferred, checksum] =
            run("minecraft:warm_ocean", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                aquaticPatchDecoration::decorateFeature(chunk, 5432LL, environment, "sea_pickle");
            });
        CHECK(plants == 39);
        CHECK(kelp == 0);
        CHECK(deferred == 0);
        CHECK(checksum == -3165663964546459962LL);
    }

    SUBCASE("regional") {
        const auto [plants, kelp, deferred, checksum] =
            run("minecraft:ocean", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                aquaticPatchDecoration::decorate(
                    chunk, 5432LL, environment,
                    [](std::int32_t, std::int32_t y, std::int32_t) -> std::optional<std::int32_t> {
                        if (y >= 40 && y <= 49) {
                            return registry::blocks::stone().stateId;
                        }
                        return y >= 50 && y <= 62 ? registry::blocks::water().stateId : registry::blocks::air().stateId;
                    });
            });
        CHECK(plants == 2382);
        CHECK(kelp == 6586);
        CHECK(deferred == 1438);
        CHECK(checksum == -4857871477058609813LL);
    }
}
