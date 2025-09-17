#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/noise/SimplexNoise.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace basalt::server::world::noise {

/// parity: `net.minecraft.world.level.levelgen.synth.PerlinSimplexNoise`, octave-summed
/// `SimplexNoise`.
///
/// Unlike `PerlinNoise` (built from `ImprovedNoise`), octave index 0 here is the *highest* frequency
/// one, and octaves below zero are reseeded from a worldgen-wrapped legacy source derived from the
/// zero octave's own value at its own origin, not drawn from `source` directly.
class PerlinSimplexNoise {
public:
    PerlinSimplexNoise(random::RandomSource& source, std::span<const std::int32_t> octaves);

    [[nodiscard]] double getValue(double x, double y, bool useNoiseStart = false) const noexcept;

private:
    std::vector<std::optional<SimplexNoise>> noiseLevels_;
    double highestFreqValueFactor_ = 0.0;
    double highestFreqInputFactor_ = 0.0;
};

} // namespace basalt::server::world::noise
