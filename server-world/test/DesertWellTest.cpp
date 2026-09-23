#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/DesertWellDecoration.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <tuple>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using basalt::server::world::testsupport::stateIdOf;
namespace registry = basalt::server::registry;

namespace {

/// Sand over stone, with a hollow carved under one corner: the well refuses to build where any
/// column of its 5 by 5 footprint has two air blocks beneath, so a solid plain would never reach
/// that abort.
[[nodiscard]] Chunk sandChunk(std::int32_t chunkX, std::int32_t chunkZ, bool hollow) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 60; y < 70; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            chunk.setBlockStateId(x, 70, z, stateIdOf("sand"));
        }
    }
    if (hollow) {
        for (std::int32_t x = 4; x <= 6; ++x) {
            for (std::int32_t z = 4; z <= 6; ++z) {
                chunk.setBlockStateId(x, 69, z, registry::blocks::air().stateId);
                chunk.setBlockStateId(x, 68, z, registry::blocks::air().stateId);
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::tuple<std::int64_t, std::int32_t, std::int32_t> fold(const Chunk& chunk) {
    testsupport::Checksum checksum;
    std::int32_t sandstone = 0;
    std::int32_t suspicious = 0;
    for (std::int32_t y = 60; y <= 80; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum.feed(id);
                if (id == stateIdOf("sandstone") || id == stateIdOf("sandstone_slab")) {
                    ++sandstone;
                }
                if (id == stateIdOf("suspicious_sand")) {
                    ++suspicious;
                }
            }
        }
    }
    return {checksum.value(), sandstone, suspicious};
}

[[nodiscard]] std::tuple<std::int32_t, std::int32_t, std::int32_t, std::string, std::int64_t> run(bool hollow) {
    testsupport::Checksum checksum;
    std::int32_t sandstone = 0;
    std::int32_t suspicious = 0;
    std::int32_t entities = 0;
    std::string seeds;
    for (std::int32_t index = 0; index < 30; ++index) {
        Chunk chunk = sandChunk(index * 3 - 15, index * -5 + 10, hollow);
        basalt::server::random::RandomSource random =
            basalt::server::random::RandomSource::worldgen(basalt::server::random::RandomSource::legacy(
                static_cast<std::int64_t>(818181ULL + static_cast<std::uint64_t>(index) * 6364136223846793005ULL)));
        // `placeAt` takes world coordinates, like the placed-feature registry hands it.
        desertWellDecoration::placeAt(chunk, random, chunk.chunkX() * 16 + 5, 70, chunk.chunkZ() * 16 + 5);
        const auto [chunkChecksum, chunkSandstone, chunkSuspicious] = fold(chunk);
        checksum.feed(chunkChecksum);
        sandstone += chunkSandstone;
        suspicious += chunkSuspicious;
        for (const ChunkBlockEntity& entity : chunk.blockEntities()) {
            ++entities;
            seeds.append(std::to_string(entity.data().getLong("LootTableSeed"))).push_back(',');
        }
    }
    return {sandstone, suspicious, entities, seeds, checksum.value()};
}

} // namespace

TEST_CASE("the desert well matches the frozen kotlin") {
    SUBCASE("solid") {
        const auto [sandstone, suspicious, entities, seeds, checksum] = run(false);
        CHECK(sandstone == 3030);
        CHECK(suspicious == 60);
        // Every buried sand carries exactly one brushable block entity.
        CHECK(entities == 60);
        CHECK(seeds ==
              "-64321429548987,-64596307455932,-51127290343355,-51402168250300,-38482906951611,-38208029044668,-"
              "24739011936187,-24739011927996,-11819750633403,-11819750633404,1649266475077,1649266479172,"
              "15118283591749,14568527777860,28037544890437,27762666983492,41506562003013,40956806189124,"
              "54425823297605,54700701208644,67894840414277,67345084600388,80814101708869,80814101708868,"
              "94008240918597,94008240918596,107202380128325,107202380124228,120671397236805,120396519333956,"
              "133590658531397,133590658531396,146784797741125,146509919834180,159978936950853,159978936942660,"
              "172898198245445,172898198245444,186367215353925,186367215362116,199561354559557,199561354559556,"
              "212480615862341,212755493765188,225949632974917,225949632974916,239143772184645,239143772180548,"
              "252612789293125,252063033479236,265532050587717,265257172684868,278726189793349,278726189801540,"
              "291920329007173,291645451096132,305114468208709,305389346115652,318033729507397,318583485321284,");
        CHECK(checksum == 8062633070561909411LL);
    }

    SUBCASE("hollow") {
        // A hollow under the footprint aborts the whole well, so nothing at all is written.
        const auto [sandstone, suspicious, entities, seeds, checksum] = run(true);
        CHECK(sandstone == 0);
        CHECK(suspicious == 0);
        // Every buried sand carries exactly one brushable block entity.
        CHECK(entities == 0);
        CHECK(seeds == "");
        CHECK(checksum == 8470561932817895187LL);
    }
}
