#include <basalt/server/world/GenerationMetrics.hpp>

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace basalt::server::world {
namespace {

constexpr std::array<std::string_view, generationMetrics::kPhaseCount> kPhaseNames{
    "TERRAIN_FILL", "SURFACE_RULES",  "CARVERS",    "STRUCTURES",
    "DECORATION",   "SURFACE_FREEZE", "BIOME_FILL", "NEIGHBOURS"};

/// The names the operator sees, kept in the frozen Kotlin's spelling so a saved report still reads
/// the same.
constexpr std::array<std::string_view, 6> kMarkerNames{"INTERPOLATED", "FLAT_CACHE",        "CACHE_2D",
                                                       "CACHE_ONCE",   "CACHE_ALL_IN_CELL", "BLEND_DENSITY"};

struct Counters {
    bool enabled = false;
    std::array<std::int64_t, generationMetrics::kPhaseCount> phaseNanos{};
    std::array<std::int64_t, kMarkerNames.size()> markerHits{};
    std::int64_t chunks = 0;
    std::int64_t normalNoiseEvaluations = 0;
    std::int64_t biomeLookups = 0;
    std::int64_t biomeCandidatesScanned = 0;
    std::int64_t heightmapScans = 0;
    std::int64_t heightmapScanSteps = 0;
};

Counters& counters() {
    static Counters state;
    return state;
}

[[nodiscard]] std::string padded(std::string_view name, std::size_t width) {
    std::string out{name};
    out.resize(std::max(out.size(), width), ' ');
    return out;
}

/// Three decimals without a locale or a stream, which is what the frozen Kotlin printed.
[[nodiscard]] std::string format(double value) {
    const auto scaled = static_cast<std::int64_t>(value * 1000.0);
    std::string fraction = std::to_string(scaled % 1000);
    fraction.insert(0, 3 - fraction.size(), '0');
    return std::to_string(scaled / 1000) + "." + fraction;
}

} // namespace

bool generationMetrics::enabled() noexcept { return counters().enabled; }

void generationMetrics::setEnabled(const bool value) noexcept { counters().enabled = value; }

void generationMetrics::countChunk() noexcept { ++counters().chunks; }

void generationMetrics::countNormalNoise() noexcept { ++counters().normalNoiseEvaluations; }

void generationMetrics::countHeightmapScan(const std::int32_t steps) noexcept {
    ++counters().heightmapScans;
    counters().heightmapScanSteps += steps;
}

void generationMetrics::countBiomeLookup() noexcept { ++counters().biomeLookups; }

void generationMetrics::countClimateComparison() noexcept { ++counters().biomeCandidatesScanned; }

void generationMetrics::countMarker(const densityfunction::MarkerType type) noexcept {
    ++counters().markerHits[static_cast<std::size_t>(type)];
}

void generationMetrics::addPhaseNanos(const Phase phase, const std::int64_t nanos) noexcept {
    counters().phaseNanos[static_cast<std::size_t>(phase)] += nanos;
}

void generationMetrics::reset() noexcept {
    Counters& state = counters();
    const bool wasEnabled = state.enabled;
    state = Counters{};
    state.enabled = wasEnabled;
}

std::string generationMetrics::report() {
    const Counters& state = counters();
    if (state.chunks == 0) {
        return "No chunks generated, nothing to report.";
    }
    const auto chunks = static_cast<double>(state.chunks);
    std::vector<std::string> lines{"Generation metrics over " + std::to_string(state.chunks) + " chunk(s), per chunk:"};
    for (std::size_t index = 0; index < kPhaseNames.size(); ++index) {
        const double millis = static_cast<double>(state.phaseNanos[index]) / 1'000'000.0 / chunks;
        lines.push_back("  " + padded(kPhaseNames[index], 14) + " " + format(millis) + " ms");
    }
    lines.push_back("  NormalNoise evaluations  " + std::to_string(state.normalNoiseEvaluations / state.chunks));
    lines.push_back("  Biome lookups            " + std::to_string(state.biomeLookups / state.chunks));
    lines.push_back("  Climate comparisons      " + std::to_string(state.biomeCandidatesScanned / state.chunks));
    lines.push_back("  Heightmap column scans   " + std::to_string(state.heightmapScans / state.chunks));
    lines.push_back("  Heightmap scan steps     " + std::to_string(state.heightmapScanSteps / state.chunks));
    lines.emplace_back("  Cache markers reached:");
    for (std::size_t index = 0; index < kMarkerNames.size(); ++index) {
        const std::int64_t hits = state.markerHits[index];
        if (hits != 0) {
            lines.push_back("    " + padded(kMarkerNames[index], 18) + " " + std::to_string(hits / state.chunks));
        }
    }
    std::string out;
    for (const std::string& line : lines) {
        if (!out.empty()) {
            out += "\n";
        }
        out += line;
    }
    return out;
}

} // namespace basalt::server::world
