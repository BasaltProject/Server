#include <basalt/server/world/noise/NormalNoise.hpp>

#include <cstdint>
#include <limits>

namespace basalt::server::world::noise {
namespace {

double expectedDeviation(std::int32_t octaveSpan) {
    return 0.1 * (1.0 + 1.0 / (static_cast<double>(octaveSpan) + 1.0));
}

} // namespace

NormalNoise::NormalNoise(PerlinNoise first, PerlinNoise second, std::span<const double> amplitudes)
    : first_(std::move(first)), second_(std::move(second)) {
    std::int32_t minOctave = std::numeric_limits<std::int32_t>::max();
    std::int32_t maxOctave = std::numeric_limits<std::int32_t>::min();
    for (std::size_t i = 0; i < amplitudes.size(); ++i) {
        if (amplitudes[i] != 0.0) {
            const auto index = static_cast<std::int32_t>(i);
            minOctave = std::min(minOctave, index);
            maxOctave = std::max(maxOctave, index);
        }
    }
    valueFactor_ = kValueFactorNumerator / expectedDeviation(maxOctave - minOctave);
    maxValue_ = (first_.maxValue() + second_.maxValue()) * valueFactor_;
}

NormalNoise NormalNoise::create(random::RandomSource& source, std::int32_t firstOctave,
                                std::span<const double> amplitudes) {
    PerlinNoise first = PerlinNoise::create(source, firstOctave, amplitudes);
    PerlinNoise second = PerlinNoise::create(source, firstOctave, amplitudes);
    return NormalNoise{std::move(first), std::move(second), amplitudes};
}

NormalNoise NormalNoise::createLegacyNetherBiome(random::RandomSource& source, std::int32_t firstOctave,
                                                 std::span<const double> amplitudes) {
    PerlinNoise first = PerlinNoise::createLegacyForLegacyNetherBiome(source, firstOctave, amplitudes);
    PerlinNoise second = PerlinNoise::createLegacyForLegacyNetherBiome(source, firstOctave, amplitudes);
    return NormalNoise{std::move(first), std::move(second), amplitudes};
}

} // namespace basalt::server::world::noise
