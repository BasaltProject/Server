#pragma once

#include <basalt/server/random/RandomSource.hpp>

#include <cstdint>
#include <string_view>

namespace basalt::server::random {

/// parity: PositionalRandomFactory. Derives deterministic per-position and per-name sources.
/// Flattened the same way RandomSource is, and for the same reason.
class PositionalRandomFactory {
public:
    [[nodiscard]] static PositionalRandomFactory legacy(std::int64_t seed) noexcept {
        return PositionalRandomFactory{RandomSource::Algorithm::Legacy, seed, 0};
    }

    [[nodiscard]] static PositionalRandomFactory xoroshiro(std::int64_t seedLo, std::int64_t seedHi) noexcept {
        return PositionalRandomFactory{RandomSource::Algorithm::Xoroshiro, seedLo, seedHi};
    }

    [[nodiscard]] RandomSource at(std::int32_t x, std::int32_t y, std::int32_t z) const noexcept;

    [[nodiscard]] RandomSource fromHashOf(std::string_view name) const noexcept;

    [[nodiscard]] RandomSource fromSeed(std::int64_t seed) const noexcept;

private:
    PositionalRandomFactory(RandomSource::Algorithm algorithm, std::int64_t seedLo, std::int64_t seedHi) noexcept
        : algorithm_(algorithm), seedLo_(seedLo), seedHi_(seedHi) {}

    RandomSource::Algorithm algorithm_;
    std::int64_t seedLo_;
    std::int64_t seedHi_;
};

} // namespace basalt::server::random
