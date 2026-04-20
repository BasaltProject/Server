#pragma once

#include <basalt/server/random/RandomSupport.hpp>
#include <basalt/server/random/Xoroshiro128PlusPlus.hpp>

#include <cstdint>

namespace basalt::server::random {
class PositionalRandomFactory;

// parity: net.minecraft.util.RandomSource, flattened into one value type.
class RandomSource {
public:
    enum class Algorithm : std::uint8_t {
        Legacy,
        Xoroshiro,
    };

    [[nodiscard]] static RandomSource legacy(std::int64_t seed) noexcept;

    [[nodiscard]] static RandomSource xoroshiro(std::int64_t seed) noexcept;

    [[nodiscard]] static RandomSource xoroshiro(std::int64_t seedLo, std::int64_t seedHi) noexcept;

    [[nodiscard]] static RandomSource xoroshiro(Seed128bit seed) noexcept;

    // parity: WorldgenRandom.
    [[nodiscard]] static RandomSource worldgen(RandomSource inner) noexcept;

    [[nodiscard]] Algorithm algorithm() const noexcept { return algorithm_; }

    [[nodiscard]] bool isWorldgenView() const noexcept { return bitAlgorithms_; }

    // parity: `Xoroshiro128PlusPlus.CODEC`, two longs.
    [[nodiscard]] Seed128bit xoroshiroState() const noexcept { return generator_.state(); }

    [[nodiscard]] RandomSource fork() noexcept;

    [[nodiscard]] PositionalRandomFactory forkPositional() noexcept;

    void setSeed(std::int64_t seed) noexcept;

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

    // parity: vanilla decorates a chunk with a single `WorldgenRandom`.
    class SharedGaussian {
    public:
        SharedGaussian() noexcept;
        ~SharedGaussian() noexcept;
        SharedGaussian(const SharedGaussian&) = delete;
        SharedGaussian& operator=(const SharedGaussian&) = delete;
        SharedGaussian(SharedGaussian&&) = delete;
        SharedGaussian& operator=(SharedGaussian&&) = delete;
    };

private:
    RandomSource() = default;

    [[nodiscard]] std::int64_t rawNextLong() noexcept;

    void resetGaussian() noexcept {
        haveNextGaussian_ = false;
        nextGaussian_ = 0.0;
    }

    Algorithm algorithm_ = Algorithm::Legacy;
    bool bitAlgorithms_ = false;
    bool haveNextGaussian_ = false;
    std::uint64_t legacySeed_ = 0;
    Xoroshiro128PlusPlus generator_;
    double nextGaussian_ = 0.0;
};
} // namespace basalt::server::random
