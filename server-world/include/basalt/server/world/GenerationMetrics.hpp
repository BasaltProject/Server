#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace basalt::server::world {

namespace generationMetrics {

enum class Phase : std::uint8_t {
    TerrainFill,
    SurfaceRules,
    Carvers,
    Structures,
    Decoration,
    SurfaceFreeze,
    BiomeFill,
    Neighbours,
};

inline constexpr std::size_t kPhaseCount = 8;

[[nodiscard]] bool enabled() noexcept;
void setEnabled(bool value) noexcept;

void countChunk() noexcept;
void countNormalNoise() noexcept;

void countHeightmapScan(std::int32_t steps) noexcept;

void countBiomeLookup() noexcept;

void countClimateComparison() noexcept;

void addPhaseNanos(Phase phase, std::int64_t nanos) noexcept;

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
