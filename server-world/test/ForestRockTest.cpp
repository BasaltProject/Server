#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/ForestRockDecoration.hpp>

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

/// Supporting ground under a gap of air: the rock walks down until it finds something it may sit on,
/// so a solid column would never exercise that search and a hollow one would never terminate it.
[[nodiscard]] Chunk terrainChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 60; y < 68; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            // A stripe of non-supporting ground, so the downward walk really has to move.
            chunk.setBlockStateId(x, 68, z, x % 3 == 0 ? stateIdOf("sand") : stateIdOf("grass_block"));
        }
    }
    return chunk;
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk) {
    testsupport::Checksum checksum;
    std::int32_t rock = 0;
    for (std::int32_t y = 58; y <= 76; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum.feed(id);
                if (id == stateIdOf("mossy_cobblestone")) {
                    ++rock;
                }
            }
        }
    }
    return {checksum.value(), rock};
}

[[nodiscard]] std::pair<std::int32_t, std::int64_t> run(std::string_view biome) {
    testsupport::Checksum checksum;
    std::int32_t rock = 0;
    for (std::int32_t index = 0; index < 80; ++index) {
        Chunk chunk = terrainChunk(index * 3 - 40, index * -5 + 15);
        const testsupport::FixedBiomeEnvironment environment{std::string{biome}};
        forestRockDecoration::decorate(chunk, 606LL, environment);
        const auto [chunkChecksum, chunkRock] = fold(chunk);
        checksum.feed(chunkChecksum);
        rock += chunkRock;
    }
    return {rock, checksum.value()};
}

} // namespace

TEST_CASE("the forest rock matches the frozen kotlin") {
    SUBCASE("pineTaiga") {
        const auto [rock, checksum] = run("minecraft:old_growth_pine_taiga");
        CHECK(rock == 1742);
        CHECK(checksum == 1846982856835562088LL);
    }

    SUBCASE("spruceTaiga") {
        const auto [rock, checksum] = run("minecraft:old_growth_spruce_taiga");
        CHECK(rock == 1742);
        CHECK(checksum == 1846982856835562088LL);
    }

    SUBCASE("wrong") {
        // plains is in neither old-growth list, so this is the real rejection case.
        const auto [rock, checksum] = run("minecraft:plains");
        CHECK(rock == 0);
        CHECK(checksum == -9024844322599378571LL);
    }
}

TEST_CASE("the rock blob walks and sizes as the frozen kotlin does") {
    // Sizes vary from a couple of blocks to a couple of dozen, so the three radius draws and the
    // per-step origin drift are both exercised rather than one fixed shape.
    Chunk chunk = terrainChunk(0, 0);
    basalt::server::random::RandomSource random =
        basalt::server::random::RandomSource::worldgen(basalt::server::random::RandomSource::legacy(191919LL));
    std::string sizes;
    for (std::int32_t i = 0; i < 12; ++i) {
        const std::int32_t before = fold(chunk).second;
        forestRockDecoration::placeAt(chunk, random, 2 + i % 12, 72, 3 + (i * 5) % 12);
        sizes.append(std::to_string(fold(chunk).second - before)).push_back(',');
    }
    CHECK(sizes == "2,10,3,10,10,3,6,28,28,7,8,21,");
    CHECK(fold(chunk).first == 4417213654595201512LL);
    CHECK(random.nextInt() == 1247703182);
}
