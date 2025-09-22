#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/SurfaceFreezingDecoration.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using basalt::server::world::testsupport::stateIdOf;
namespace registry = basalt::server::registry;

namespace {

/// Water pools, grass, podzol, ice and honey stripes at varying heights: ice only forms on exposed
/// water, snow refuses to sit on ice and is forced onto honey, and the snowy property exists only on
/// grass, podzol and mycelium.
[[nodiscard]] Chunk terrainChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            const std::int32_t top = 68 + ((x * 3 + z * 5) % 4);
            for (std::int32_t y = 60; y < top; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            std::int32_t surface = 0;
            switch ((x + z) % 5) {
            case 0:
                surface = registry::blocks::water().stateId;
                break;
            case 1:
                surface = stateIdOf("grass_block");
                break;
            case 2:
                surface = stateIdOf("podzol");
                break;
            case 3:
                surface = stateIdOf("ice");
                break;
            default:
                surface = stateIdOf("honey_block");
                break;
            }
            chunk.setBlockStateId(x, top, z, surface);
        }
    }
    return chunk;
}

[[nodiscard]] std::tuple<std::int32_t, std::int32_t, std::int64_t> fold(const Chunk& chunk) {
    testsupport::Checksum checksum;
    std::int32_t snow = 0;
    std::int32_t ice = 0;
    for (std::int32_t y = 60; y <= 76; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum.feed(id);
                if (id == stateIdOf("snow")) {
                    ++snow;
                }
                if (id == stateIdOf("ice")) {
                    ++ice;
                }
            }
        }
    }
    return {snow, ice, checksum.value()};
}

[[nodiscard]] std::tuple<std::int32_t, std::int32_t, std::int32_t, std::int64_t> run(std::string_view biome) {
    testsupport::Checksum checksum;
    std::int32_t snow = 0;
    std::int32_t ice = 0;
    std::int32_t snowy = 0;
    for (std::int32_t index = 0; index < 40; ++index) {
        Chunk chunk = terrainChunk(index * 3 - 20, index * -5 + 10);
        const testsupport::FixedBiomeEnvironment environment{std::string{biome}};
        surfaceFreezingDecoration::decorate(chunk, environment);
        const auto [chunkSnow, chunkIce, chunkChecksum] = fold(chunk);
        checksum.feed(chunkChecksum);
        snow += chunkSnow;
        ice += chunkIce;
        // The snowy variant of grass and podzol sits one id below the bare state.
        for (std::int32_t y = 60; y <= 76; ++y) {
            for (std::int32_t z = 0; z < 16; ++z) {
                for (std::int32_t x = 0; x < 16; ++x) {
                    const std::int32_t id = chunk.blockStateId(x, y, z);
                    if (id == stateIdOf("grass_block") - 1 || id == stateIdOf("podzol") - 1) {
                        ++snowy;
                    }
                }
            }
        }
    }
    return {snow, ice, snowy, checksum.value()};
}

} // namespace

TEST_CASE("the top-layer freeze matches the frozen kotlin") {
    SUBCASE("snowyPlains") {
        const auto [snow, ice, snowy, checksum] = run("minecraft:snowy_plains");
        CHECK(snow == 6120);
        CHECK(ice == 4120);
        CHECK(snowy == 4080);
        CHECK(checksum == -1283577860500779859LL);
    }

    SUBCASE("frozenOcean") {
        const auto [snow, ice, snowy, checksum] = run("minecraft:frozen_ocean");
        CHECK(snow == 2149);
        CHECK(ice == 2784);
        CHECK(snowy == 1423);
        CHECK(checksum == -4846886339566156235LL);
    }

    SUBCASE("taiga") {
        const auto [snow, ice, snowy, checksum] = run("minecraft:taiga");
        CHECK(snow == 0);
        CHECK(ice == 2040);
        CHECK(snowy == 0);
        CHECK(checksum == -8385240389407724571LL);
    }

    SUBCASE("plains") {
        const auto [snow, ice, snowy, checksum] = run("minecraft:plains");
        CHECK(snow == 0);
        CHECK(ice == 2040);
        CHECK(snowy == 0);
        CHECK(checksum == -8385240389407724571LL);
    }

    SUBCASE("desert") {
        // Warm biomes reach the same result by different branches: plains and taiga fail the
        // temperature test, desert fails the precipitation test before it.
        const auto [snow, ice, snowy, checksum] = run("minecraft:desert");
        CHECK(snow == 0);
        CHECK(ice == 2040);
        CHECK(snowy == 0);
        CHECK(checksum == -8385240389407724571LL);
    }

    SUBCASE("only a cold biome adds ice to what the fixture already had") {
        // Independent of the checksums: the fixture lays down its own ice stripe, so a warm biome
        // has to leave that count untouched while a cold one adds newly frozen water on top.
        const auto [warmSnow, warmIce, warmSnowy, warmChecksum] = run("minecraft:plains");
        const auto [coldSnow, coldIce, coldSnowy, coldChecksum] = run("minecraft:snowy_plains");
        CHECK(warmIce == 2040);
        CHECK(coldIce > warmIce);
        CHECK(warmSnow == 0);
        CHECK(coldSnow > 0);
    }
}
