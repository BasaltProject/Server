#pragma once

#include <basalt/server/random/RandomSupport.hpp>

#include <bit>
#include <cstdint>

namespace basalt::server::random {

/// parity: net.minecraft.world.level.levelgen.Xoroshiro128PlusPlus.
class Xoroshiro128PlusPlus {
public:
    constexpr Xoroshiro128PlusPlus() = default;

    constexpr Xoroshiro128PlusPlus(std::int64_t seedLo, std::int64_t seedHi) noexcept {
        if ((seedLo | seedHi) == 0) {
            lo_ = static_cast<std::uint64_t>(kGoldenRatio64);
            hi_ = static_cast<std::uint64_t>(kSilverRatio64);
        } else {
            lo_ = static_cast<std::uint64_t>(seedLo);
            hi_ = static_cast<std::uint64_t>(seedHi);
        }
    }

    constexpr explicit Xoroshiro128PlusPlus(Seed128bit seed) noexcept : Xoroshiro128PlusPlus(seed.lo, seed.hi) {}

    constexpr std::int64_t nextLong() noexcept {
        const std::uint64_t s0 = lo_;
        std::uint64_t s1 = hi_;
        const std::uint64_t result = std::rotl(s0 + s1, 17) + s0;
        s1 ^= s0;
        lo_ = std::rotl(s0, 49) ^ s1 ^ (s1 << 21);
        hi_ = std::rotl(s1, 28);
        return static_cast<std::int64_t>(result);
    }

private:
    std::uint64_t lo_ = static_cast<std::uint64_t>(kGoldenRatio64);
    std::uint64_t hi_ = static_cast<std::uint64_t>(kSilverRatio64);
};

} // namespace basalt::server::random
