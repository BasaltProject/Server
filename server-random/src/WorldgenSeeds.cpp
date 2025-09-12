#include <basalt/server/random/WorldgenSeeds.hpp>

namespace basalt::server::random {
namespace {

constexpr std::int64_t wrap(std::uint64_t value) noexcept { return static_cast<std::int64_t>(value); }

constexpr std::uint64_t bits(std::int64_t value) noexcept { return static_cast<std::uint64_t>(value); }

} // namespace

std::int64_t setDecorationSeed(RandomSource& random, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) {
    random.setSeed(seed);
    const std::int64_t xScale = random.nextLong() | 1LL;
    const std::int64_t zScale = random.nextLong() | 1LL;
    const std::int64_t result = wrap((bits(static_cast<std::int64_t>(chunkX)) * bits(xScale) +
                                      bits(static_cast<std::int64_t>(chunkZ)) * bits(zScale)) ^
                                     bits(seed));
    random.setSeed(result);
    return result;
}

void setFeatureSeed(RandomSource& random, std::int64_t seed, std::int32_t index, std::int32_t step) {
    random.setSeed(wrap(bits(seed) + bits(index) + 10000ULL * bits(step)));
}

void setLargeFeatureSeed(RandomSource& random, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ) {
    random.setSeed(seed);
    const std::int64_t xScale = random.nextLong();
    const std::int64_t zScale = random.nextLong();
    random.setSeed(wrap((bits(static_cast<std::int64_t>(chunkX)) * bits(xScale)) ^
                        (bits(static_cast<std::int64_t>(chunkZ)) * bits(zScale)) ^ bits(seed)));
}

void setLargeFeatureWithSalt(RandomSource& random, std::int64_t seed, std::int32_t x, std::int32_t z,
                             std::int32_t blend) {
    random.setSeed(wrap(bits(static_cast<std::int64_t>(x)) * 341873128712ULL +
                        bits(static_cast<std::int64_t>(z)) * 132897987541ULL + bits(seed) +
                        bits(static_cast<std::int64_t>(blend))));
}

} // namespace basalt::server::random
