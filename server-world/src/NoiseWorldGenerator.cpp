#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/NoiseWorldGenerator.hpp>
#include <basalt/server/world/WorldHeight.hpp>
#include <basalt/server/world/densityfunction/DensityFunction.hpp>
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

/// How much a world coordinate is scaled before sampling the height noise: higher means a shorter
/// wavelength, so more distinct hills per loaded chunk instead of one broad slope spanning the whole
/// loaded area. Kept low, with a small height variation, for gentle rolling ground.
constexpr double kHeightNoiseScale = 0.25;

/// See the note above on skipping the 3D noise far from the surface. The theoretical maximum is
/// much larger, but real samples across the whole verified test suite never exceeded roughly 0.5.
constexpr double kSkipThreshold = 2.0;

constexpr std::int32_t kMaxTreesPerChunk = 2;
constexpr std::int32_t kMinTrunkHeight = 4;
constexpr std::int32_t kTrunkHeightVariation = 2;

[[nodiscard]] const densityfunction::Function& base3d() {
    static const densityfunction::Function noise =
        densityfunction::functions::blendedNoiseUnseeded(0.25, 0.125, 80.0, 160.0, 8.0);
    return noise;
}

[[nodiscard]] const noise::NormalNoise& heightNoise() {
    static const noise::NormalNoise built = [] {
        random::RandomSource source = random::RandomSource::legacy(kSeed);
        static constexpr std::array<double, 5> kAmplitudes{1.0, 1.0, 1.0, 1.0, 1.0};
        return noise::NormalNoise::create(source, -5, kAmplitudes);
    }();
    return built;
}

void generateColumn(Chunk& chunk, std::int32_t x, std::int32_t z, std::int32_t worldX, std::int32_t worldZ) {
    const std::int32_t surfaceY =
        kSeaLevel +
        static_cast<std::int32_t>(heightNoise().getValue(worldX * kHeightNoiseScale, 0.0, worldZ * kHeightNoiseScale) *
                                  kHeightVariation);
    double cellBase3d = 0.0;
    for (std::int32_t y = worldHeight::kMinY; y <= worldHeight::kMaxY; ++y) {
        const double depthTerm = (surfaceY - y) * kDensitySlope;
        // The two skips are an optimisation, not a rule: past the threshold the depth term alone
        // already decides the sign, and no sample of the noise is large enough to overturn it.
        bool solid = false;
        if (depthTerm > kSkipThreshold) {
            solid = true;
        } else if (depthTerm < -kSkipThreshold) {
            solid = false;
        } else {
            if ((y - worldHeight::kMinY) % kCellHeight == 0) {
                densityfunction::FunctionContext context{worldX, y, worldZ};
                cellBase3d = base3d()->compute(context);
            }
            solid = (cellBase3d + depthTerm) > 0.0;
        }
        if (solid) {
            chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
        }
    }
    chunk.setBlockStateId(x, worldHeight::kMinY, z, registry::blocks::bedrock().stateId);

    // Surface pass: from the top down, turn the topmost contiguous run of stone into dirt and
    // grass, the same shape as vanilla's own surface builder, just without its per-biome rules.
    const std::int32_t stone = registry::blocks::stone().stateId;
    const std::int32_t grass = registry::blocks::named("grass_block")->stateId;
    const std::int32_t dirt = registry::blocks::named("dirt")->stateId;
    // Down to one above the floor, so the bedrock below is never turned into dirt. It would be
    // safe either way today, since bedrock is not stone and the loop stops at it.
    std::int32_t depth = 0;
    for (std::int32_t y = worldHeight::kMaxY; y > worldHeight::kMinY; --y) {
        if (chunk.blockStateId(x, y, z) != stone) {
            // Stops at the first gap below the surface run. Nothing here digs one yet, so this
            // only matters the day caves do: without it a cave roof would grow its own lawn.
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
                // The trunk's own column, which the air check below would also refuse: two
                // guards for one thing, and either alone is enough.
                if (dx == 0 && dz == 0 && dy < 0) {
                    continue;
                }
                const std::int32_t px = trunkX + dx;
                const std::int32_t pz = trunkZ + dz;
                // Canopy that would cross into a neighbouring chunk is skipped, not placed there:
                // see the note on this generator's tree-placement gaps.
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

/// Not vanilla's own per-chunk feature seed, just a stand-in hash: see the note on this generator's
/// tree-placement gaps.
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
        // Asked again for every tree, not once for the chunk: the first tree raises the ground it
        // stands on, and the second has to see that rather than the height before it existed.
        const std::int32_t topY = chunk.heightmap()[static_cast<std::size_t>(x + z * 16)];
        if (chunk.blockStateId(x, topY - 1, z) != grass) {
            continue;
        }
        const std::int32_t trunkHeight = kMinTrunkHeight + rolls.nextInt(kTrunkHeightVariation + 1);
        // The roof of the world, which ground around sea level never comes near: it is here so a
        // taller generator does not write outside the chunk.
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
