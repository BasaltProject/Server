#include <basalt/server/random/PositionalRandomFactory.hpp>
#include <basalt/server/world/noise/PerlinNoise.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <string>

namespace basalt::server::world::noise {
namespace {

struct OctaveShape {
    std::int32_t firstOctave;
    std::vector<double> amplitudes;
};

OctaveShape makeAmplitudes(std::span<const std::int32_t> octaves) {
    assert(!octaves.empty() && "Need some octaves!");
    const std::int32_t lowFreqOctaves = -*std::ranges::min_element(octaves);
    const std::int32_t highFreqOctaves = *std::ranges::max_element(octaves);
    const std::int32_t span = lowFreqOctaves + highFreqOctaves + 1;
    assert(span >= 1 && "Total number of octaves needs to be >= 1");

    std::vector<double> amplitudes(static_cast<std::size_t>(span), 0.0);
    for (const std::int32_t octave : octaves) {
        amplitudes[static_cast<std::size_t>(octave + lowFreqOctaves)] = 1.0;
    }
    return OctaveShape{-lowFreqOctaves, std::move(amplitudes)};
}

} // namespace

PerlinNoise::PerlinNoise(random::RandomSource& source, std::int32_t firstOctave, std::span<const double> amplitudes,
                         bool useNewInitialization) {
    levelCount_ = amplitudes.size();
    const std::int32_t zeroOctaveIndex = -firstOctave;

    if (useNewInitialization) {
        random::PositionalRandomFactory positional = source.forkPositional();
        for (std::size_t i = 0; i < amplitudes.size(); ++i) {
            if (amplitudes[i] != 0.0) {
                random::RandomSource octaveSource =
                    positional.fromHashOf("octave_" + std::to_string(firstOctave + static_cast<std::int32_t>(i)));
                octaves_.push_back(Octave{ImprovedNoise{octaveSource}, amplitudes[i], 0.0, 0.0, i});
            }
        }
    } else {
        buildLegacyOctaves(source, amplitudes, zeroOctaveIndex);
    }

    // parity: the factors come from walking every octave, zero-amplitude ones included, with the
    // same doubling and halving vanilla does. Only the values at live octaves are kept.
    double inputFactor = std::pow(2.0, -zeroOctaveIndex);
    double valueFactor = std::pow(2.0, static_cast<double>(amplitudes.size()) - 1.0) /
                         (std::pow(2.0, static_cast<double>(amplitudes.size())) - 1.0);
    std::size_t next = 0;
    for (std::size_t i = 0; i < amplitudes.size(); ++i) {
        if (next < octaves_.size() && octaves_[next].level == i) {
            octaves_[next].inputFactor = inputFactor;
            octaves_[next].valueFactor = valueFactor;
            ++next;
        }
        inputFactor *= 2.0;
        valueFactor /= 2.0;
    }

    maxValue_ = edgeValue(2.0);
}

void PerlinNoise::buildLegacyOctaves(random::RandomSource& source, std::span<const double> amplitudes,
                                     std::int32_t zeroOctaveIndex) {
    const auto octaveCount = static_cast<std::int32_t>(amplitudes.size());
    ImprovedNoise zeroOctave{source};
    if (zeroOctaveIndex >= 0 && zeroOctaveIndex < octaveCount &&
        amplitudes[static_cast<std::size_t>(zeroOctaveIndex)] != 0.0) {
        octaves_.push_back(Octave{std::move(zeroOctave), amplitudes[static_cast<std::size_t>(zeroOctaveIndex)], 0.0,
                                  0.0, static_cast<std::size_t>(zeroOctaveIndex)});
    }
    for (std::int32_t i = zeroOctaveIndex - 1; i >= 0; --i) {
        if (i < octaveCount && amplitudes[static_cast<std::size_t>(i)] != 0.0) {
            octaves_.push_back(Octave{ImprovedNoise{source}, amplitudes[static_cast<std::size_t>(i)], 0.0, 0.0,
                                      static_cast<std::size_t>(i)});
        } else {
            source.consumeCount(kSkippedOctaveRandomCalls);
        }
    }
    assert(zeroOctaveIndex >= octaveCount - 1 && "Positive octaves are temporarily disabled");
    // The legacy path fills octaves from the zero octave downwards, so they land out of order.
    std::ranges::sort(octaves_, {}, &Octave::level);
}

const ImprovedNoise* PerlinNoise::octaveNoise(std::size_t i) const noexcept {
    const std::size_t level = levelCount_ - 1 - i;
    for (const Octave& octave : octaves_) {
        if (octave.level == level) {
            return &octave.noise;
        }
    }
    return nullptr;
}

double PerlinNoise::edgeValue(double noiseValue) const noexcept {
    double value = 0.0;
    for (const Octave& octave : octaves_) {
        value += octave.amplitude * noiseValue * octave.valueFactor;
    }
    return value;
}

PerlinNoise PerlinNoise::create(random::RandomSource& source, std::int32_t firstOctave,
                                std::span<const double> amplitudes) {
    return PerlinNoise{source, firstOctave, amplitudes, true};
}

PerlinNoise PerlinNoise::create(random::RandomSource& source, std::span<const std::int32_t> octaves) {
    const OctaveShape shape = makeAmplitudes(octaves);
    return PerlinNoise{source, shape.firstOctave, shape.amplitudes, true};
}

PerlinNoise PerlinNoise::createLegacyForBlendedNoise(random::RandomSource& source,
                                                     std::span<const std::int32_t> octaves) {
    const OctaveShape shape = makeAmplitudes(octaves);
    return PerlinNoise{source, shape.firstOctave, shape.amplitudes, false};
}

PerlinNoise PerlinNoise::createLegacyForLegacyNetherBiome(random::RandomSource& source, std::int32_t firstOctave,
                                                          std::span<const double> amplitudes) {
    return PerlinNoise{source, firstOctave, amplitudes, false};
}

} // namespace basalt::server::world::noise
