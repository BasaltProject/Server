#pragma once

#include <cstdint>
#include <string_view>

// parity: net.minecraft.world.level.levelgen.RandomSupport.
namespace basalt::server::random {

/// A 128-bit seed as a pair of longs, mirroring `RandomSupport.Seed128bit`.
struct Seed128bit {
    std::int64_t lo = 0;
    std::int64_t hi = 0;

    [[nodiscard]] constexpr Seed128bit xorWith(std::int64_t otherLo, std::int64_t otherHi) const noexcept {
        return Seed128bit{lo ^ otherLo, hi ^ otherHi};
    }

    [[nodiscard]] constexpr Seed128bit xorWith(Seed128bit other) const noexcept { return xorWith(other.lo, other.hi); }

    [[nodiscard]] constexpr Seed128bit mixed() const noexcept;

    friend constexpr bool operator==(const Seed128bit&, const Seed128bit&) = default;
};

inline constexpr std::int64_t kGoldenRatio64 = -7046029254386353131LL;
inline constexpr std::int64_t kSilverRatio64 = 7640891576956012809LL;

[[nodiscard]] constexpr std::int64_t mixStafford13(std::int64_t input) noexcept {
    auto z = static_cast<std::uint64_t>(input);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return static_cast<std::int64_t>(z ^ (z >> 31));
}

constexpr Seed128bit Seed128bit::mixed() const noexcept { return Seed128bit{mixStafford13(lo), mixStafford13(hi)}; }

[[nodiscard]] constexpr Seed128bit upgradeSeedTo128bitUnmixed(std::int64_t legacySeed) noexcept {
    const std::int64_t lowBits = legacySeed ^ kSilverRatio64;
    const auto highBits =
        static_cast<std::int64_t>(static_cast<std::uint64_t>(lowBits) + static_cast<std::uint64_t>(kGoldenRatio64));
    return Seed128bit{lowBits, highBits};
}

[[nodiscard]] constexpr Seed128bit upgradeSeedTo128bit(std::int64_t legacySeed) noexcept {
    return upgradeSeedTo128bitUnmixed(legacySeed).mixed();
}

/// Hashes the name through MD5 the way vanilla does, then reads the digest as two big-endian longs.
[[nodiscard]] Seed128bit seedFromHashOf(std::string_view input);

/// Reproduces `java.lang.String.hashCode()`, used by `LegacyPositionalRandomFactory.fromHashOf`.
[[nodiscard]] std::int32_t javaStringHashCode(std::string_view value) noexcept;

} // namespace basalt::server::random
