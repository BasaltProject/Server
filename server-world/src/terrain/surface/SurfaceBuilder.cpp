#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/BiomeTemperature.hpp>
#include <basalt/server/world/terrain/surface/SurfaceBuilder.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

namespace basalt::server::world::terrain::surface {
namespace {

constexpr std::int32_t kSeaLevel = 63;

// Far enough below any real Y that a stone-below depth computed from it reads as "solid all the way
// down, no ceiling found". Vanilla's own marker plays the same role and is never read directly.
constexpr std::int32_t kWayBelowMinY = std::numeric_limits<std::int32_t>::min() / 2;

[[nodiscard]] std::int32_t blockAt(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    if (y >= chunk.verticalBounds().minY() && y <= chunk.verticalBounds().maxY()) {
        return chunk.blockStateId(x, y, z);
    }
    return registry::blocks::air().stateId;
}

void setBlock(Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
    if (y >= chunk.verticalBounds().minY() && y <= chunk.verticalBounds().maxY()) {
        chunk.setBlockStateId(x, y, z, stateId);
    }
}

[[nodiscard]] bool isFluid(std::int32_t stateId) { return registry::blockFluids::hasFluid(stateId); }

[[nodiscard]] bool isStone(std::int32_t stateId) { return !registry::blocks::isAir(stateId) && !isFluid(stateId); }

void erodedBadlandsExtension(Chunk& chunk, const SurfaceRuleEnvironment& environment, std::int32_t localX,
                             std::int32_t localZ, std::int32_t blockX, std::int32_t blockZ, std::int32_t height,
                             std::int32_t defaultBlockStateId) {
    const double pillarBuffer =
        std::min(std::abs(environment.sample2dScaled("badlands_surface", static_cast<double>(blockX),
                                                     static_cast<double>(blockZ)) *
                          8.25),
                 environment.sample2dScaled("badlands_pillar", blockX * 0.2, blockZ * 0.2) * 15.0);
    if (pillarBuffer <= 0.0) {
        return;
    }
    const double pillarFloor =
        std::abs(environment.sample2dScaled("badlands_pillar_roof", blockX * 0.75, blockZ * 0.75) * 1.5);
    const double extensionTop =
        64.0 + std::min(pillarBuffer * pillarBuffer * 2.5, std::ceil(pillarFloor * 50.0) + 24.0);
    const auto startY = static_cast<std::int32_t>(std::floor(extensionTop));
    if (height > startY) {
        return;
    }

    for (std::int32_t y = startY; y >= chunk.verticalBounds().minY(); --y) {
        const std::int32_t stateId = blockAt(chunk, localX, y, localZ);
        if (stateId == defaultBlockStateId) {
            break;
        }
        if (stateId == registry::blocks::water().stateId) {
            return;
        }
    }
    std::int32_t y = startY;
    while (y >= chunk.verticalBounds().minY() && registry::blocks::isAir(blockAt(chunk, localX, y, localZ))) {
        setBlock(chunk, localX, y, localZ, defaultBlockStateId);
        --y;
    }
}

void frozenOceanExtension(Chunk& chunk, const SurfaceRuleEnvironment& environment, std::int32_t localX,
                          std::int32_t localZ, std::int32_t blockX, std::int32_t blockZ, std::int32_t height,
                          const std::string& surfaceBiome, std::int32_t minSurfaceLevel) {
    const double iceberg = std::min(std::abs(environment.sample2dScaled("iceberg_surface", static_cast<double>(blockX),
                                                                        static_cast<double>(blockZ)) *
                                             8.25),
                                    environment.sample2dScaled("iceberg_pillar", blockX * 1.28, blockZ * 1.28) * 15.0);
    if (iceberg <= 1.8) {
        return;
    }

    const double icebergRoof =
        std::abs(environment.sample2dScaled("iceberg_pillar_roof", blockX * 1.17, blockZ * 1.17) * 1.5);
    double top = std::min(iceberg * iceberg * 1.2, std::ceil(icebergRoof * 40.0) + 14.0);
    if (biomeTemperature::shouldMeltFrozenOceanIcebergSlightly(surfaceBiome, blockX, kSeaLevel, blockZ)) {
        top -= 2.0;
    }

    double extensionBottom = 0.0;
    if (top > 2.0) {
        extensionBottom = kSeaLevel - top - 7.0;
        top += kSeaLevel;
    } else {
        top = 0.0;
    }

    random::RandomSource random = environment.positionalRandom(blockX, 0, blockZ);
    const std::int32_t maxSnowDepth = 2 + random.nextInt(4);
    const std::int32_t minSnowHeight = kSeaLevel + 18 + random.nextInt(10);
    std::int32_t snowDepth = 0;
    for (std::int32_t y = std::max(height, static_cast<std::int32_t>(top) + 1); y >= minSurfaceLevel; --y) {
        const std::int32_t stateId = blockAt(chunk, localX, y, localZ);
        const bool fillAir =
            registry::blocks::isAir(stateId) && y < static_cast<std::int32_t>(top) && random.nextDouble() > 0.01;
        const bool fillWater = stateId == registry::blocks::water().stateId &&
                               y > static_cast<std::int32_t>(extensionBottom) && y < kSeaLevel &&
                               extensionBottom != 0.0 && random.nextDouble() > 0.15;
        if (fillAir || fillWater) {
            if (snowDepth <= maxSnowDepth && y > minSnowHeight) {
                setBlock(chunk, localX, y, localZ, registry::blocks::named("snow_block")->stateId);
                ++snowDepth;
            } else {
                setBlock(chunk, localX, y, localZ, registry::blocks::named("packed_ice")->stateId);
            }
        }
    }
}

} // namespace

std::optional<std::int32_t> surfaceBuilder::topMaterial(const SurfaceRuleEnvironment& environment,
                                                        const RuleSource& ruleSource, std::int32_t x, std::int32_t y,
                                                        std::int32_t z, bool underFluid, std::int32_t generationMinY,
                                                        std::int32_t generationHeight) {
    SurfaceRuleContext context{environment, WorldGenerationContext{generationMinY, generationHeight}};
    const SurfaceRule rule = ruleSource(context);
    context.updateXZ(x, z);
    context.updateY(1, 1, underFluid ? y + 1 : std::numeric_limits<std::int32_t>::min(), y);
    return rule(x, y, z);
}

void surfaceBuilder::buildSurface(Chunk& chunk, const SurfaceRuleEnvironment& environment, const RuleSource& ruleSource,
                                  std::optional<std::int32_t> defaultBlockStateId,
                                  std::optional<std::int32_t> generationMinY,
                                  std::optional<std::int32_t> generationHeight) {
    const std::int32_t defaultStateId = defaultBlockStateId.value_or(registry::blocks::stone().stateId);
    // parity: WorldGenerationContext intersects the dimension height with the generator's noise
    // settings. Nether chunks are 256 blocks tall but generate only 128.
    const WorldGenerationContext worldGenContext{generationMinY.value_or(chunk.verticalBounds().minY()),
                                                 generationHeight.value_or(chunk.verticalBounds().height())};
    SurfaceRuleContext context{environment, worldGenContext};
    const SurfaceRule rule = ruleSource(context);
    const std::int32_t minBlockX = chunk.chunkX() * 16;
    const std::int32_t minBlockZ = chunk.chunkZ() * 16;
    const std::int32_t endY = chunk.verticalBounds().minY();

    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            const std::int32_t blockX = minBlockX + x;
            const std::int32_t blockZ = minBlockZ + z;
            // parity: the jar asks for `WORLD_SURFACE_WG` and adds one, and its heightmap answers with
            // the topmost matching block. Ours answers with the free space over it, which is already
            // that same number.
            const std::int32_t startingHeight = chunk.heightAt(x, z, HeightmapType::WorldSurface);
            const std::string surfaceBiome = environment.biomeAt(blockX, startingHeight, blockZ);
            if (surfaceBiome == "minecraft:eroded_badlands") {
                erodedBadlandsExtension(chunk, environment, x, z, blockX, blockZ, startingHeight, defaultStateId);
            }
            const std::int32_t height = chunk.heightAt(x, z, HeightmapType::WorldSurface);
            context.updateXZ(blockX, blockZ);

            std::int32_t stoneAboveDepth = 0;
            std::int32_t waterHeight = std::numeric_limits<std::int32_t>::min();
            std::int32_t nextCeilingStoneY = std::numeric_limits<std::int32_t>::max();

            for (std::int32_t y = height; y >= endY; --y) {
                const std::int32_t old = blockAt(chunk, x, y, z);
                if (registry::blocks::isAir(old)) {
                    stoneAboveDepth = 0;
                    waterHeight = std::numeric_limits<std::int32_t>::min();
                } else if (isFluid(old)) {
                    if (waterHeight == std::numeric_limits<std::int32_t>::min()) {
                        waterHeight = y + 1;
                    }
                } else {
                    if (nextCeilingStoneY >= y) {
                        nextCeilingStoneY = kWayBelowMinY;
                        for (std::int32_t lookaheadY = y - 1; lookaheadY >= endY - 1; --lookaheadY) {
                            if (!isStone(blockAt(chunk, x, lookaheadY, z))) {
                                nextCeilingStoneY = lookaheadY + 1;
                                break;
                            }
                        }
                    }
                    ++stoneAboveDepth;
                    const std::int32_t stoneBelowDepth = y - nextCeilingStoneY + 1;
                    context.updateY(stoneAboveDepth, stoneBelowDepth, waterHeight, y);
                    if (old == defaultStateId) {
                        if (const std::optional<std::int32_t> replacement = rule(blockX, y, blockZ);
                            replacement.has_value()) {
                            chunk.setBlockStateId(x, y, z, *replacement);
                        }
                    }
                }
            }

            if (surfaceBiome == "minecraft:frozen_ocean" || surfaceBiome == "minecraft:deep_frozen_ocean") {
                frozenOceanExtension(chunk, environment, x, z, blockX, blockZ, startingHeight, surfaceBiome,
                                     context.minSurfaceLevel());
            }
        }
    }
}

} // namespace basalt::server::world::terrain::surface
