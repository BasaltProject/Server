#pragma once

#include <basalt/server/random/RandomSupport.hpp>
#include <basalt/server/random/Xoroshiro128PlusPlus.hpp>

#include <cstdint>

namespace basalt::server::random {

class PositionalRandomFactory;

/// parity: net.minecraft.util.RandomSource, flattened into one value type.
///
/// Vanilla has an interface with three implementations that worldgen passes around by reference.
/// Here the three are a tag plus their state inline, so a source is copied, forked and stored
/// without ever touching the heap and without a virtual call per `nextInt` in the hottest loop of
/// the server. Every sequence stays bit-exact: the tag picks the same algorithm the interface
/// dispatch used to.
class RandomSource {
public:
    enum class Algorithm : std::uint8_t {
        /// LegacyRandomSource: the 48-bit LCG of java.util.Random.
        Legacy,
        /// XoroshiroRandomSource: xoroshiro128++ with its own bounded-int algorithm.
        Xoroshiro,
    };

    [[nodiscard]] static RandomSource legacy(std::int64_t seed) noexcept;

    [[nodiscard]] static RandomSource xoroshiro(std::int64_t seed) noexcept;

    [[nodiscard]] static RandomSource xoroshiro(std::int64_t seedLo, std::int64_t seedHi) noexcept;

    [[nodiscard]] static RandomSource xoroshiro(Seed128bit seed) noexcept;

    /// parity: WorldgenRandom. Bridges any source to the java.util.Random-style `next(bits)`
    /// algorithms, so worldgen written against them behaves the same over either generator. Over a
    /// legacy source that is a passthrough; over xoroshiro the bits are synthesized from
    /// `nextLong()`, discarding its own bounded-int algorithm.
    [[nodiscard]] static RandomSource worldgen(RandomSource inner) noexcept;

    [[nodiscard]] Algorithm algorithm() const noexcept { return algorithm_; }

    [[nodiscard]] bool isWorldgenView() const noexcept { return bitAlgorithms_; }

    /// Where a xoroshiro source stands right now. parity: `Xoroshiro128PlusPlus.CODEC`, two longs
    /// and nothing else: the half-drawn gaussian is not part of what vanilla writes down either.
    [[nodiscard]] Seed128bit xoroshiroState() const noexcept { return generator_.state(); }

    [[nodiscard]] RandomSource fork() noexcept;

    [[nodiscard]] PositionalRandomFactory forkPositional() noexcept;

    void setSeed(std::int64_t seed) noexcept;

    /// The `next(bits)` primitive. Only meaningful for legacy and worldgen sources; a bare
    /// xoroshiro source has no such primitive in vanilla, and neither has this one.
    [[nodiscard]] std::int32_t next(int bits) noexcept;

    [[nodiscard]] std::int32_t nextInt() noexcept;

    [[nodiscard]] std::int32_t nextInt(std::int32_t bound) noexcept;

    [[nodiscard]] std::int32_t nextInt(std::int32_t origin, std::int32_t bound) noexcept {
        return origin + nextInt(bound - origin);
    }

    [[nodiscard]] std::int32_t nextIntBetweenInclusive(std::int32_t min, std::int32_t maxInclusive) noexcept {
        return nextInt(maxInclusive - min + 1) + min;
    }

    [[nodiscard]] std::int64_t nextLong() noexcept;

    [[nodiscard]] bool nextBoolean() noexcept;

    [[nodiscard]] float nextFloat() noexcept;

    [[nodiscard]] double nextDouble() noexcept;

    [[nodiscard]] double nextGaussian() noexcept;

    [[nodiscard]] double triangle(double mean, double spread) noexcept {
        return mean + spread * (nextDouble() - nextDouble());
    }

    [[nodiscard]] float triangle(float mean, float spread) noexcept {
        return mean + spread * (nextFloat() - nextFloat());
    }

    void consumeCount(int rounds) noexcept;

private:
    RandomSource() = default;

    [[nodiscard]] std::int64_t rawNextLong() noexcept;

    void resetGaussian() noexcept {
        haveNextGaussian_ = false;
        nextGaussian_ = 0.0;
    }

    Algorithm algorithm_ = Algorithm::Legacy;
    /// True for a worldgen view: every derived value comes from `next(bits)`.
    bool bitAlgorithms_ = false;
    bool haveNextGaussian_ = false;
    std::uint64_t legacySeed_ = 0;
    Xoroshiro128PlusPlus generator_;
    double nextGaussian_ = 0.0;
};

} // namespace basalt::server::random
