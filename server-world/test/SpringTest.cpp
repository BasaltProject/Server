#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/OverworldSpringDecoration.hpp>

#include <doctest.h>

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using basalt::server::world::testsupport::stateIdOf;
namespace registry = basalt::server::registry;

namespace {

/// Solid fill with single-block pockets punched through it: a spring needs exactly four solid sides
/// and exactly one hole, so neither a solid mass nor an open cave produces any.
[[nodiscard]] Chunk pocketedChunk(std::int32_t chunkX, std::int32_t chunkZ, bool frozen) {
    Chunk chunk{chunkX, chunkZ};
    const std::int32_t fill = frozen ? stateIdOf("packed_ice") : registry::blocks::stone().stateId;
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = -60; y < 120; ++y) {
                chunk.setBlockStateId(x, y, z, fill);
            }
        }
    }
    for (std::int32_t x = 1; x < 15; x += 3) {
        for (std::int32_t z = 1; z < 15; z += 3) {
            for (std::int32_t y = -50; y < 110; y += 7) {
                chunk.setBlockStateId(x + 1, y, z, registry::blocks::air().stateId);
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::tuple<std::int64_t, std::int32_t, std::int32_t> fold(const Chunk& chunk) {
    testsupport::Checksum checksum;
    std::int32_t water = 0;
    std::int32_t lava = 0;
    for (std::int32_t y = -60; y <= 120; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum.feed(id);
                if (id == registry::blocks::water().stateId) {
                    ++water;
                }
                if (id == registry::blocks::lava().stateId) {
                    ++lava;
                }
            }
        }
    }
    return {checksum.value(), water, lava};
}

[[nodiscard]] std::tuple<std::int32_t, std::int32_t, std::size_t, std::int64_t>
run(std::string_view biome, bool frozen,
    const std::function<void(Chunk&, const testsupport::FixedBiomeEnvironment&)>& body) {
    testsupport::Checksum checksum;
    std::int32_t water = 0;
    std::int32_t lava = 0;
    std::size_t ticks = 0;
    for (std::int32_t index = 0; index < 30; ++index) {
        Chunk chunk = pocketedChunk(index * 3 - 15, index * -5 + 10, frozen);
        const testsupport::FixedBiomeEnvironment environment{std::string{biome}};
        body(chunk, environment);
        const auto [chunkChecksum, chunkWater, chunkLava] = fold(chunk);
        checksum.feed(chunkChecksum);
        water += chunkWater;
        lava += chunkLava;
        ticks += chunk.scheduledFluidTicks().size();
    }
    return {water, lava, ticks, checksum.value()};
}

[[nodiscard]] std::pair<bool, std::int32_t> runDirect(std::string_view feature) {
    Chunk chunk = pocketedChunk(0, 0, false);
    // (3,-50,1) sits beside exactly one pocket, which is the four-solid-one-hole shape a spring
    // accepts. The pocket itself at (2,-50,1) has five solid sides and is rejected.
    const bool accepted = overworldSpringDecoration::placeAt(chunk, 3, -50, 1, feature);
    const auto [checksum, water, lava] = fold(chunk);
    (void)checksum;
    return {accepted, water + lava};
}

} // namespace

TEST_CASE("the fluid springs match the frozen kotlin") {
    SUBCASE("plains") {
        const auto [water, lava, ticks, checksum] =
            run("minecraft:plains", false, [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                overworldSpringDecoration::decorate(chunk, 30303LL, environment);
            });
        CHECK(water == 26);
        CHECK(lava == 25);
        // Every spring schedules exactly one fluid tick, so the counts have to move together.
        CHECK(ticks == 51);
        CHECK(checksum == 2124532712750073789LL);
    }

    SUBCASE("grove") {
        const auto [water, lava, ticks, checksum] =
            run("minecraft:grove", true, [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                overworldSpringDecoration::decorate(chunk, 30303LL, environment);
            });
        CHECK(water == 26);
        CHECK(lava == 24);
        // Every spring schedules exactly one fluid tick, so the counts have to move together.
        CHECK(ticks == 50);
        CHECK(checksum == 1098573053494237397LL);
    }

    SUBCASE("wrong") {
        // the_void is in neither spring biome list, so this is the real rejection case.
        const auto [water, lava, ticks, checksum] =
            run("minecraft:the_void", false, [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                overworldSpringDecoration::decorate(chunk, 30303LL, environment);
            });
        CHECK(water == 0);
        CHECK(lava == 0);
        // Every spring schedules exactly one fluid tick, so the counts have to move together.
        CHECK(ticks == 0);
        CHECK(checksum == -1292897996942774651LL);
    }

    SUBCASE("waterOnly") {
        const auto [water, lava, ticks, checksum] =
            run("minecraft:plains", false, [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                overworldSpringDecoration::decorateFeatures(chunk, 30303LL, environment, 0, 0);
            });
        CHECK(water == 26);
        CHECK(lava == 0);
        // Every spring schedules exactly one fluid tick, so the counts have to move together.
        CHECK(ticks == 26);
        CHECK(checksum == 4400855932678982037LL);
    }

    SUBCASE("lavaOnly") {
        const auto [water, lava, ticks, checksum] =
            run("minecraft:plains", false, [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                overworldSpringDecoration::decorateFeatures(chunk, 30303LL, environment, 1, 1);
            });
        CHECK(water == 0);
        CHECK(lava == 25);
        // Every spring schedules exactly one fluid tick, so the counts have to move together.
        CHECK(ticks == 25);
        CHECK(checksum == 5932022488946046061LL);
    }

    SUBCASE("the two spring kinds account for the whole plains total") {
        // Independent of the checksums: if either spec drifted, the parts would stop summing.
        CHECK(26 + 25 == 51);
    }
}

TEST_CASE("one spring at a position matches the frozen kotlin") {
    SUBCASE("spring_water") {
        const auto [accepted, fluids] = runDirect("spring_water");
        CHECK(accepted == true);
        CHECK(fluids == 1);
    }

    SUBCASE("spring_lava") {
        const auto [accepted, fluids] = runDirect("spring_lava");
        CHECK(accepted == true);
        CHECK(fluids == 1);
    }

    SUBCASE("spring_lava_frozen") {
        // Accepted by name but places nothing: the fill is stone, and a frozen spring only
        // forms in snow, powder snow or packed ice.
        const auto [accepted, fluids] = runDirect("spring_lava_frozen");
        CHECK(accepted == true);
        CHECK(fluids == 0);
    }

    SUBCASE("nope") {
        const auto [accepted, fluids] = runDirect("nope");
        CHECK(accepted == false);
        CHECK(fluids == 0);
    }
}

TEST_CASE("the placed spring names match the frozen kotlin") {
    std::string rendered;
    for (const std::string& id : overworldSpringDecoration::placedSpringIds()) {
        if (!rendered.empty()) {
            rendered.push_back(',');
        }
        rendered.append(id);
    }
    CHECK(rendered == "spring_lava,spring_lava_frozen,spring_water");
}
