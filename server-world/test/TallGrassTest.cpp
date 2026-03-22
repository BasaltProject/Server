#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/TallGrassDecoration.hpp>

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

/// Grass ground with a low ceiling over one stripe, so the upper-half rejection really runs.
[[nodiscard]] Chunk groundChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 66; y < 70; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            chunk.setBlockStateId(x, 70, z, x % 5 == 0 ? registry::blocks::stone().stateId : stateIdOf("grass_block"));
            if (z % 6 == 0) {
                chunk.setBlockStateId(x, 72, z, registry::blocks::stone().stateId);
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::tuple<std::int64_t, std::int32_t, std::int32_t> fold(const Chunk& chunk) {
    testsupport::Checksum checksum;
    std::int32_t lower = 0;
    std::int32_t halves = 0;
    for (std::int32_t y = 69; y <= 78; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum.feed(id);
                if (registry::blockStateFamilies::isDoublePlantState(id)) {
                    ++halves;
                    if (id == stateIdOf("tall_grass")) {
                        ++lower;
                    }
                }
            }
        }
    }
    return {checksum.value(), lower, halves};
}

[[nodiscard]] std::tuple<std::int32_t, std::int32_t, std::size_t, std::int64_t>
run(std::string_view biome, const std::function<void(Chunk&, const testsupport::FixedBiomeEnvironment&)>& body) {
    testsupport::Checksum checksum;
    std::int32_t lower = 0;
    std::int32_t halves = 0;
    std::size_t deferred = 0;
    for (std::int32_t index = 0; index < 120; ++index) {
        Chunk chunk = groundChunk(index * 3 - 60, index * -5 + 25);
        const testsupport::FixedBiomeEnvironment environment{std::string{biome}};
        body(chunk, environment);
        const auto [chunkChecksum, chunkLower, chunkHalves] = fold(chunk);
        checksum.feed(chunkChecksum);
        lower += chunkLower;
        halves += chunkHalves;
        deferred += chunk.takeGeneratedBlockWrites().size();
    }
    return {lower, halves, deferred, checksum.value()};
}

} // namespace

TEST_CASE("the tall grass patches place what the frozen kotlin places") {
    SUBCASE("savanna") {
        const auto [lower, halves, deferred, checksum] =
            run("minecraft:savanna", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                tallGrassDecoration::decorate(chunk, 24680LL, environment);
            });
        CHECK(lower == 190);
        // Both halves always land together, so this pins the upper write as well as the lower.
        CHECK(halves == 380);
        CHECK(deferred == 0);
        CHECK(checksum == -2189456718603499799LL);
    }

    SUBCASE("plains") {
        const auto [lower, halves, deferred, checksum] =
            run("minecraft:plains", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                tallGrassDecoration::decorate(chunk, 24680LL, environment);
            });
        CHECK(lower == 215);
        // Both halves always land together, so this pins the upper write as well as the lower.
        CHECK(halves == 430);
        CHECK(deferred == 0);
        CHECK(checksum == -5110387324682303692LL);
    }

    SUBCASE("meadow") {
        const auto [lower, halves, deferred, checksum] =
            run("minecraft:meadow", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                tallGrassDecoration::decorate(chunk, 24680LL, environment);
            });
        CHECK(lower == 215);
        // Both halves always land together, so this pins the upper write as well as the lower.
        CHECK(halves == 430);
        CHECK(deferred == 0);
        CHECK(checksum == -5110387324682303692LL);
    }

    SUBCASE("wrong") {
        // desert is in neither list, so this is the real rejection case.
        const auto [lower, halves, deferred, checksum] =
            run("minecraft:desert", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                tallGrassDecoration::decorate(chunk, 24680LL, environment);
            });
        CHECK(lower == 0);
        // Both halves always land together, so this pins the upper write as well as the lower.
        CHECK(halves == 0);
        CHECK(deferred == 0);
        CHECK(checksum == -3214159528747379075LL);
    }

    SUBCASE("savannaOnly") {
        const auto [lower, halves, deferred, checksum] =
            run("minecraft:savanna", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                tallGrassDecoration::decorateFeature(11, chunk, 24680LL, environment);
            });
        CHECK(lower == 190);
        // Both halves always land together, so this pins the upper write as well as the lower.
        CHECK(halves == 380);
        CHECK(deferred == 0);
        CHECK(checksum == -2189456718603499799LL);
    }

    SUBCASE("plainOnly") {
        const auto [lower, halves, deferred, checksum] =
            run("minecraft:plains", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                tallGrassDecoration::decorateFeature(29, chunk, 24680LL, environment);
            });
        CHECK(lower == 215);
        // Both halves always land together, so this pins the upper write as well as the lower.
        CHECK(halves == 430);
        CHECK(deferred == 0);
        CHECK(checksum == -5110387324682303692LL);
    }

    SUBCASE("regional") {
        const auto [lower, halves, deferred, checksum] =
            run("minecraft:plains", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                tallGrassDecoration::decorate(
                    chunk, 24680LL, environment,
                    [](std::int32_t, std::int32_t y, std::int32_t) -> std::optional<std::int32_t> {
                        if (y >= 66 && y <= 69) {
                            return registry::blocks::stone().stateId;
                        }
                        return y == 70 ? stateIdOf("grass_block") : registry::blocks::air().stateId;
                    });
            });
        CHECK(lower == 215);
        // Both halves always land together, so this pins the upper write as well as the lower.
        CHECK(halves == 430);
        CHECK(deferred == 320);
        CHECK(checksum == -5110387324682303692LL);
    }
}

TEST_CASE("one tall grass patch at an origin matches the frozen kotlin") {
    Chunk chunk = groundChunk(0, 0);
    basalt::server::random::RandomSource random =
        basalt::server::random::RandomSource::worldgen(basalt::server::random::RandomSource::legacy(97531LL));
    tallGrassDecoration::placeAt(chunk, random, DecorationPos{8, 71, 8});
    const auto [checksum, lower, halves] = fold(chunk);
    CHECK(lower == 17);
    CHECK(halves == 34);
    CHECK(checksum == -8649927065220509924LL);
    CHECK(random.nextInt() == 2076158532);
}
