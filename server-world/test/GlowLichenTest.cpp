#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/GlowLichenDecoration.hpp>

#include <doctest.h>

#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <tuple>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using basalt::server::world::testsupport::stateIdOf;
namespace registry = basalt::server::registry;

namespace {

/// Stone with open shafts through it, and a water column in one: lichen attaches to the shaft walls,
/// so a solid mass gives it nowhere to sit and open air gives it nothing to attach to. The water
/// exercises the waterlogged state, which is a different bit than the faces.
[[nodiscard]] Chunk shaftChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = -60; y < 100; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
        }
    }
    for (std::int32_t x = 2; x < 15; x += 4) {
        for (std::int32_t z = 2; z < 15; z += 4) {
            for (std::int32_t y = -55; y < 95; ++y) {
                chunk.setBlockStateId(
                    x, y, z, x == 2 && z == 2 ? registry::blocks::water().stateId : registry::blocks::air().stateId);
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::tuple<std::int64_t, std::int32_t, std::size_t> fold(const Chunk& chunk) {
    testsupport::Checksum checksum;
    std::int32_t lichen = 0;
    std::set<std::int32_t> distinct;
    for (std::int32_t y = -60; y <= 100; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum.feed(id);
                if (registry::blockStateFamilies::isGlowLichenState(id)) {
                    ++lichen;
                    distinct.insert(id);
                }
            }
        }
    }
    return {checksum.value(), lichen, distinct.size()};
}

[[nodiscard]] std::tuple<std::int32_t, std::size_t, std::int64_t> run(std::string_view biome) {
    testsupport::Checksum checksum;
    std::int32_t lichen = 0;
    std::size_t distinct = 0;
    for (std::int32_t index = 0; index < 25; ++index) {
        Chunk chunk = shaftChunk(index * 3 - 12, index * -5 + 8);
        const testsupport::FixedBiomeEnvironment environment{std::string{biome}};
        glowLichenDecoration::decorate(chunk, 515151LL, environment);
        const auto [chunkChecksum, chunkLichen, chunkDistinct] = fold(chunk);
        checksum.feed(chunkChecksum);
        lichen += chunkLichen;
        distinct += chunkDistinct;
    }
    return {lichen, distinct, checksum.value()};
}

} // namespace

TEST_CASE("the glow lichen growth matches the frozen kotlin") {
    SUBCASE("lush_caves") {
        // The distinct count matters as much as the total: each lichen carries a set of face
        // bits, so a port that always wrote one face would still place the right number.
        const auto [lichen, distinct, checksum] = run("minecraft:lush_caves");
        CHECK(lichen == 99);
        CHECK(distinct == 62);
        CHECK(checksum == 8335974245601930525LL);
    }

    SUBCASE("the_void") {
        // the_void is the one biome the feature refuses outright.
        const auto [lichen, distinct, checksum] = run("minecraft:the_void");
        CHECK(lichen == 0);
        CHECK(distinct == 0);
        CHECK(checksum == -2809108535340428696LL);
    }
}

TEST_CASE("one growth at a shaft wall matches the frozen kotlin") {
    // Some attempts place one block and some two: the second is the spread step, which only fires on
    // a coin flip, so a run of all ones would mean it never ran.
    std::int32_t placed = 0;
    std::string states;
    for (std::int32_t index = 0; index < 24; ++index) {
        Chunk chunk = shaftChunk(0, 0);
        basalt::server::random::RandomSource random =
            basalt::server::random::RandomSource::worldgen(basalt::server::random::RandomSource::legacy(
                static_cast<std::int64_t>(6161ULL + static_cast<std::uint64_t>(index) * 6364136223846793005ULL)));
        if (glowLichenDecoration::placeAt(chunk, random, DecorationPos{2, -50 + index, 6})) {
            ++placed;
        }
        const auto [checksum, lichen, distinct] = fold(chunk);
        (void)checksum;
        (void)distinct;
        states.append(std::to_string(lichen)).push_back(',');
    }
    CHECK(placed == 24);
    CHECK(states == "1,1,1,1,2,2,2,2,1,1,1,1,1,1,1,1,1,1,2,1,1,1,1,1,");
}
