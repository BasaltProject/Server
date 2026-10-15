#pragma once

#include <basalt/server/random/RandomSource.hpp>

#include <cstdint>

// parity: the seeding helpers of net.minecraft.world.level.levelgen.WorldgenRandom.
namespace basalt::server::random {
// parity: WorldgenRandom.setDecorationSeed.
std::int64_t setDecorationSeed(RandomSource& random, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ);

// parity: WorldgenRandom.setFeatureSeed.
void setFeatureSeed(RandomSource& random, std::int64_t seed, std::int32_t index, std::int32_t step);

// parity: WorldgenRandom.setLargeFeatureSeed.
void setLargeFeatureSeed(RandomSource& random, std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ);

// parity: WorldgenRandom.setLargeFeatureWithSalt.
void setLargeFeatureWithSalt(RandomSource& random, std::int64_t seed, std::int32_t x, std::int32_t z,
                             std::int32_t blend);
} // namespace basalt::server::random
