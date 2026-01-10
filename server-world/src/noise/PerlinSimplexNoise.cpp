#include <basalt/server/world/noise/PerlinSimplexNoise.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace basalt::server::world::noise {
namespace {

constexpr int kSkippedOctaveRandomCalls = 262;

} // namespace

PerlinSimplexNoise::PerlinSimplexNoise(random::RandomSource& source, std::span<const std::int32_t> octaves) {
    std::vector<std::int32_t> sorted{octaves.begin(), octaves.end()};
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
    const std::int32_t lowFreqOctaves = -sorted.front();
    const std::int32_t highFreqOctaves = sorted.back();
    const std::int32_t totalOctaves = lowFreqOctaves + highFreqOctaves + 1;

    const auto wanted = [&sorted](std::int32_t octave) {
        return std::binary_search(sorted.begin(), sorted.end(), octave);
    };

    SimplexNoise zeroOctave{source};
    const std::int32_t zeroOctaveIndex = highFreqOctaves;
    noiseLevels_.resize(static_cast<std::size_t>(totalOctaves));
    if (zeroOctaveIndex >= 0 && zeroOctaveIndex < totalOctaves && wanted(0)) {
        noiseLevels_[static_cast<std::size_t>(zeroOctaveIndex)] = zeroOctave;
    }
    for (std::int32_t i = zeroOctaveIndex + 1; i < totalOctaves; ++i) {
        if (wanted(zeroOctaveIndex - i)) {
            noiseLevels_[static_cast<std::size_t>(i)] = SimplexNoise{source};
        } else {
            source.consumeCount(kSkippedOctaveRandomCalls);
        }
    }

    if (highFreqOctaves > 0) {
        const auto positiveOctaveSeed = static_cast<std::int64_t>(
            zeroOctave.getValue(zeroOctave.xo(), zeroOctave.yo(), zeroOctave.zo()) * 9.223372E18);
        random::RandomSource highFreqRandom =
            random::RandomSource::worldgen(random::RandomSource::legacy(positiveOctaveSeed));
        for (std::int32_t i = zeroOctaveIndex - 1; i >= 0; --i) {
            if (i < totalOctaves && wanted(zeroOctaveIndex - i)) {
                noiseLevels_[static_cast<std::size_t>(i)] = SimplexNoise{highFreqRandom};
            } else {
                highFreqRandom.consumeCount(kSkippedOctaveRandomCalls);
            }
        }
    }

    highestFreqInputFactor_ = std::pow(2.0, static_cast<double>(highFreqOctaves));
    highestFreqValueFactor_ = 1.0 / (std::pow(2.0, static_cast<double>(totalOctaves)) - 1.0);
}

double PerlinSimplexNoise::getValue(double x, double y, bool useNoiseStart) const noexcept {
    double value = 0.0;
    double factor = highestFreqInputFactor_;
    double valueFactor = highestFreqValueFactor_;
    for (const std::optional<SimplexNoise>& level : noiseLevels_) {
        if (level.has_value()) {
            const double ox = useNoiseStart ? level->xo() : 0.0;
            const double oy = useNoiseStart ? level->yo() : 0.0;
            value += level->getValue(x * factor + ox, y * factor + oy) * valueFactor;
        }
        factor /= 2.0;
        valueFactor *= 2.0;
    }
    return value;
}

} // namespace basalt::server::world::noise
