#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/NoiseWorldGenerator.hpp>
#include <basalt/server/world/WorldHeight.hpp>
#include <basalt/server/world/densityfunction/DensityFunction.hpp>
#include <basalt/server/world/densityfunction/SamplerContext.hpp>
#include <basalt/server/world/noise/Noise.hpp>
#include <basalt/server/world/noise/NormalNoise.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace basalt::server::world::noiseWorldGenerator {
namespace {

constexpr std::int64_t kSeed = 0;
constexpr std::int32_t kSeaLevel = 63;
constexpr double kHeightVariation = 6.0;
constexpr double kDensitySlope = 0.08;
constexpr std::int32_t kSurfaceDepth = 4;
constexpr std::int32_t kCellHeight = 4;

constexpr double kHeightNoiseScale = 0.25;

constexpr double kSkipThreshold = 2.0;

constexpr std::int32_t kMaxTreesPerChunk = 2;
constexpr std::int32_t kMinTrunkHeight = 4;
constexpr std::int32_t kTrunkHeightVariation = 2;

/// The terrain noise seeded with xoroshiro zero, as vanilla's unseeded blended noise is.
[[nodiscard]] const densityfunction::DensitySampler& base3d() {
    static const densityfunction::Sampler noise = [] {
        random::RandomSource source = random::RandomSource::xoroshiro(0);
        return densityfunction::functions::compileBlendedNoise(source, 0.25, 0.125, 80.0, 160.0, 8.0);
    }();
    return *noise;
}

[[nodiscard]] const noise::Noise& heightNoise() {
    static const std::shared_ptr<const noise::NoiseStack> built = [] {
        random::RandomSource source = random::RandomSource::legacy(kSeed);
        static constexpr std::array<double, 5> kAmplitudes{1.0, 1.0, 1.0, 1.0, 1.0};
        return noise::NormalNoise::createParity(-5, kAmplitudes).create(source);
    }();
    return *built;
}

void generateColumn(Chunk& chunk, std::int32_t x, std::int32_t z, std::int32_t worldX, std::int32_t worldZ) {
    const std::int32_t surfaceY =
        kSeaLevel + static_cast<std::int32_t>(static_cast<double>(heightNoise().get(worldX * kHeightNoiseScale, 0.0,
                                                                                    worldZ * kHeightNoiseScale)) *
                                              kHeightVariation);
    double cellBase3d = 0.0;
    for (std::int32_t y = worldHeight::kMinY; y <= worldHeight::kMaxY; ++y) {
        const double depthTerm = (surfaceY - y) * kDensitySlope;
        bool solid = false;
        if (depthTerm > kSkipThreshold) {
            solid = true;
        } else if (depthTerm < -kSkipThreshold) {
            solid = false;
        } else {
            if ((y - worldHeight::kMinY) % kCellHeight == 0) {
                densityfunction::SamplerContext context{densityfunction::SamplerContext::Options{}};
                cellBase3d = static_cast<double>(base3d().sampleValue(context, worldX, y, worldZ));
            }
            solid = (cellBase3d + depthTerm) > 0.0;
        }
        if (solid) {
            chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
        }
    }
    chunk.setBlockStateId(x, worldHeight::kMinY, z, registry::blocks::bedrock().stateId);

    const std::int32_t stone = registry::blocks::stone().stateId;
    const std::int32_t grass = registry::blocks::named("grass_block")->stateId;
    const std::int32_t dirt = registry::blocks::named("dirt")->stateId;
    // Down to one above the floor, so the bedrock below is never turned into dirt.
    std::int32_t depth = 0;
    for (std::int32_t y = worldHeight::kMaxY; y > worldHeight::kMinY; --y) {
        if (chunk.blockStateId(x, y, z) != stone) {
            if (depth > 0) {
                break;
            }
            continue;
        }
        chunk.setBlockStateId(x, y, z, depth == 0 ? grass : dirt);
        ++depth;
        if (depth >= kSurfaceDepth) {
            break;
        }
    }
}

void placeCanopy(Chunk& chunk, std::int32_t trunkX, std::int32_t trunkTopY, std::int32_t trunkZ) {
    const std::int32_t leaves = registry::blocks::named("oak_leaves")->stateId;
    for (std::int32_t dy = -2; dy <= 0; ++dy) {
        const std::int32_t radius = dy == 0 ? 1 : 2;
        for (std::int32_t dx = -radius; dx <= radius; ++dx) {
            for (std::int32_t dz = -radius; dz <= radius; ++dz) {
                if (dx == 0 && dz == 0 && dy < 0) {
                    continue;
                }
                const std::int32_t px = trunkX + dx;
                const std::int32_t pz = trunkZ + dz;
                if (px < 0 || px > 15 || pz < 0 || pz > 15) {
                    continue;
                }
                const std::int32_t py = trunkTopY + dy;
                if (registry::blocks::isAir(chunk.blockStateId(px, py, pz))) {
                    chunk.setBlockStateId(px, py, pz, leaves);
                }
            }
        }
    }
}

/// Not vanilla's own per-chunk feature seed, just a stand-in hash.
[[nodiscard]] std::int64_t chunkSeed(std::int32_t chunkX, std::int32_t chunkZ) {
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(static_cast<std::int64_t>(chunkX)) *
                                     0x9E3779B97F4A7C15ULL) ^
           static_cast<std::int64_t>(static_cast<std::uint64_t>(static_cast<std::int64_t>(chunkZ)) *
                                     0xBF58476D1CE4E5B9ULL) ^
           kSeed;
}

void placeTrees(Chunk& chunk, std::int32_t chunkX, std::int32_t chunkZ) {
    random::RandomSource rolls = random::RandomSource::legacy(chunkSeed(chunkX, chunkZ));
    const std::int32_t treeCount = rolls.nextInt(kMaxTreesPerChunk + 1);
    const std::int32_t grass = registry::blocks::named("grass_block")->stateId;
    const std::int32_t log = registry::blocks::named("oak_log")->stateId;
    for (std::int32_t tree = 0; tree < treeCount; ++tree) {
        const std::int32_t x = rolls.nextInt(16);
        const std::int32_t z = rolls.nextInt(16);
        const std::int32_t topY = chunk.heightmap()[static_cast<std::size_t>(x + z * 16)];
        if (chunk.blockStateId(x, topY - 1, z) != grass) {
            continue;
        }
        const std::int32_t trunkHeight = kMinTrunkHeight + rolls.nextInt(kTrunkHeightVariation + 1);
        if (topY + trunkHeight + 1 > worldHeight::kMaxY) {
            continue;
        }
        for (std::int32_t dy = 0; dy < trunkHeight; ++dy) {
            chunk.setBlockStateId(x, topY + dy, z, log);
        }
        placeCanopy(chunk, x, topY + trunkHeight, z);
    }
}

} // namespace

Chunk generate(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            generateColumn(chunk, x, z, chunkX * 16 + x, chunkZ * 16 + z);
        }
    }
    placeTrees(chunk, chunkX, chunkZ);
    return chunk;
}

} // namespace basalt::server::world::noiseWorldGenerator
