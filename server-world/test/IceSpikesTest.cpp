#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/IceSpikesDecoration.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using basalt::server::world::testsupport::stateIdOf;
namespace registry = basalt::server::registry;

namespace {

/// Snow over dirt over stone, well above y=50: a spike roots downward until it hits something it may
/// not replace, and its pillar stops at the literal 50 rather than the world floor, so the surface
/// has to sit above that for the run-length logic to run at all. Dirt is replaceable, stone is not,
/// which is what stops the pillar.
[[nodiscard]] Chunk snowChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 40; y < 66; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            for (std::int32_t y = 66; y < 72; ++y) {
                chunk.setBlockStateId(x, y, z, stateIdOf("dirt"));
            }
            chunk.setBlockStateId(x, 72, z, stateIdOf("snow_block"));
        }
    }
    return chunk;
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk) {
    testsupport::Checksum checksum;
    std::int32_t ice = 0;
    for (std::int32_t y = 40; y <= 120; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum.feed(id);
                if (id == stateIdOf("packed_ice")) {
                    ++ice;
                }
            }
        }
    }
    return {checksum.value(), ice};
}

[[nodiscard]] std::pair<std::int32_t, std::int64_t> run(std::string_view biome) {
    testsupport::Checksum checksum;
    std::int32_t ice = 0;
    for (std::int32_t index = 0; index < 60; ++index) {
        Chunk chunk = snowChunk(index * 3 - 30, index * -5 + 10);
        const testsupport::FixedBiomeEnvironment environment{std::string{biome}};
        iceSpikesDecoration::decorate(chunk, 24242LL, environment);
        const auto [chunkChecksum, chunkIce] = fold(chunk);
        checksum.feed(chunkChecksum);
        ice += chunkIce;
    }
    return {ice, checksum.value()};
}

[[nodiscard]] std::pair<bool, std::int32_t> runFeature(std::string_view feature) {
    bool accepted = true;
    std::int32_t total = 0;
    for (std::int32_t index = 0; index < 20; ++index) {
        Chunk chunk = snowChunk(0, 0);
        basalt::server::random::RandomSource random =
            basalt::server::random::RandomSource::worldgen(basalt::server::random::RandomSource::legacy(
                static_cast<std::int64_t>(70707ULL + static_cast<std::uint64_t>(index) * 6364136223846793005ULL)));
        accepted = iceSpikesDecoration::placeAt(chunk, random, DecorationPos{8, 72, 8}, feature);
        total += fold(chunk).second;
    }
    return {accepted, total};
}

} // namespace

TEST_CASE("the ice spikes step matches the frozen kotlin") {
    SUBCASE("iceSpikes") {
        const auto [ice, checksum] = run("minecraft:ice_spikes");
        CHECK(ice == 26374);
        CHECK(checksum == -5722558532901948091LL);
    }

    SUBCASE("wrong") {
        // snowy_plains is not the ice-spikes biome, so this is the real rejection case.
        const auto [ice, checksum] = run("minecraft:snowy_plains");
        CHECK(ice == 0);
        CHECK(checksum == -5036690325832860503LL);
    }
}

TEST_CASE("each ice feature on its own matches the frozen kotlin") {
    SUBCASE("ice_spike") {
        const auto [accepted, ice] = runFeature("ice_spike");
        CHECK(accepted == true);
        CHECK(ice == 3062);
    }

    SUBCASE("ice_patch") {
        const auto [accepted, ice] = runFeature("ice_patch");
        CHECK(accepted == true);
        CHECK(ice == 776);
    }

    SUBCASE("nope") {
        const auto [accepted, ice] = runFeature("nope");
        CHECK(accepted == false);
        CHECK(ice == 0);
    }
}

TEST_CASE("the placed feature names match the frozen kotlin") {
    std::string rendered;
    for (const std::string& id : iceSpikesDecoration::placedIds()) {
        if (!rendered.empty()) {
            rendered.push_back(',');
        }
        rendered.append(id);
    }
    CHECK(rendered == "ice_patch,ice_spike");
}
