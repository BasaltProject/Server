#include <basalt/server/world/noise/ImprovedNoise.hpp>

#include <cstdint>

namespace basalt::server::world::noise {

ImprovedNoise::ImprovedNoise(random::RandomSource& source) {
    xo_ = source.nextDouble() * 256.0;
    yo_ = source.nextDouble() * 256.0;
    zo_ = source.nextDouble() * 256.0;

    for (std::size_t i = 0; i < 256; ++i) {
        permutation_[i] = static_cast<std::uint8_t>(i);
    }
    for (std::int32_t i = 0; i < 256; ++i) {
        const std::int32_t offset = source.nextInt(256 - i);
        const auto at = static_cast<std::size_t>(i);
        const auto swapWith = static_cast<std::size_t>(i + offset);
        const std::uint8_t tmp = permutation_[at];
        permutation_[at] = permutation_[swapWith];
        permutation_[swapWith] = tmp;
    }
}

} // namespace basalt::server::world::noise
