#pragma once

#include <basalt/server/world/noise/Interval.hpp>
#include <basalt/server/world/noise/Noise.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace basalt::server::random {
class RandomSource;
}

namespace basalt::server::world::noise {

/// parity: `NormalNoise`, which since 26.3 is a recipe rather than a sampler: the parameters of a
/// `worldgen/noise` file, turned into a stack of Perlin octaves by `create`.
class NormalNoise {
public:
    /// parity: `NormalNoise.Normalization`.
    enum class Normalization : std::uint8_t { Disabled, Enabled, Legacy };

    /// parity: `NormalNoise.Parameters`, the fields of a `worldgen/noise` file.
    struct Parameters {
        double baseAmplitude = 1.0;
        std::int32_t baseOctave = 0;
        std::int32_t octaveCount = 1;
        Normalization normalize = Normalization::Enabled;
        std::vector<double> amplitudeModifiers;
    };

    explicit NormalNoise(Parameters parameters);

    /// parity: `NormalNoise.createParity`, the pre-26.3 `firstOctave` and amplitude list.
    [[nodiscard]] static NormalNoise createParity(std::int32_t firstOctave, std::span<const double> amplitudes);

    /// parity: `NormalNoise.create`. Two Perlin octaves per level, interleaved in the stack.
    [[nodiscard]] std::shared_ptr<const NoiseStack> create(random::RandomSource& random) const;

    /// parity: `NormalNoise.createForLegacyNetherBiome`.
    [[nodiscard]] std::shared_ptr<const NoiseStack> createForLegacyNetherBiome(random::RandomSource& random) const;

    [[nodiscard]] Interval range() const noexcept { return range_; }

    [[nodiscard]] const Parameters& parameters() const noexcept { return parameters_; }

private:
    struct OctaveInfo {
        std::int32_t octaveIndex;
        double frequency;
        double amplitude;
    };

    Parameters parameters_;
    std::vector<OctaveInfo> octaves_;
    double normalizationFactor_ = 0.0;
    Interval range_ = Interval::notAnInterval();
};

/// parity: `LegacyFbmInitializer.createForLegacyNetherBiome`, the old sequential octave seeding.
[[nodiscard]] std::shared_ptr<const NoiseStack> createLegacyFbm(random::RandomSource& random, std::int32_t firstOctave,
                                                                std::span<const double> amplitudes);

} // namespace basalt::server::world::noise
