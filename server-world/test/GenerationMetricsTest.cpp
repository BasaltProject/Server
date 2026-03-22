#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/world/GenerationMetrics.hpp>

#include <doctest.h>

#include <string>

using namespace basalt::server::world;
namespace metrics = basalt::server::world::generationMetrics;

namespace {

/// The report is multi-line; the oracle joins it with pipes so one string compares the whole thing.
[[nodiscard]] std::string joined() {
    std::string out = metrics::report();
    for (char& character : out) {
        if (character == '\n') {
            character = '|';
        }
    }
    return out;
}

} // namespace

TEST_CASE("the metrics report matches the frozen kotlin") {
    metrics::reset();
    metrics::setEnabled(false);
    CHECK(joined() == "No chunks generated, nothing to report.");

    metrics::setEnabled(true);
    for (int index = 0; index < 4; ++index) {
        metrics::countChunk();
    }
    for (int index = 0; index < 9; ++index) {
        metrics::countNormalNoise();
    }
    for (int index = 0; index < 3; ++index) {
        metrics::countBiomeLookup();
    }
    for (int index = 0; index < 17; ++index) {
        metrics::countClimateComparison();
    }
    metrics::countHeightmapScan(11);
    metrics::countHeightmapScan(5);
    // Nanoseconds fed straight in, so the report is exact rather than a timing.
    metrics::addPhaseNanos(metrics::Phase::TerrainFill, 12345678LL);
    metrics::addPhaseNanos(metrics::Phase::Decoration, 4000000LL);
    metrics::addPhaseNanos(metrics::Phase::Neighbours, 999LL);
    CHECK(joined() == "Generation metrics over 4 chunk(s), per chunk:|  TERRAIN_FILL   3.086 ms|"
                      "  SURFACE_RULES  0.000 ms|  CARVERS        0.000 ms|  STRUCTURES     0.000 ms|"
                      "  DECORATION     1.000 ms|  SURFACE_FREEZE 0.000 ms|  BIOME_FILL     0.000 ms|"
                      "  NEIGHBOURS     0.000 ms|  NormalNoise evaluations  2|  Biome lookups            0|"
                      "  Climate comparisons      4|  Heightmap column scans   0|  Heightmap scan steps     4");

    int ran = 0;
    metrics::measure(metrics::Phase::Carvers, [&ran] { ++ran; });
    metrics::setEnabled(false);
    metrics::measure(metrics::Phase::Carvers, [&ran] { ++ran; });
    CHECK(ran == 2);

    metrics::reset();
    CHECK(joined() == "No chunks generated, nothing to report.");
}
