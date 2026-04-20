#include <basalt/server/random/PositionalRandomFactory.hpp>
#include <basalt/server/random/RandomSource.hpp>

#include <cmath>
#include <cstdint>

namespace basalt::server::random {
namespace {
constexpr std::uint64_t kModulusMask = 281474976710655ULL;
constexpr std::uint64_t kMultiplier = 25214903917ULL;
constexpr std::uint64_t kIncrement = 11ULL;

constexpr float kFloatMultiplier = 5.9604645E-8F;
// parity: vanilla widens a float literal here.
constexpr double kDoubleMultiplier = static_cast<double>(1.110223E-16F);
} // namespace

RandomSource RandomSource::legacy(std::int64_t seed) noexcept {
    RandomSource source;
    source.algorithm_ = Algorithm::Legacy;
    source.setSeed(seed);
    return source;
}

RandomSource RandomSource::xoroshiro(std::int64_t seed) noexcept { return xoroshiro(upgradeSeedTo128bit(seed)); }

RandomSource RandomSource::xoroshiro(std::int64_t seedLo, std::int64_t seedHi) noexcept {
    RandomSource source;
    source.algorithm_ = Algorithm::Xoroshiro;
    source.generator_ = Xoroshiro128PlusPlus{seedLo, seedHi};
    return source;
}

RandomSource RandomSource::xoroshiro(Seed128bit seed) noexcept { return xoroshiro(seed.lo, seed.hi); }

namespace {
struct SharedGaussianState {
    bool active = false;
    bool have = false;
    double value = 0.0;
};

thread_local SharedGaussianState tSharedGaussian;
} // namespace

RandomSource::SharedGaussian::SharedGaussian() noexcept {
    tSharedGaussian = {.active = true, .have = false, .value = 0.0};
}

RandomSource::SharedGaussian::~SharedGaussian() noexcept { tSharedGaussian = {}; }

RandomSource RandomSource::worldgen(RandomSource inner) noexcept {
    inner.bitAlgorithms_ = true;
    if (!tSharedGaussian.active) {
        inner.resetGaussian();
    }
    return inner;
}

void RandomSource::setSeed(std::int64_t seed) noexcept {
    if (algorithm_ == Algorithm::Legacy) {
        legacySeed_ = (static_cast<std::uint64_t>(seed) ^ kMultiplier) & kModulusMask;
    } else {
        generator_ = Xoroshiro128PlusPlus{upgradeSeedTo128bit(seed)};
    }
    // parity: `WorldgenRandom.setSeed` never clears the spare.
    if (!(bitAlgorithms_ && tSharedGaussian.active)) {
        resetGaussian();
    }
}

std::int64_t RandomSource::rawNextLong() noexcept { return generator_.nextLong(); }

std::int32_t RandomSource::next(int bits) noexcept {
    if (algorithm_ == Algorithm::Legacy) {
        legacySeed_ = (legacySeed_ * kMultiplier + kIncrement) & kModulusMask;
        return static_cast<std::int32_t>(legacySeed_ >> (48 - bits));
    }
    return static_cast<std::int32_t>(static_cast<std::uint64_t>(rawNextLong()) >> (64 - bits));
}

std::int32_t RandomSource::nextInt() noexcept {
    if (bitAlgorithms_ || algorithm_ == Algorithm::Legacy) {
        return next(32);
    }
    return static_cast<std::int32_t>(static_cast<std::uint64_t>(rawNextLong()));
}

std::int32_t RandomSource::nextInt(std::int32_t bound) noexcept {
    if (bitAlgorithms_ || algorithm_ == Algorithm::Legacy) {
        // parity: BitRandomSource.nextInt, java.util.Random's power-of-two shortcut plus rejection.
        if ((bound & (bound - 1)) == 0) {
            return static_cast<std::int32_t>((static_cast<std::int64_t>(bound) * next(31)) >> 31);
        }
        std::int32_t sample = 0;
        std::int32_t modulo = 0;
        do {
            sample = next(31);
            modulo = sample % bound;
        } while (static_cast<std::int32_t>(static_cast<std::uint32_t>(sample) - static_cast<std::uint32_t>(modulo) +
                                           static_cast<std::uint32_t>(bound - 1)) < 0);
        return modulo;
    }

    // parity: XoroshiroRandomSource.nextInt, Lemire's method rather than rejection sampling.
    auto randomBits = static_cast<std::uint64_t>(static_cast<std::uint32_t>(nextInt()));
    std::uint64_t multiplied = randomBits * static_cast<std::uint64_t>(bound);
    std::uint64_t fractional = multiplied & 0xFFFFFFFFULL;
    if (fractional < static_cast<std::uint64_t>(bound)) {
        const std::uint64_t unbiasedStart =
            static_cast<std::uint64_t>(static_cast<std::uint32_t>(-bound)) % static_cast<std::uint64_t>(bound);
        while (fractional < unbiasedStart) {
            randomBits = static_cast<std::uint64_t>(static_cast<std::uint32_t>(nextInt()));
            multiplied = randomBits * static_cast<std::uint64_t>(bound);
            fractional = multiplied & 0xFFFFFFFFULL;
        }
    }
    return static_cast<std::int32_t>(multiplied >> 32);
}

std::int64_t RandomSource::nextLong() noexcept {
    if (bitAlgorithms_ || algorithm_ == Algorithm::Legacy) {
        const std::int32_t upper = next(32);
        const std::int32_t lower = next(32);
        // parity: vanilla adds a sign-extended int to the shifted upper half.
        return static_cast<std::int64_t>((static_cast<std::uint64_t>(static_cast<std::uint32_t>(upper)) << 32) +
                                         static_cast<std::uint64_t>(static_cast<std::int64_t>(lower)));
    }
    return rawNextLong();
}

bool RandomSource::nextBoolean() noexcept {
    if (bitAlgorithms_ || algorithm_ == Algorithm::Legacy) {
        return next(1) != 0;
    }
    return (rawNextLong() & 1) != 0;
}

float RandomSource::nextFloat() noexcept {
    if (bitAlgorithms_ || algorithm_ == Algorithm::Legacy) {
        return static_cast<float>(next(24)) * kFloatMultiplier;
    }
    return static_cast<float>(static_cast<std::uint64_t>(rawNextLong()) >> 40) * kFloatMultiplier;
}

double RandomSource::nextDouble() noexcept {
    if (bitAlgorithms_ || algorithm_ == Algorithm::Legacy) {
        const auto upper = static_cast<std::int64_t>(next(26));
        const auto lower = static_cast<std::int64_t>(next(27));
        return static_cast<double>((upper << 27) + lower) * kDoubleMultiplier;
    }
    return static_cast<double>(static_cast<std::int64_t>(static_cast<std::uint64_t>(rawNextLong()) >> 11)) *
           kDoubleMultiplier;
}

double RandomSource::nextGaussian() noexcept {
    // parity: MarsagliaPolarGaussian, the polar method with its cached spare sample.
    const bool shared = bitAlgorithms_ && tSharedGaussian.active;
    if (shared) {
        if (tSharedGaussian.have) {
            tSharedGaussian.have = false;
            return tSharedGaussian.value;
        }
    } else if (haveNextGaussian_) {
        haveNextGaussian_ = false;
        return nextGaussian_;
    }

    double x = 0.0;
    double y = 0.0;
    double radiusSquared = 0.0;
    do {
        x = 2.0 * nextDouble() - 1.0;
        y = 2.0 * nextDouble() - 1.0;
        radiusSquared = x * x + y * y;
    } while (radiusSquared >= 1.0 || radiusSquared == 0.0);

    const double multiplier = std::sqrt(-2.0 * std::log(radiusSquared) / radiusSquared);
    if (shared) {
        tSharedGaussian.value = y * multiplier;
        tSharedGaussian.have = true;
    } else {
        nextGaussian_ = y * multiplier;
        haveNextGaussian_ = true;
    }
    return x * multiplier;
}

void RandomSource::consumeCount(int rounds) noexcept {
    for (int i = 0; i < rounds; ++i) {
        static_cast<void>(nextInt());
    }
}

RandomSource RandomSource::fork() noexcept {
    if (algorithm_ == Algorithm::Legacy) {
        return legacy(nextLong());
    }
    const std::int64_t lo = rawNextLong();
    const std::int64_t hi = rawNextLong();
    return xoroshiro(lo, hi);
}

PositionalRandomFactory RandomSource::forkPositional() noexcept {
    if (algorithm_ == Algorithm::Legacy) {
        return PositionalRandomFactory::legacy(nextLong());
    }
    const std::int64_t lo = rawNextLong();
    const std::int64_t hi = rawNextLong();
    return PositionalRandomFactory::xoroshiro(lo, hi);
}
} // namespace basalt::server::random
