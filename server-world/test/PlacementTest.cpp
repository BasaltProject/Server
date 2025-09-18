#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] Chunk stockedChunk() {
    Chunk chunk{1, 2};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = -64; y < 70 + ((x * 3 + z * 7) % 5); ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::pair<std::string, std::int32_t> run(const std::vector<PlacementModifier>& modifiers) {
    Chunk chunk = stockedChunk();
    basalt::server::random::RandomSource random =
        basalt::server::random::RandomSource::worldgen(basalt::server::random::RandomSource::legacy(12345LL));
    std::string placed;
    placeFrom(chunk, random, DecorationPos{16, 0, 32}, modifiers, [&placed](const DecorationPos& position) {
        placed.append(std::to_string(position.x)).push_back('/');
        placed.append(std::to_string(position.y)).push_back('/');
        placed.append(std::to_string(position.z)).push_back(',');
    });
    // The trailing draw proves the pipeline consumed exactly as many values as vanilla's.
    return {placed, random.nextInt()};
}

} // namespace

TEST_CASE("the placement pipeline scatters and draws exactly as the frozen kotlin does") {
    SUBCASE("count") {
        const std::vector<PlacementModifier> modifiers = {countPlacement(5), inSquarePlacement(), heightmapPlacement()};
        const auto [placed, after] = run(modifiers);
        CHECK(placed == "21/71/40,30/70/46,29/74/32,21/72/33,19/74/32,");
        CHECK(after == 1499439034);
    }

    SUBCASE("nested") {
        const std::vector<PlacementModifier> modifiers = {countPlacement(IntProvider::uniform(2, 6)),
                                                          inSquarePlacement(), heightmapPlacement(), countPlacement(3),
                                                          randomOffsetPlacement(2, 1)};
        const auto [placed, after] = run(modifiers);
        CHECK(placed == "23/72/45,22/73/45,23/72/44,17/73/33,18/73/32,18/73/34,29/75/46,29/74/46,27/74/47,");
        CHECK(after == -2086595122);
    }

    SUBCASE("filters") {
        const std::vector<PlacementModifier> modifiers = {
            countPlacement(12),   inSquarePlacement(),        rarityFilter(2),
            heightmapPlacement(), surfaceWaterDepthFilter(4), constantOffsetPlacement(1, 2)};
        const auto [placed, after] = run(modifiers);
        CHECK(placed == "31/75/46,22/74/34,29/73/38,28/75/38,");
        CHECK(after == 387591904);
    }

    SUBCASE("noise") {
        const std::vector<PlacementModifier> modifiers = {noiseThresholdCountPlacement(-0.8, 5, 10),
                                                          inSquarePlacement(), uniformHeightRangePlacement(-20, 40),
                                                          countExtraPlacement(1, 0.25F, 2)};
        const auto [placed, after] = run(modifiers);
        CHECK(placed == "21/33/40,21/33/40,21/33/40,29/-10/32,19/34/32,19/34/32,19/34/32,23/-19/46,18/38/46,31/-6/"
                        "34,28/-18/37,21/6/36,30/-19/38,30/-19/38,30/-19/38,17/-2/46,");
        CHECK(after == 441414792);
    }

    SUBCASE("weighted") {
        const std::vector<PlacementModifier> modifiers = {
            countPlacement(IntProvider::weighted({{1, 3}, {4, 1}, {9, 2}})), inSquarePlacement(),
            noiseBasedCountPlacement(3, 40.0, 0.5), heightmapPlacement()};
        const auto [placed, after] = run(modifiers);
        CHECK(placed == "24/72/46,");
        CHECK(after == -355989640);
    }
}
