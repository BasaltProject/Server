#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/carver/NetherCaveCarver.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <numbers>
#include <set>
#include <string>
#include <vector>

namespace basalt::server::world::terrain::carver {
namespace mth = wire::common::mth;

namespace {

constexpr std::int32_t kMinGenerationY = 0;
constexpr std::int32_t kGenerationHeight = 128;
constexpr std::int32_t kMaxStartY = 126;
constexpr std::int32_t kCarverRange = 8;
constexpr std::int32_t kCaveBound = 10;
constexpr std::int32_t kMaxDistance = 112;
constexpr std::int32_t kProtectedBlocksOnTop = 7;
constexpr float kStartProbability = 0.2F;
constexpr double kFloorLevel = -0.7;
constexpr double kRoomYScale = 0.5;
constexpr double kNetherTunnelYScale = 5.0;
constexpr auto kPiFloat = static_cast<float>(std::numbers::pi);
constexpr auto kHalfPi = static_cast<float>(std::numbers::pi / 2.0);
constexpr auto kTwoPi = static_cast<float>(std::numbers::pi * 2.0);

const std::set<std::string, std::less<>>& replaceableBlocks() {
    static const std::set<std::string, std::less<>> blocks = {"stone",
                                                              "granite",
                                                              "diorite",
                                                              "andesite",
                                                              "tuff",
                                                              "deepslate",
                                                              "netherrack",
                                                              "basalt",
                                                              "blackstone",
                                                              "dirt",
                                                              "coarse_dirt",
                                                              "rooted_dirt",
                                                              "mud",
                                                              "muddy_mangrove_roots",
                                                              "moss_block",
                                                              "pale_moss_block",
                                                              "grass_block",
                                                              "mycelium",
                                                              "podzol",
                                                              "crimson_nylium",
                                                              "warped_nylium",
                                                              "nether_wart_block",
                                                              "warped_wart_block",
                                                              "soul_sand",
                                                              "soul_soil"};
    return blocks;
}

[[nodiscard]] bool canReplace(std::int32_t stateId) {
    const std::optional<registry::BlockState> block = registry::blocks::byStateId(stateId);
    if (!block.has_value()) {
        return false;
    }
    return replaceableBlocks().find(block->block.path()) != replaceableBlocks().end();
}

[[nodiscard]] bool canReach(const Chunk& chunk, double x, double z, std::int32_t currentStep, std::int32_t totalSteps,
                            float thickness) {
    const double xd = x - (chunk.chunkX() * 16 + 8.0);
    const double zd = z - (chunk.chunkZ() * 16 + 8.0);
    const auto remaining = static_cast<double>(totalSteps - currentStep);
    const auto radius = static_cast<double>(thickness + 2.0F + 16.0F);
    return xd * xd + zd * zd - remaining * remaining <= radius * radius;
}

void carveEllipsoid(Chunk& chunk, double x, double y, double z, double horizontalRadius, double verticalRadius,
                    std::vector<bool>& mask) {
    const double centerX = chunk.chunkX() * 16 + 8.0;
    const double centerZ = chunk.chunkZ() * 16 + 8.0;
    const double maxDelta = 16.0 + horizontalRadius * 2.0;
    if (std::abs(x - centerX) > maxDelta || std::abs(z - centerZ) > maxDelta) {
        return;
    }

    const std::int32_t chunkMinX = chunk.chunkX() * 16;
    const std::int32_t chunkMinZ = chunk.chunkZ() * 16;
    const std::int32_t minLocalX = std::max(mth::floor(x - horizontalRadius) - chunkMinX - 1, 0);
    const std::int32_t maxLocalX = std::min(mth::floor(x + horizontalRadius) - chunkMinX, 15);
    const std::int32_t minY = std::max(mth::floor(y - verticalRadius) - 1, kMinGenerationY + 1);
    const std::int32_t maxY =
        std::min(mth::floor(y + verticalRadius) + 1, kMinGenerationY + kGenerationHeight - 1 - kProtectedBlocksOnTop);
    const std::int32_t minLocalZ = std::max(mth::floor(z - horizontalRadius) - chunkMinZ - 1, 0);
    const std::int32_t maxLocalZ = std::min(mth::floor(z + horizontalRadius) - chunkMinZ, 15);

    for (std::int32_t localX = minLocalX; localX <= maxLocalX; ++localX) {
        const std::int32_t worldX = chunkMinX + localX;
        const double xd = (worldX + 0.5 - x) / horizontalRadius;
        for (std::int32_t localZ = minLocalZ; localZ <= maxLocalZ; ++localZ) {
            const std::int32_t worldZ = chunkMinZ + localZ;
            const double zd = (worldZ + 0.5 - z) / horizontalRadius;
            if (xd * xd + zd * zd >= 1.0) {
                continue;
            }
            for (std::int32_t worldY = maxY; worldY >= minY + 1; --worldY) {
                const double yd = (worldY - 0.5 - y) / verticalRadius;
                if (yd <= kFloorLevel || xd * xd + yd * yd + zd * zd >= 1.0) {
                    continue;
                }
                const auto maskIndex = static_cast<std::size_t>((worldY * 16 + localZ) * 16 + localX);
                if (mask[maskIndex]) {
                    continue;
                }
                mask[maskIndex] = true;
                if (canReplace(chunk.blockStateId(localX, worldY, localZ))) {
                    chunk.setBlockStateId(localX, worldY, localZ,
                                          worldY <= kMinGenerationY + 31 ? registry::blocks::lava().stateId
                                                                         : registry::blocks::caveAir().stateId);
                }
            }
        }
    }
}

void createTunnel(Chunk& chunk, std::int64_t tunnelSeed, double xStart, double yStart, double zStart, float thickness,
                  float horizontalRotationStart, float verticalRotationStart, std::int32_t step, std::int32_t distance,
                  double yScale, std::vector<bool>& mask) {
    random::RandomSource random = random::RandomSource::legacy(tunnelSeed);
    const std::int32_t splitPoint = random.nextInt(distance / 2) + distance / 4;
    const bool steep = random.nextInt(6) == 0;
    double x = xStart;
    double y = yStart;
    double z = zStart;
    float horizontalRotation = horizontalRotationStart;
    float verticalRotation = verticalRotationStart;
    float verticalRotationVelocity = 0.0F;
    float horizontalRotationVelocity = 0.0F;

    for (std::int32_t currentStep = step; currentStep < distance; ++currentStep) {
        const double horizontalRadius =
            1.5 + static_cast<double>(
                      mth::sin(kPiFloat * static_cast<float>(currentStep) / static_cast<float>(distance)) * thickness);
        const double verticalRadius = horizontalRadius * yScale;
        const float cosVertical = mth::cos(verticalRotation);
        x += static_cast<double>(mth::cos(horizontalRotation) * cosVertical);
        y += static_cast<double>(mth::sin(verticalRotation));
        z += static_cast<double>(mth::sin(horizontalRotation) * cosVertical);
        verticalRotation *= steep ? 0.92F : 0.7F;
        verticalRotation += verticalRotationVelocity * 0.1F;
        horizontalRotation += horizontalRotationVelocity * 0.1F;
        verticalRotationVelocity *= 0.9F;
        horizontalRotationVelocity *= 0.75F;
        verticalRotationVelocity += (random.nextFloat() - random.nextFloat()) * random.nextFloat() * 2.0F;
        horizontalRotationVelocity += (random.nextFloat() - random.nextFloat()) * random.nextFloat() * 4.0F;

        if (currentStep == splitPoint && thickness > 1.0F) {
            const std::int64_t firstSeed = random.nextLong();
            const float firstThickness = random.nextFloat() * 0.5F + 0.5F;
            createTunnel(chunk, firstSeed, x, y, z, firstThickness, horizontalRotation - kHalfPi,
                         verticalRotation / 3.0F, currentStep, distance, 1.0, mask);
            const std::int64_t secondSeed = random.nextLong();
            const float secondThickness = random.nextFloat() * 0.5F + 0.5F;
            createTunnel(chunk, secondSeed, x, y, z, secondThickness, horizontalRotation + kHalfPi,
                         verticalRotation / 3.0F, currentStep, distance, 1.0, mask);
            return;
        }

        if (random.nextInt(4) != 0) {
            if (!canReach(chunk, x, z, currentStep, distance, thickness)) {
                return;
            }
            carveEllipsoid(chunk, x, y, z, horizontalRadius, verticalRadius, mask);
        }
    }
}

void carveSource(Chunk& chunk, random::RandomSource& random, std::int32_t sourceChunkX, std::int32_t sourceChunkZ,
                 std::vector<bool>& mask) {
    const std::int32_t caveCount = random.nextInt(random.nextInt(random.nextInt(kCaveBound) + 1) + 1);
    for (std::int32_t cave = 0; cave < caveCount; ++cave) {
        const double x = sourceChunkX * 16 + static_cast<double>(random.nextInt(16));
        const auto y = static_cast<double>(random.nextInt(kMaxStartY + 1));
        const double z = sourceChunkZ * 16 + static_cast<double>(random.nextInt(16));
        std::int32_t tunnels = 1;
        if (random.nextInt(4) == 0) {
            const float thickness = 1.0F + random.nextFloat() * 6.0F;
            const double horizontalRadius = 1.5 + static_cast<double>(mth::sin(kHalfPi) * thickness);
            carveEllipsoid(chunk, x + 1.0, y, z, horizontalRadius, horizontalRadius * kRoomYScale, mask);
            tunnels += random.nextInt(4);
        }

        for (std::int32_t tunnel = 0; tunnel < tunnels; ++tunnel) {
            const float horizontalRotation = random.nextFloat() * kTwoPi;
            const float verticalRotation = (random.nextFloat() - 0.5F) / 4.0F;
            const float thickness = (random.nextFloat() * 2.0F + random.nextFloat()) * 2.0F;
            const std::int32_t distance = kMaxDistance - random.nextInt(kMaxDistance / 4);
            createTunnel(chunk, random.nextLong(), x, y, z, thickness, horizontalRotation, verticalRotation, 0,
                         distance, kNetherTunnelYScale, mask);
        }
    }
}

} // namespace

void carveNether(Chunk& chunk, std::int64_t seed) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::legacy(0LL));
    std::vector<bool> mask(static_cast<std::size_t>(kGenerationHeight) * 16 * 16, false);
    for (std::int32_t dx = -kCarverRange; dx <= kCarverRange; ++dx) {
        for (std::int32_t dz = -kCarverRange; dz <= kCarverRange; ++dz) {
            const std::int32_t sourceChunkX = chunk.chunkX() + dx;
            const std::int32_t sourceChunkZ = chunk.chunkZ() + dz;
            random::setLargeFeatureSeed(random, seed, sourceChunkX, sourceChunkZ);
            if (random.nextFloat() <= kStartProbability) {
                carveSource(chunk, random, sourceChunkX, sourceChunkZ, mask);
            }
        }
    }
}

} // namespace basalt::server::world::terrain::carver
