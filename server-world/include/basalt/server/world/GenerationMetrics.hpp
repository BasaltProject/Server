#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace basalt::server::world {

/// Opt-in counters and per-phase timings for chunk generation, off by default, read through
/// `[performance] generation-metrics`. Not thread safe: enabling it forces single-threaded
/// generation, since an atomic per noise evaluation would distort the numbers. Call sites guard on
/// `enabled` so a disabled build pays a branch.
namespace generationMetrics {

/// One measured step of the terrain generator.
enum class Phase : std::uint8_t {
    TerrainFill,
    SurfaceRules,
    Carvers,
    Structures,
    Decoration,
    SurfaceFreeze,
    BiomeFill,
    /// Undecorated neighbours generated so a feature could read past the chunk edge. Nested inside
    /// Decoration, and its own work is also counted in the phases above it, so this is how much of
    /// those belongs to a neighbour rather than to the chunk being generated.
    Neighbours,
};

inline constexpr std::size_t kPhaseCount = 8;

[[nodiscard]] bool enabled() noexcept;
void setEnabled(bool value) noexcept;

void countChunk() noexcept;
void countNormalNoise() noexcept;

/// One top-down column scan for a heightmap value, the unit of work a heightmap read does.
void countHeightmapScan(std::int32_t steps) noexcept;

void countBiomeLookup() noexcept;

/// One climate-region distance computation, whether from the index or a full scan.
void countClimateComparison() noexcept;

void addPhaseNanos(Phase phase, std::int64_t nanos) noexcept;

/// Runs `block`, adding its wall time to `phase` when metrics are on.
template <typename Block>
decltype(auto) measure(Phase phase, Block&& block) {
    if (!enabled()) {
        return std::forward<Block>(block)();
    }
    const auto start = std::chrono::steady_clock::now();

    struct Recorder {
        Phase phase;
        std::chrono::steady_clock::time_point start;

        ~Recorder() {
            addPhaseNanos(
                phase,
                std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count());
        }
    } recorder{phase, start};

    return std::forward<Block>(block)();
}

void reset() noexcept;

[[nodiscard]] std::string report();

} // namespace generationMetrics

} // namespace basalt::server::world
