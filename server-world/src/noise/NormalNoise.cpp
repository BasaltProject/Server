#include <basalt/server/random/PositionalRandomFactory.hpp>
#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/noise/NormalNoise.hpp>

#include <climits>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace basalt::server::world::noise {
namespace {

constexpr double kInputFactor = 1.0181268882175227;
constexpr double kTargetDeviation = 0.3333333333333333;
constexpr double kPerlinStandardDeviation = 0.2702247831245211;

struct Octave {
    std::int32_t octaveIndex;
    double frequency;
    double amplitude;
};

[[nodiscard]] double modifierAt(const std::vector<double>& modifiers, std::int32_t index) {
    return modifiers.empty() ? 1.0 : modifiers[static_cast<std::size_t>(index)];
}

[[nodiscard]] std::vector<Octave> buildOctaves(std::int32_t baseOctave, double baseAmplitude, std::int32_t octaveCount,
                                               bool normalize, const std::vector<double>& modifiers) {
    double frequency = std::pow(2.0, static_cast<double>(baseOctave));
    double amplitude = baseAmplitude;
    if (normalize) {
        amplitude = baseAmplitude * (std::pow(0.5, static_cast<double>(-(octaveCount - 1))) /
                                     (std::pow(0.5, static_cast<double>(-octaveCount)) - 1.0));
    }
    std::vector<Octave> octaves;
    for (std::int32_t i = 0; i < octaveCount; ++i) {
        const double modifier = modifierAt(modifiers, i);
        if (modifier != 0.0) {
            octaves.push_back(Octave{baseOctave + i, frequency, amplitude * modifier});
        }
        frequency *= 2.0;
        amplitude *= 0.5;
    }
    return octaves;
}

[[nodiscard]] double absSum(const std::vector<Octave>& octaves) {
    double sum = 0.0;
    for (const Octave& octave : octaves) {
        sum += std::fabs(octave.amplitude);
    }
    return sum;
}

[[nodiscard]] double normalizationFactorFor(double targetAmplitude, const std::vector<Octave>& octaves) {
    double variance = 0.0;
    for (const Octave& octave : octaves) {
        const double layerDeviation = kPerlinStandardDeviation * std::fabs(octave.amplitude);
        variance += layerDeviation * layerDeviation;
    }
    const double inputDeviation = std::sqrt(variance);
    if (inputDeviation == 0.0) {
        return 0.0;
    }
    return targetAmplitude * kTargetDeviation / (inputDeviation * std::sqrt(2.0));
}

[[nodiscard]] double parityNormalizationFactor(double baseAmplitude, std::int32_t octaveCount,
                                               const std::vector<double>& modifiers) {
    std::int32_t minOctave = INT_MAX;
    std::int32_t maxOctave = INT_MIN;
    for (std::int32_t i = 0; i < octaveCount; ++i) {
        if (modifierAt(modifiers, i) != 0.0) {
            minOctave = std::min(minOctave, i);
            maxOctave = std::max(maxOctave, i);
        }
    }
    const double expected = 0.1 * (1.0 + 1.0 / static_cast<double>(maxOctave - minOctave + 1));
    return baseAmplitude * 0.5 * kTargetDeviation / expected;
}

} // namespace

NormalNoise::NormalNoise(Parameters parameters) : parameters_(std::move(parameters)) {
    const std::vector<Octave> octaves =
        buildOctaves(parameters_.baseOctave, parameters_.baseAmplitude, parameters_.octaveCount,
                     parameters_.normalize != Normalization::Disabled, parameters_.amplitudeModifiers);
    double targetAmplitude = absSum(octaves);
    double factor = normalizationFactorFor(targetAmplitude, octaves);
    if (parameters_.normalize == Normalization::Legacy && factor != 0.0) {
        const double parity = parityNormalizationFactor(parameters_.baseAmplitude, parameters_.octaveCount,
                                                        parameters_.amplitudeModifiers);
        targetAmplitude *= parity / factor;
        factor = parity;
    }
    normalizationFactor_ = factor;
    range_ = Interval::ofSymmetric(static_cast<float>(targetAmplitude * kTargetDeviation * 6.0));
    for (const Octave& octave : octaves) {
        octaves_.push_back(OctaveInfo{octave.octaveIndex, octave.frequency, octave.amplitude});
    }
}

NormalNoise NormalNoise::createParity(std::int32_t firstOctave, std::span<const double> amplitudes) {
    if (amplitudes.empty()) {
        throw std::invalid_argument{"Need at least 1 amplitude"};
    }
    const std::vector<double> list(amplitudes.begin(), amplitudes.end());
    const auto count = static_cast<std::int32_t>(list.size());
    const std::vector<Octave> octaves = buildOctaves(firstOctave, 1.0, count, true, list);
    const double newFactor = normalizationFactorFor(absSum(octaves), octaves);
    const double baseAmplitude = newFactor == 0.0 ? 1.0 : parityNormalizationFactor(1.0, count, list) / newFactor;
    Parameters parameters{.baseAmplitude = baseAmplitude,
                          .baseOctave = firstOctave,
                          .octaveCount = count,
                          .normalize = Normalization::Enabled,
                          .amplitudeModifiers = {}};
    for (std::int32_t i = 0; i < count; ++i) {
        if (list[static_cast<std::size_t>(i)] != 1.0) {
            if (parameters.amplitudeModifiers.empty()) {
                parameters.amplitudeModifiers.assign(list.size(), 1.0);
            }
            parameters.amplitudeModifiers[static_cast<std::size_t>(i)] = list[static_cast<std::size_t>(i)];
        }
    }
    return NormalNoise{std::move(parameters)};
}

std::shared_ptr<const NoiseStack> NormalNoise::create(random::RandomSource& random) const {
    const random::PositionalRandomFactory first = random.forkPositional();
    const random::PositionalRandomFactory second = random.forkPositional();
    std::vector<NoiseStack::Layer> layers;
    for (const OctaveInfo& octave : octaves_) {
        const std::string seed = "octave_" + std::to_string(octave.octaveIndex);
        random::RandomSource firstRandom = first.fromHashOf(seed);
        random::RandomSource secondRandom = second.fromHashOf(seed);
        const auto valueFactor = static_cast<float>(normalizationFactor_ * octave.amplitude);
        layers.push_back({std::make_shared<const PerlinNoise>(firstRandom), octave.frequency, valueFactor});
        layers.push_back(
            {std::make_shared<const PerlinNoise>(secondRandom), octave.frequency * kInputFactor, valueFactor});
    }
    return std::make_shared<const NoiseStack>(std::move(layers));
}

std::shared_ptr<const NoiseStack> NormalNoise::createForLegacyNetherBiome(random::RandomSource& random) const {
    std::vector<double> amplitudes = parameters_.amplitudeModifiers;
    if (amplitudes.empty()) {
        amplitudes.assign(static_cast<std::size_t>(parameters_.octaveCount), 1.0);
    }
    const std::shared_ptr<const NoiseStack> first = createLegacyFbm(random, parameters_.baseOctave, amplitudes);
    const std::shared_ptr<const NoiseStack> second = createLegacyFbm(random, parameters_.baseOctave, amplitudes);
    const auto valueFactor = static_cast<float>(normalizationFactor_ * parameters_.baseAmplitude);
    std::vector<NoiseStack::Layer> layers;
    for (const NoiseStack::Layer& layer : first->layers()) {
        layers.push_back({layer.noise, layer.frequency * 1.0, layer.amplitude * valueFactor});
    }
    for (const NoiseStack::Layer& layer : second->layers()) {
        layers.push_back({layer.noise, layer.frequency * kInputFactor, layer.amplitude * valueFactor});
    }
    return std::make_shared<const NoiseStack>(std::move(layers));
}

std::shared_ptr<const NoiseStack> createLegacyFbm(random::RandomSource& random, std::int32_t firstOctave,
                                                  std::span<const double> amplitudes) {
    const auto octaves = static_cast<std::int32_t>(amplitudes.size());
    const std::int32_t zeroOctaveIndex = -firstOctave;
    std::vector<std::shared_ptr<const PerlinNoise>> levels(amplitudes.size());
    auto zeroOctave = std::make_shared<const PerlinNoise>(random);
    if (zeroOctaveIndex >= 0 && zeroOctaveIndex < octaves &&
        amplitudes[static_cast<std::size_t>(zeroOctaveIndex)] != 0.0) {
        levels[static_cast<std::size_t>(zeroOctaveIndex)] = zeroOctave;
    }
    for (std::int32_t i = zeroOctaveIndex - 1; i >= 0; --i) {
        if (i < octaves && amplitudes[static_cast<std::size_t>(i)] != 0.0) {
            levels[static_cast<std::size_t>(i)] = std::make_shared<const PerlinNoise>(random);
        } else {
            random.consumeCount(262);
        }
    }
    if (zeroOctaveIndex < octaves - 1) {
        throw std::invalid_argument{"Positive octaves are temporarily disabled"};
    }
    double factor = std::pow(2.0, static_cast<double>(-zeroOctaveIndex));
    double valueFactor =
        std::pow(2.0, static_cast<double>(octaves - 1)) / (std::pow(2.0, static_cast<double>(octaves)) - 1.0);
    std::vector<NoiseStack::Layer> layers;
    for (std::size_t i = 0; i < levels.size(); ++i) {
        if (levels[i] != nullptr) {
            layers.push_back({levels[i], factor, static_cast<float>(valueFactor * amplitudes[i])});
        }
        factor *= 2.0;
        valueFactor /= 2.0;
    }
    return std::make_shared<const NoiseStack>(std::move(layers));
}

} // namespace basalt::server::world::noise
