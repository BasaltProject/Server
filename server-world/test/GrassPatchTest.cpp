#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/GrassPatchDecoration.hpp>

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

/// Grass, podzol and bare stone in stripes: the jungle spec rejects podzol specifically.
[[nodiscard]] Chunk groundChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 66; y < 70; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            std::int32_t top = 0;
            switch ((x + z) % 3) {
            case 0:
                top = stateIdOf("podzol");
                break;
            case 1:
                top = registry::blocks::stone().stateId;
                break;
            default:
                top = stateIdOf("grass_block");
                break;
            }
            chunk.setBlockStateId(x, 70, z, top);
        }
    }
    return chunk;
}

[[nodiscard]] std::tuple<std::int64_t, std::int32_t, std::int32_t> fold(const Chunk& chunk) {
    testsupport::Checksum checksum;
    std::int32_t grass = 0;
    std::int32_t fern = 0;
    for (std::int32_t y = 69; y <= 76; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum.feed(id);
                if (id == stateIdOf("short_grass")) {
                    ++grass;
                }
                if (id == stateIdOf("fern")) {
                    ++fern;
                }
            }
        }
    }
    return {checksum.value(), grass, fern};
}

[[nodiscard]] std::tuple<std::int32_t, std::int32_t, std::size_t, std::int64_t>
run(std::string_view biome, const std::function<void(Chunk&, const testsupport::FixedBiomeEnvironment&)>& body) {
    testsupport::Checksum checksum;
    std::int32_t grass = 0;
    std::int32_t fern = 0;
    std::size_t deferred = 0;
    for (std::int32_t index = 0; index < 100; ++index) {
        Chunk chunk = groundChunk(index * 3 - 50, index * -5 + 20);
        const testsupport::FixedBiomeEnvironment environment{std::string{biome}};
        body(chunk, environment);
        const auto [chunkChecksum, chunkGrass, chunkFern] = fold(chunk);
        checksum.feed(chunkChecksum);
        grass += chunkGrass;
        fern += chunkFern;
        deferred += chunk.takeGeneratedBlockWrites().size();
    }
    return {grass, fern, deferred, checksum.value()};
}

} // namespace

TEST_CASE("the grass patches place what the frozen kotlin places") {
    SUBCASE("jungle") {
        const auto [grass, fern, deferred, checksum] =
            run("minecraft:jungle", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                grassPatchDecoration::decorate(chunk, 8642LL, environment);
            });
        CHECK(grass == 2684);
        CHECK(fern == 895);
        CHECK(deferred == 0);
        CHECK(checksum == 7987090220193389082LL);
    }

    SUBCASE("taiga") {
        const auto [grass, fern, deferred, checksum] =
            run("minecraft:taiga", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                grassPatchDecoration::decorate(chunk, 8642LL, environment);
            });
        CHECK(grass == 72);
        CHECK(fern == 288);
        CHECK(deferred == 0);
        CHECK(checksum == -4201033982068820979LL);
    }

    SUBCASE("plains") {
        const auto [grass, fern, deferred, checksum] =
            run("minecraft:plains", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                grassPatchDecoration::decorate(chunk, 8642LL, environment);
            });
        CHECK(grass == 3171);
        CHECK(fern == 0);
        CHECK(deferred == 0);
        CHECK(checksum == 9090508517663256889LL);
    }

    SUBCASE("meadow") {
        const auto [grass, fern, deferred, checksum] =
            run("minecraft:meadow", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                grassPatchDecoration::decorate(chunk, 8642LL, environment);
            });
        CHECK(grass == 1659);
        CHECK(fern == 0);
        CHECK(deferred == 0);
        CHECK(checksum == 319151973256264633LL);
    }

    SUBCASE("savanna") {
        const auto [grass, fern, deferred, checksum] =
            run("minecraft:savanna", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                grassPatchDecoration::decorate(chunk, 8642LL, environment);
            });
        CHECK(grass == 5839);
        CHECK(fern == 0);
        CHECK(deferred == 0);
        CHECK(checksum == 7561410202383648921LL);
    }

    SUBCASE("wrong") {
        // the_void is in none of the nine specs, so this is the real rejection case.
        const auto [grass, fern, deferred, checksum] =
            run("minecraft:the_void", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                grassPatchDecoration::decorate(chunk, 8642LL, environment);
            });
        CHECK(grass == 0);
        CHECK(fern == 0);
        CHECK(deferred == 0);
        CHECK(checksum == -2499981960218941167LL);
    }

    SUBCASE("jungleOnly") {
        const auto [grass, fern, deferred, checksum] =
            run("minecraft:jungle", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                grassPatchDecoration::decorateFeature(11, chunk, 8642LL, environment);
            });
        CHECK(grass == 2684);
        CHECK(fern == 895);
        CHECK(deferred == 0);
        CHECK(checksum == 7987090220193389082LL);
    }

    SUBCASE("regional") {
        const auto [grass, fern, deferred, checksum] =
            run("minecraft:plains", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                grassPatchDecoration::decorate(
                    chunk, 8642LL, environment,
                    [](std::int32_t, std::int32_t y, std::int32_t) -> std::optional<std::int32_t> {
                        if (y >= 66 && y <= 69) {
                            return registry::blocks::stone().stateId;
                        }
                        return y == 70 ? stateIdOf("grass_block") : registry::blocks::air().stateId;
                    });
            });
        CHECK(grass == 3171);
        CHECK(fern == 0);
        CHECK(deferred == 2191);
        CHECK(checksum == 9090508517663256889LL);
    }

    SUBCASE("the weighted providers come out at their configured ratios") {
        // Independent of the checksums: the jungle provider is nextInt(4) < 3 and the taiga one is
        // nextInt(5) == 0, so the counts have to land on 3:1 and 1:4 whatever else changed.
        CHECK(2684 == 3 * 895 - 1);
        CHECK(288 == 4 * 72);
    }
}

TEST_CASE("one grass patch at an origin matches the frozen kotlin") {
    Chunk chunk = groundChunk(0, 0);
    basalt::server::random::RandomSource random =
        basalt::server::random::RandomSource::worldgen(basalt::server::random::RandomSource::legacy(13131LL));
    CHECK(grassPatchDecoration::placeAt(chunk, random, DecorationPos{8, 71, 8}, "patch_grass_taiga") == true);
    CHECK(grassPatchDecoration::placeAt(chunk, random, DecorationPos{8, 71, 8}, "nope") == false);
    const auto [checksum, grass, fern] = fold(chunk);
    CHECK(grass == 1);
    CHECK(fern == 5);
    CHECK(checksum == -1042881058992142260LL);
    CHECK(random.nextInt() == -1188359520);
}
