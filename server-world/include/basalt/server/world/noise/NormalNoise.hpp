#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/noise/PerlinNoise.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace basalt::server::world::noise {

/// parity: net.minecraft.world.level.levelgen.synth.NormalNoise. Two independent PerlinNoise
/// instances, the second sampled at a slightly different frequency, summed and rescaled to an
/// approximately fixed output deviation. This is what vanilla actually samples for biome climate
/// and density functions, as opposed to raw Perlin noise.
class NormalNoise {
public:
    struct Parameters {
        std::int32_t firstOctave;
        std::vector<double> amplitudes;
    };

    [[nodiscard]] static NormalNoise create(random::RandomSource& source, std::int32_t firstOctave,
                                            std::span<const double> amplitudes);

    /// parity: NormalNoise.createLegacyNetherBiome.
    [[nodiscard]] static NormalNoise createLegacyNetherBiome(random::RandomSource& source, std::int32_t firstOctave,
                                                             std::span<const double> amplitudes);

    [[nodiscard]] double maxValue() const noexcept { return maxValue_; }

    [[nodiscard]] double getValue(double x, double y, double z) const noexcept {
        return (first_.getValue(x, y, z) + second_.getValue(x * kInputFactor, y * kInputFactor, z * kInputFactor)) *
               valueFactor_;
    }

private:
    NormalNoise(PerlinNoise first, PerlinNoise second, std::span<const double> amplitudes);

    static constexpr double kInputFactor = 1.0181268882175227;
    // parity: vanilla's own TARGET_DEVIATION field (1/3) is declared but never used here; the
    // constructor divides by a literal 1/6 instead. Kept as-is, bug and all.
    static constexpr double kValueFactorNumerator = 0.16666666666666666;

    PerlinNoise first_;
    PerlinNoise second_;
    double valueFactor_ = 0.0;
    double maxValue_ = 0.0;
};

} // namespace basalt::server::world::noise
