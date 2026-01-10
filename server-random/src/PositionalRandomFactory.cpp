#include <basalt/server/random/PositionalHashing.hpp>
#include <basalt/server/random/PositionalRandomFactory.hpp>

#include <cstdint>

namespace basalt::server::random {

RandomSource PositionalRandomFactory::at(std::int32_t x, std::int32_t y, std::int32_t z) const noexcept {
    const std::int64_t seed = positionalSeed(x, y, z);
    if (algorithm_ == RandomSource::Algorithm::Legacy) {
        return RandomSource::legacy(seed ^ seedLo_);
    }
    return RandomSource::xoroshiro(seed ^ seedLo_, seedHi_);
}

RandomSource PositionalRandomFactory::fromHashOf(std::string_view name) const noexcept {
    if (algorithm_ == RandomSource::Algorithm::Legacy) {
        return RandomSource::legacy(static_cast<std::int64_t>(javaStringHashCode(name)) ^ seedLo_);
    }
    return RandomSource::xoroshiro(seedFromHashOf(name).xorWith(seedLo_, seedHi_));
}

RandomSource PositionalRandomFactory::fromSeed(std::int64_t seed) const noexcept {
    if (algorithm_ == RandomSource::Algorithm::Legacy) {
        // parity: LegacyPositionalRandomFactory.fromSeed ignores the factory seed entirely.
        return RandomSource::legacy(seed);
    }
    return RandomSource::xoroshiro(seed ^ seedLo_, seed ^ seedHi_);
}

} // namespace basalt::server::random
