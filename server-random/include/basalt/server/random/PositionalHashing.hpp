#pragma once

#include <cstdint>

namespace basalt::server::random {

/// parity: net.minecraft.util.Mth.getSeed(int, int, int). Deliberately reproduces the 32-bit
/// overflow of `x * 3129871`: this is not "cleaned up" to use longs throughout.
[[nodiscard]] constexpr std::int64_t positionalSeed(std::int32_t x, std::int32_t y, std::int32_t z) noexcept {
    const auto xTerm = static_cast<std::int32_t>(static_cast<std::uint32_t>(x) * 3129871U);
    auto seed = static_cast<std::uint64_t>(static_cast<std::int64_t>(xTerm));
    seed ^= static_cast<std::uint64_t>(static_cast<std::int64_t>(z) * 116129781LL);
    seed ^= static_cast<std::uint64_t>(static_cast<std::int64_t>(y));
    seed = seed * seed * 42317861ULL + seed * 11ULL;
    return static_cast<std::int64_t>(seed) >> 16;
}

} // namespace basalt::server::random
