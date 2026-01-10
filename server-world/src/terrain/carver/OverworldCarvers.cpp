#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/carver/OverworldCarvers.hpp>
#include <basalt/server/world/terrain/surface/SurfaceBuilder.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <numbers>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::world::terrain::carver {
namespace mth = wire::common::mth;

namespace {

constexpr std::int32_t kSourceRange = 8;
constexpr std::int32_t kCaveBound = 15;
constexpr std::int32_t kCaveMinY = -56;
constexpr std::int32_t kCanyonMinY = 10;
constexpr std::int32_t kCanyonMaxY = 67;
constexpr float kCanyonProbability = 0.01F;
constexpr double kCanyonYScale = 3.0;
constexpr std::int32_t kCanyonWidthSmoothness = 3;
constexpr std::int32_t kMaxDistance = 112;
constexpr std::int32_t kLavaLevel = -56;
constexpr std::int32_t kProtectedBlocksOnTop = 7;
constexpr auto kPiFloat = static_cast<float>(std::numbers::pi);
constexpr auto kHalfPi = static_cast<float>(std::numbers::pi / 2.0);
constexpr auto kTwoPi = static_cast<float>(std::numbers::pi * 2.0);

constexpr std::int32_t kMinY = kOverworldHeight.minY();
constexpr std::int32_t kHeight = kOverworldHeight.height();

struct CaveConfiguration {
    float probability;
    std::int32_t maxY;
};

constexpr std::array<CaveConfiguration, 2> kCaveConfigs = {{{0.15F, 180}, {0.07F, 47}}};

/// Which blocks a carver is allowed to cut through. Anything else stops it.
const std::set<std::string, std::less<>>& replaceableBlocks() {
    static const std::set<std::string, std::less<>> blocks = [] {
        std::set<std::string, std::less<>> built = {"stone",
                                                    "granite",
                                                    "diorite",
                                                    "andesite",
                                                    "tuff",
                                                    "deepslate",
                                                    "dirt",
                                                    "coarse_dirt",
                                                    "rooted_dirt",
                                                    "mud",
                                                    "muddy_mangrove_roots",
                                                    "moss_block",
                                                    "pale_moss_block",
                                                    "grass_block",
                                                    "podzol",
                                                    "mycelium",
                                                    "sand",
                                                    "red_sand",
                                                    "suspicious_sand",
                                                    "iron_ore",
                                                    "deepslate_iron_ore",
                                                    "copper_ore",
                                                    "deepslate_copper_ore",
                                                    "snow",
                                                    "snow_block",
                                                    "powder_snow",
                                                    "water",
                                                    "gravel",
                                                    "suspicious_gravel",
                                                    "sandstone",
                                                    "red_sandstone",
                                                    "calcite",
                                                    "packed_ice",
                                                    "raw_iron_block",
                                                    "raw_copper_block",
                                                    "cinnabar",
                                                    "sulfur",
                                                    "potent_sulfur"};
        for (const std::string_view color :
             {"", "white", "orange", "magenta", "light_blue", "yellow", "lime", "pink", "gray", "light_gray", "cyan",
              "purple", "blue", "brown", "green", "red", "black"}) {
            built.insert(color.empty() ? std::string{"terracotta"} : std::string{color} + "_terracotta");
        }
        return built;
    }();
    return blocks;
}

[[nodiscard]] float uniformFloat(random::RandomSource& random, float min, float maxExclusive) {
    return random.nextFloat() * (maxExclusive - min) + min;
}

[[nodiscard]] float trapezoidFloat(random::RandomSource& random, float min, float max, float plateau) {
    const float range = max - min;
    const float plateauStart = (range - plateau) / 2.0F;
    const float plateauEnd = range - plateauStart;
    return min + random.nextFloat() * plateauEnd + random.nextFloat() * plateauStart;
}

[[nodiscard]] bool canReach(const Chunk& chunk, double x, double z, std::int32_t currentStep, std::int32_t totalSteps,
                            float thickness) {
    const double xd = x - (chunk.chunkX() * 16 + 8.0);
    const double zd = z - (chunk.chunkZ() * 16 + 8.0);
    const auto remaining = static_cast<double>(totalSteps - currentStep);
    const auto radius = static_cast<double>(thickness + 2.0F + 16.0F);
    return xd * xd + zd * zd - remaining * remaining <= radius * radius;
}

/// Everything the carve loop needs but never changes, so the recursive tunnel walkers stay readable.
struct CarveScope {
    Chunk& chunk;
    OverworldAquifer& aquifer;
    const surface::SurfaceRuleEnvironment& environment;
    const surface::RuleSource& surfaceRule;
    std::vector<bool>& mask;
};

using SkipTest = std::function<bool(double, double, double, std::int32_t)>;

void carveEllipsoid(const CarveScope& scope, double x, double y, double z, double horizontalRadius,
                    double verticalRadius, const SkipTest& shouldSkip) {
    const double centerX = scope.chunk.chunkX() * 16 + 8.0;
    const double centerZ = scope.chunk.chunkZ() * 16 + 8.0;
    const double maxDelta = 16.0 + horizontalRadius * 2.0;
    if (std::abs(x - centerX) > maxDelta || std::abs(z - centerZ) > maxDelta) {
        return;
    }

    const std::int32_t chunkMinX = scope.chunk.chunkX() * 16;
    const std::int32_t chunkMinZ = scope.chunk.chunkZ() * 16;
    const std::int32_t minLocalX = std::max(mth::floor(x - horizontalRadius) - chunkMinX - 1, 0);
    const std::int32_t maxLocalX = std::min(mth::floor(x + horizontalRadius) - chunkMinX, 15);
    const std::int32_t minY = std::max(mth::floor(y - verticalRadius) - 1, kMinY + 1);
    const std::int32_t maxY = std::min(mth::floor(y + verticalRadius) + 1, kMinY + kHeight - 1 - kProtectedBlocksOnTop);
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
            bool hasGrass = false;
            for (std::int32_t worldY = maxY; worldY >= minY + 1; --worldY) {
                const double yd = (worldY - 0.5 - y) / verticalRadius;
                if (shouldSkip(xd, yd, zd, worldY)) {
                    continue;
                }
                const auto maskIndex = static_cast<std::size_t>(((worldY - kMinY) * 16 + localZ) * 16 + localX);
                if (scope.mask[maskIndex]) {
                    continue;
                }
                scope.mask[maskIndex] = true;
                const std::int32_t oldStateId = scope.chunk.blockStateId(localX, worldY, localZ);
                const std::optional<registry::BlockState> old = registry::blocks::byStateId(oldStateId);
                const std::string oldPath = old.has_value() ? old->block.path() : std::string{};
                if (oldPath == "grass_block" || oldPath == "mycelium") {
                    hasGrass = true;
                }
                if (replaceableBlocks().find(oldPath) == replaceableBlocks().end()) {
                    continue;
                }

                std::int32_t stateId = 0;
                if (worldY <= kLavaLevel) {
                    stateId = registry::blocks::lava().stateId;
                } else {
                    const registry::BlockState* substance = scope.aquifer.computeSubstance(worldX, worldY, worldZ, 0.0);
                    if (substance == nullptr) {
                        continue;
                    }
                    stateId = substance->stateId;
                }
                scope.chunk.setBlockStateId(localX, worldY, localZ, stateId);
                if (hasGrass && worldY - 1 >= kMinY) {
                    const std::int32_t belowStateId = scope.chunk.blockStateId(localX, worldY - 1, localZ);
                    const std::optional<registry::BlockState> below = registry::blocks::byStateId(belowStateId);
                    if (below.has_value() && below->block.path() == "dirt") {
                        const std::optional<std::int32_t> topMaterial = surface::surfaceBuilder::topMaterial(
                            scope.environment, scope.surfaceRule, worldX, worldY - 1, worldZ,
                            registry::blockFluids::hasFluid(stateId));
                        if (topMaterial.has_value()) {
                            scope.chunk.setBlockStateId(localX, worldY - 1, localZ, *topMaterial);
                        }
                    }
                }
            }
        }
    }
}

void createCaveRoom(const CarveScope& scope, double x, double y, double z, float thickness, double yScale,
                    float floorLevel) {
    const double horizontalRadius = 1.5 + static_cast<double>(mth::sin(kHalfPi) * thickness);
    carveEllipsoid(scope, x + 1.0, y, z, horizontalRadius, horizontalRadius * yScale,
                   [floorLevel](double xd, double yd, double zd, std::int32_t) {
                       return yd <= static_cast<double>(floorLevel) || xd * xd + yd * yd + zd * zd >= 1.0;
                   });
}

void createCaveTunnel(const CarveScope& scope, std::int64_t tunnelSeed, double xStart, double yStart, double zStart,
                      double horizontalRadiusMultiplier, double verticalRadiusMultiplier, float thickness,
                      float horizontalRotationStart, float verticalRotationStart, std::int32_t step,
                      std::int32_t distance, double yScale, float floorLevel) {
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
            createCaveTunnel(scope, firstSeed, x, y, z, horizontalRadiusMultiplier, verticalRadiusMultiplier,
                             firstThickness, horizontalRotation - kHalfPi, verticalRotation / 3.0F, currentStep,
                             distance, 1.0, floorLevel);
            const std::int64_t secondSeed = random.nextLong();
            const float secondThickness = random.nextFloat() * 0.5F + 0.5F;
            createCaveTunnel(scope, secondSeed, x, y, z, horizontalRadiusMultiplier, verticalRadiusMultiplier,
                             secondThickness, horizontalRotation + kHalfPi, verticalRotation / 3.0F, currentStep,
                             distance, 1.0, floorLevel);
            return;
        }

        if (random.nextInt(4) != 0) {
            if (!canReach(scope.chunk, x, z, currentStep, distance, thickness)) {
                return;
            }
            carveEllipsoid(scope, x, y, z, horizontalRadius * horizontalRadiusMultiplier,
                           verticalRadius * verticalRadiusMultiplier,
                           [floorLevel](double xd, double yd, double zd, std::int32_t) {
                               return yd <= static_cast<double>(floorLevel) || xd * xd + yd * yd + zd * zd >= 1.0;
                           });
        }
    }
}

[[nodiscard]] std::vector<float> initCanyonWidthFactors(random::RandomSource& random) {
    std::vector<float> factors(static_cast<std::size_t>(kHeight));
    float factor = 1.0F;
    for (std::size_t index = 0; index < factors.size(); ++index) {
        if (index == 0 || random.nextInt(kCanyonWidthSmoothness) == 0) {
            factor = 1.0F + random.nextFloat() * random.nextFloat();
        }
        factors[index] = factor * factor;
    }
    return factors;
}

void createCanyonTunnel(const CarveScope& scope, std::int64_t tunnelSeed, double xStart, double yStart, double zStart,
                        float thickness, float horizontalRotationStart, float verticalRotationStart,
                        std::int32_t distance) {
    random::RandomSource random = random::RandomSource::legacy(tunnelSeed);
    const std::vector<float> widthFactors = initCanyonWidthFactors(random);
    double x = xStart;
    double y = yStart;
    double z = zStart;
    float horizontalRotation = horizontalRotationStart;
    float verticalRotation = verticalRotationStart;
    float verticalRotationVelocity = 0.0F;
    float horizontalRotationVelocity = 0.0F;

    for (std::int32_t currentStep = 0; currentStep < distance; ++currentStep) {
        double horizontalRadius =
            1.5 + static_cast<double>(
                      mth::sin(static_cast<float>(currentStep) * kPiFloat / static_cast<float>(distance)) * thickness);
        double verticalRadius = horizontalRadius * kCanyonYScale;
        horizontalRadius *= static_cast<double>(uniformFloat(random, 0.75F, 1.0F));
        const float verticalMultiplier =
            1.0F - std::abs(0.5F - static_cast<float>(currentStep) / static_cast<float>(distance)) * 2.0F;
        // parity: vanilla multiplies the ramp by zero here, so it contributes nothing.
        const float verticalFactor = 1.0F + 0.0F * verticalMultiplier;
        verticalRadius *= static_cast<double>(verticalFactor * uniformFloat(random, 0.75F, 1.0F));
        const float cosVertical = mth::cos(verticalRotation);
        x += static_cast<double>(mth::cos(horizontalRotation) * cosVertical);
        y += static_cast<double>(mth::sin(verticalRotation));
        z += static_cast<double>(mth::sin(horizontalRotation) * cosVertical);
        verticalRotation *= 0.7F;
        verticalRotation += verticalRotationVelocity * 0.05F;
        horizontalRotation += horizontalRotationVelocity * 0.05F;
        verticalRotationVelocity *= 0.8F;
        horizontalRotationVelocity *= 0.5F;
        verticalRotationVelocity += (random.nextFloat() - random.nextFloat()) * random.nextFloat() * 2.0F;
        horizontalRotationVelocity += (random.nextFloat() - random.nextFloat()) * random.nextFloat() * 4.0F;

        if (random.nextInt(4) != 0) {
            if (!canReach(scope.chunk, x, z, currentStep, distance, thickness)) {
                return;
            }
            carveEllipsoid(scope, x, y, z, horizontalRadius, verticalRadius,
                           [&widthFactors](double xd, double yd, double zd, std::int32_t worldY) {
                               const float widthFactor = widthFactors[static_cast<std::size_t>(worldY - kMinY - 1)];
                               return (xd * xd + zd * zd) * static_cast<double>(widthFactor) + yd * yd / 6.0 >= 1.0;
                           });
        }
    }
}

void carveCaveSource(const CarveScope& scope, random::RandomSource& random, std::int32_t sourceChunkX,
                     std::int32_t sourceChunkZ, const CaveConfiguration& configuration) {
    const std::int32_t caveCount = random.nextInt(random.nextInt(random.nextInt(kCaveBound) + 1) + 1);
    for (std::int32_t cave = 0; cave < caveCount; ++cave) {
        const double x = sourceChunkX * 16 + static_cast<double>(random.nextInt(16));
        const std::int32_t y = random.nextInt(configuration.maxY - kCaveMinY + 1) + kCaveMinY;
        const double z = sourceChunkZ * 16 + static_cast<double>(random.nextInt(16));
        const auto horizontalRadiusMultiplier = static_cast<double>(uniformFloat(random, 0.7F, 1.4F));
        const auto verticalRadiusMultiplier = static_cast<double>(uniformFloat(random, 0.8F, 1.3F));
        const float floorLevel = uniformFloat(random, -1.0F, -0.4F);
        std::int32_t tunnels = 1;
        if (random.nextInt(4) == 0) {
            const auto yScale = static_cast<double>(uniformFloat(random, 0.1F, 0.9F));
            const float thickness = 1.0F + random.nextFloat() * 6.0F;
            createCaveRoom(scope, x, static_cast<double>(y), z, thickness, yScale, floorLevel);
            tunnels += random.nextInt(4);
        }

        for (std::int32_t tunnel = 0; tunnel < tunnels; ++tunnel) {
            const float horizontalRotation = random.nextFloat() * kTwoPi;
            const float verticalRotation = (random.nextFloat() - 0.5F) / 4.0F;
            float thickness = random.nextFloat() * 2.0F + random.nextFloat();
            if (random.nextInt(10) == 0) {
                thickness *= random.nextFloat() * random.nextFloat() * 3.0F + 1.0F;
            }
            const std::int32_t distance = kMaxDistance - random.nextInt(kMaxDistance / 4);
            createCaveTunnel(scope, random.nextLong(), x, static_cast<double>(y), z, horizontalRadiusMultiplier,
                             verticalRadiusMultiplier, thickness, horizontalRotation, verticalRotation, 0, distance,
                             1.0, floorLevel);
        }
    }
}

void carveCanyonSource(const CarveScope& scope, random::RandomSource& random, std::int32_t sourceChunkX,
                       std::int32_t sourceChunkZ) {
    const double x = sourceChunkX * 16 + static_cast<double>(random.nextInt(16));
    const std::int32_t y = random.nextInt(kCanyonMaxY - kCanyonMinY + 1) + kCanyonMinY;
    const double z = sourceChunkZ * 16 + static_cast<double>(random.nextInt(16));
    const float horizontalRotation = random.nextFloat() * kTwoPi;
    const float verticalRotation = uniformFloat(random, -0.125F, 0.125F);
    const float thickness = trapezoidFloat(random, 0.0F, 6.0F, 2.0F);
    const auto distance =
        static_cast<std::int32_t>(static_cast<float>(kMaxDistance) * uniformFloat(random, 0.75F, 1.0F));
    createCanyonTunnel(scope, random.nextLong(), x, static_cast<double>(y), z, thickness, horizontalRotation,
                       verticalRotation, distance);
}

} // namespace

void carve(Chunk& chunk, std::int64_t seed, OverworldAquifer& aquifer,
           const surface::SurfaceRuleEnvironment& environment, const surface::RuleSource& surfaceRule) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::legacy(0LL));
    std::vector<bool> mask(static_cast<std::size_t>(kHeight) * 16 * 16, false);
    const CarveScope scope{chunk, aquifer, environment, surfaceRule, mask};

    for (std::int32_t dx = -kSourceRange; dx <= kSourceRange; ++dx) {
        for (std::int32_t dz = -kSourceRange; dz <= kSourceRange; ++dz) {
            const std::int32_t sourceChunkX = chunk.chunkX() + dx;
            const std::int32_t sourceChunkZ = chunk.chunkZ() + dz;
            for (std::size_t index = 0; index < kCaveConfigs.size(); ++index) {
                random::setLargeFeatureSeed(random, seed + static_cast<std::int64_t>(index), sourceChunkX,
                                            sourceChunkZ);
                if (random.nextFloat() <= kCaveConfigs[index].probability) {
                    carveCaveSource(scope, random, sourceChunkX, sourceChunkZ, kCaveConfigs[index]);
                }
            }
            random::setLargeFeatureSeed(random, seed + static_cast<std::int64_t>(kCaveConfigs.size()), sourceChunkX,
                                        sourceChunkZ);
            if (random.nextFloat() <= kCanyonProbability) {
                carveCanyonSource(scope, random, sourceChunkX, sourceChunkZ);
            }
        }
    }
}

} // namespace basalt::server::world::terrain::carver
