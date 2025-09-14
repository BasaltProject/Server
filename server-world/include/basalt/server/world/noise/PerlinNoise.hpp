#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/noise/ImprovedNoise.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace basalt::server::world::noise {

/// parity: `PerlinNoise`, octave-summed ImprovedNoise, with vanilla's `useNewInitialization` split:
/// `create` seeds one noise per octave through forkPositional, `createLegacyForBlendedNoise`
/// consumes them off one shared source, skipping 262 draws for a zero-amplitude octave to keep the
/// sequence aligned. The per-octave factors are precomputed with the same doubling and halving
/// vanilla does, so the arithmetic is bit-identical.
class PerlinNoise {
public:
    [[nodiscard]] static PerlinNoise create(random::RandomSource& source, std::int32_t firstOctave,
                                            std::span<const double> amplitudes);

    /// `octaves` is the sorted, distinct set of octave indices that get amplitude 1.0; every other
    /// octave in the span is 0.0.
    [[nodiscard]] static PerlinNoise create(random::RandomSource& source, std::span<const std::int32_t> octaves);

    /// Same octave shape as create, but seeded off the source directly instead of through
    /// forkPositional.
    [[nodiscard]] static PerlinNoise createLegacyForBlendedNoise(random::RandomSource& source,
                                                                 std::span<const std::int32_t> octaves);

    /// parity: PerlinNoise.createLegacyForLegacyNetherBiome.
    [[nodiscard]] static PerlinNoise createLegacyForLegacyNetherBiome(random::RandomSource& source,
                                                                      std::int32_t firstOctave,
                                                                      std::span<const double> amplitudes);

    [[nodiscard]] double maxValue() const noexcept { return maxValue_; }

    /// parity: PerlinNoise.maxBrokenValue, the bound vanilla actually uses for BlendedNoise's
    /// min and max rather than maxValue.
    [[nodiscard]] double maxBrokenValue(double yScale) const noexcept { return edgeValue(yScale + 2.0); }

    /// The octave `i` levels away from the highest frequency one (index 0), or null if that octave
    /// has zero amplitude.
    [[nodiscard]] const ImprovedNoise* octaveNoise(std::size_t i) const noexcept;

    [[nodiscard]] double getValue(double x, double y, double z) const noexcept {
        double value = 0.0;
        for (const Octave& octave : octaves_) {
            // parity: the multiplication order is vanilla's, amplitude first and the value factor
            // last. Folding amplitude into the factor at construction time would round differently.
            const double sample = octave.noise.noise(wrap(x * octave.inputFactor), wrap(y * octave.inputFactor),
                                                     wrap(z * octave.inputFactor), 0.0, 0.0);
            value += octave.amplitude * sample * octave.valueFactor;
        }
        return value;
    }

    [[nodiscard]] static double wrap(double x) noexcept {
        return x - static_cast<double>(wire::common::mth::lfloor(x / kRoundOff + 0.5)) * kRoundOff;
    }

private:
    struct Octave {
        ImprovedNoise noise;
        double amplitude;
        double inputFactor;
        double valueFactor;
        std::size_t level;
    };

    PerlinNoise(random::RandomSource& source, std::int32_t firstOctave, std::span<const double> amplitudes,
                bool useNewInitialization);

    void buildLegacyOctaves(random::RandomSource& source, std::span<const double> amplitudes,
                            std::int32_t zeroOctaveIndex);

    [[nodiscard]] double edgeValue(double noiseValue) const noexcept;

    static constexpr double kRoundOff = 33554432.0;
    static constexpr int kSkippedOctaveRandomCalls = 262;

    std::vector<Octave> octaves_;
    std::size_t levelCount_ = 0;
    double maxValue_ = 0.0;
};

} // namespace basalt::server::world::noise
