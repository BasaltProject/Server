#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/densityfunction/RandomState.hpp>
#include <basalt/server/world/densityfunction/SamplerContext.hpp>
#include <basalt/server/world/noise/JavaMath.hpp>
#include <basalt/server/world/noise/Noise.hpp>
#include <basalt/server/world/terrain/BiomeTemperature.hpp>
#include <basalt/server/world/terrain/material/MaterialRules.hpp>

#include <cmath>
#include <limits>

namespace basalt::server::world::terrain::material {
namespace {

namespace jm = noise::javaMath;
using densityfunction::DensityBuffer;
using densityfunction::DensityVolume;

/// parity: `DimensionType.WAY_BELOW_MIN_Y`.
constexpr std::int32_t kWayBelowMinY = -32'512;
constexpr std::int32_t kHowFarBelowPreliminarySurface = 8;

[[nodiscard]] std::int32_t stateNamed(std::string_view name) {
    const registry::BlockState* found = registry::blocks::named(name);
    return found == nullptr ? registry::blocks::air().stateId : found->stateId;
}

[[nodiscard]] bool isFluid(std::int32_t stateId) { return registry::blockFluids::hasFluid(stateId); }

[[nodiscard]] std::int32_t blockAt(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    if (y >= chunk.verticalBounds().minY() && y <= chunk.verticalBounds().maxY()) {
        return chunk.blockStateId(x, y, z);
    }
    return registry::blocks::air().stateId;
}

/// parity: `ChunkAccess.getHeight(WORLD_SURFACE_WG, x, z)`: the topmost non-air block.
[[nodiscard]] std::int32_t worldSurface(const Chunk& chunk, std::int32_t x, std::int32_t z) {
    return chunk.heightAt(x, z, HeightmapType::WorldSurface) - 1;
}

/// parity: `getHighestFilledSectionIndex`, as the top block of that section.
[[nodiscard]] std::int32_t highestFilledTop(const Chunk& chunk) {
    for (std::size_t index = chunk.sectionCount(); index-- > 0;) {
        if (chunk.section(index).nonAirBlockCount() > 0) {
            return chunk.verticalBounds().minY() + static_cast<std::int32_t>(index) * 16 + 15;
        }
    }
    return chunk.verticalBounds().minY() - 1;
}

[[nodiscard]] std::int32_t gradientX(const Chunk& chunk, std::int32_t x, std::int32_t z) {
    return worldSurface(chunk, std::min(x + 1, 15), z) - worldSurface(chunk, std::max(x - 1, 0), z);
}

[[nodiscard]] std::int32_t gradientZ(const Chunk& chunk, std::int32_t x, std::int32_t z) {
    return worldSurface(chunk, x, std::min(z + 1, 15)) - worldSurface(chunk, x, std::max(z - 1, 0));
}

void makeBands(random::RandomSource& random, std::array<std::int32_t, 192>& bands, std::int32_t baseWidth,
               std::int32_t state) {
    const std::int32_t count = random.nextIntBetweenInclusive(6, 15);
    for (std::int32_t i = 0; i < count; ++i) {
        const std::int32_t width = baseWidth + random.nextInt(3);
        const std::int32_t start = random.nextInt(static_cast<std::int32_t>(bands.size()));
        for (std::int32_t p = 0; start + p < static_cast<std::int32_t>(bands.size()) && p < width; ++p) {
            bands[static_cast<std::size_t>(start + p)] = state;
        }
    }
}

/// parity: `MaterialSystem.generateBands`.
[[nodiscard]] std::array<std::int32_t, 192> generateBands(random::RandomSource& random) {
    std::array<std::int32_t, 192> bands{};
    bands.fill(stateNamed("terracotta"));
    const auto size = static_cast<std::int32_t>(bands.size());
    for (std::int32_t i = 0; i < size; ++i) {
        i += random.nextInt(5) + 1;
        if (i < size) {
            bands[static_cast<std::size_t>(i)] = stateNamed("orange_terracotta");
        }
    }
    makeBands(random, bands, 1, stateNamed("yellow_terracotta"));
    makeBands(random, bands, 2, stateNamed("brown_terracotta"));
    makeBands(random, bands, 1, stateNamed("red_terracotta"));
    const std::int32_t whiteCount = random.nextIntBetweenInclusive(9, 15);
    std::int32_t placed = 0;
    for (std::int32_t start = 0; placed < whiteCount && start < size; start += random.nextInt(16) + 4) {
        bands[static_cast<std::size_t>(start)] = stateNamed("white_terracotta");
        if (start - 1 > 0 && random.nextBoolean()) {
            bands[static_cast<std::size_t>(start - 1)] = stateNamed("light_gray_terracotta");
        }
        if (start + 1 < size && random.nextBoolean()) {
            bands[static_cast<std::size_t>(start + 1)] = stateNamed("light_gray_terracotta");
        }
        ++placed;
    }
    return bands;
}

} // namespace

MaterialRuleContext::MaterialRuleContext(MaterialSystem& system, densityfunction::RandomState& state,
                                         const DensityVolume& expectedVolume, densityfunction::SamplerContext& samplers,
                                         BiomeGetter biomes, std::int32_t minGenY, std::int32_t genDepth)
    : system_(&system), state_(&state), expectedVolume_(expectedVolume), samplers_(&samplers),
      biomes_(std::move(biomes)), minGenY_(minGenY), genDepth_(genDepth),
      preliminaryVolume_{expectedVolume.sizeX,     1, expectedVolume.sizeZ,
                         expectedVolume.minBlockX, 0, expectedVolume.minBlockZ},
      lastSecondaryUpdate_(lastUpdateXZ_ - 1), lastMinSurfaceUpdate_(lastUpdateXZ_ - 1) {}

void MaterialRuleContext::updateXZ(std::int32_t x, std::int32_t z, std::int32_t gradientX, std::int32_t gradientZ) {
    ++lastUpdateXZ_;
    ++lastUpdateY_;
    blockX_ = x;
    blockZ_ = z;
    gradientX_ = gradientX;
    gradientZ_ = gradientZ;
    surfaceDepth_ = system_->surfaceDepth(x, z);
}

void MaterialRuleContext::updateY(std::int32_t stoneDepthAbove, std::int32_t stoneDepthBelow, std::int32_t waterHeight,
                                  std::int32_t y) {
    ++lastUpdateY_;
    biome_ = nullptr;
    blockY_ = y;
    waterHeight_ = waterHeight;
    stoneDepthBelow_ = stoneDepthBelow;
    stoneDepthAbove_ = stoneDepthAbove;
}

double MaterialRuleContext::surfaceSecondary() {
    if (lastSecondaryUpdate_ != lastUpdateXZ_) {
        lastSecondaryUpdate_ = lastUpdateXZ_;
        surfaceSecondary_ = system_->surfaceSecondary(blockX_, blockZ_);
    }
    return surfaceSecondary_;
}

const std::string& MaterialRuleContext::biome() {
    if (biome_ == nullptr) {
        biome_ = &biomes_(blockX_, blockY_, blockZ_);
    }
    return *biome_;
}

std::int32_t MaterialRuleContext::minSurfaceLevel() {
    if (lastMinSurfaceUpdate_ != lastUpdateXZ_) {
        lastMinSurfaceUpdate_ = lastUpdateXZ_;
        const std::int32_t index = preliminaryVolume_.indexOfBlock(blockX_, 0, blockZ_);
        float level = 0.0F;
        if (index != DensityVolume::kNoBlock) {
            if (!preliminaryFilled_) {
                preliminaryFilled_ = true;
                preliminaryBuffer_.assign(static_cast<std::size_t>(preliminaryVolume_.size()), 0.0F);
                DensityBuffer buffer{std::span<float>{preliminaryBuffer_}};
                state_->sampler(system_->chunkSurfaceLevel())->sampleVolume(*samplers_, buffer, preliminaryVolume_);
            }
            level = preliminaryBuffer_[static_cast<std::size_t>(index)];
        } else {
            level = state_->sampler(system_->chunkSurfaceLevel())->sampleValue(*samplers_, blockX_, 0, blockZ_);
        }
        minSurfaceLevel_ = jm::floor(level) + surfaceDepth_ - kHowFarBelowPreliminarySurface;
    }
    return minSurfaceLevel_;
}

std::function<double()> MaterialRuleContext::noiseSampler(const std::string& noiseId, bool is3d) {
    // parity: one sampler per noise and dimensionality, remembering its last column or block.
    struct Cached {
        std::shared_ptr<const noise::Noise> noise;
        std::int64_t stamp;
        double value = 0.0;
    };

    auto cached = std::make_shared<Cached>(Cached{state_->noise(noiseId), (is3d ? lastUpdateY_ : lastUpdateXZ_) - 1});
    return [this, cached, is3d]() {
        const std::int64_t stamp = is3d ? lastUpdateY_ : lastUpdateXZ_;
        if (cached->stamp != stamp) {
            cached->value = static_cast<double>(
                is3d ? cached->noise->get(static_cast<double>(blockX_), static_cast<double>(blockY_),
                                          static_cast<double>(blockZ_))
                     : cached->noise->get(static_cast<double>(blockX_), 0.0, static_cast<double>(blockZ_)));
            cached->stamp = stamp;
        }
        return cached->value;
    };
}

std::function<float()> MaterialRuleContext::densities(const densityfunction::Function& function, bool prefill) {
    densityfunction::Sampler sampler = state_->sampler(function);
    if (!prefill) {
        return [this, sampler]() { return sampler->sampleValue(*samplers_, blockX_, blockY_, blockZ_); };
    }
    auto values = std::make_shared<std::vector<float>>(static_cast<std::size_t>(expectedVolume_.size()));
    DensityBuffer buffer{std::span<float>{*values}};
    sampler->sampleVolume(*samplers_, buffer, expectedVolume_);
    return [this, sampler, values]() {
        const std::int32_t index = expectedVolume_.indexOfBlock(blockX_, blockY_, blockZ_);
        return index == DensityVolume::kNoBlock ? sampler->sampleValue(*samplers_, blockX_, blockY_, blockZ_)
                                                : (*values)[static_cast<std::size_t>(index)];
    };
}

random::PositionalRandomFactory MaterialRuleContext::randomFactory(std::string_view name) const {
    return state_->randomFactory(name);
}

std::int32_t MaterialRuleContext::seaLevel() const noexcept { return system_->seaLevel(); }

std::int32_t MaterialRuleContext::band(std::int32_t x, std::int32_t y, std::int32_t z) const {
    return system_->band(x, y, z);
}

MaterialSystem::MaterialSystem(densityfunction::RandomState& state, std::int32_t defaultBlock, std::int32_t seaLevel,
                               densityfunction::Function chunkSurfaceLevel)
    : state_(&state), defaultBlock_(defaultBlock), seaLevel_(seaLevel),
      chunkSurfaceLevel_(std::move(chunkSurfaceLevel)) {
    clayBandsOffset_ = state.noise("minecraft:clay_bands_offset");
    random::RandomSource bands = state.random().fromHashOf("minecraft:clay_bands");
    clayBands_ = generateBands(bands);
    surface_ = state.noise("minecraft:surface");
    surfaceSecondary_ = state.noise("minecraft:surface_secondary");
    badlandsPillar_ = state.noise("minecraft:badlands_pillar");
    badlandsPillarRoof_ = state.noise("minecraft:badlands_pillar_roof");
    badlandsSurface_ = state.noise("minecraft:badlands_surface");
    icebergPillar_ = state.noise("minecraft:iceberg_pillar");
    icebergPillarRoof_ = state.noise("minecraft:iceberg_pillar_roof");
    icebergSurface_ = state.noise("minecraft:iceberg_surface");
}

std::int32_t MaterialSystem::surfaceDepth(std::int32_t x, std::int32_t z) const {
    const auto noise = static_cast<double>(surface_->get(static_cast<double>(x), 0.0, static_cast<double>(z)));
    return jm::toInt(noise * 2.75 + 3.0 + state_->random().at(x, 0, z).nextDouble() * 0.25);
}

double MaterialSystem::surfaceSecondary(std::int32_t x, std::int32_t z) const {
    return static_cast<double>(surfaceSecondary_->get(static_cast<double>(x), 0.0, static_cast<double>(z)));
}

std::int32_t MaterialSystem::band(std::int32_t x, std::int32_t y, std::int32_t z) const {
    // parity: `Math.round(float)`, half up.
    const float scaled = clayBandsOffset_->get(static_cast<double>(x), 0.0, static_cast<double>(z)) * 4.0F;
    const auto offset = static_cast<std::int32_t>(std::floor(static_cast<double>(scaled) + 0.5));
    const auto size = static_cast<std::int32_t>(clayBands_.size());
    return clayBands_[static_cast<std::size_t>((y + offset + size) % size)];
}

void MaterialSystem::buildSurface(Chunk& chunk, densityfunction::SamplerContext& samplers, const DensityVolume& volume,
                                  const MaterialRule& rule, const BiomeGetter& biomes, std::int32_t minGenY,
                                  std::int32_t genDepth) {
    const std::int32_t minY = chunk.verticalBounds().minY();
    const std::int32_t maxY = chunk.verticalBounds().maxY();
    // parity: the volume is narrowed to the highest section that holds anything.
    const std::int32_t maxBlockY = highestFilledTop(chunk);
    const DensityVolume narrowed{volume.sizeX,     std::max(maxBlockY - volume.minBlockY + 1, 1),
                                 volume.sizeZ,     volume.minBlockX,
                                 volume.minBlockY, volume.minBlockZ};
    MaterialRuleContext context{*this, *state_, narrowed, samplers, biomes, minGenY, genDepth};
    const RuleEvaluator evaluate = rule.compile(context);
    const std::int32_t minBlockX = chunk.chunkX() * 16;
    const std::int32_t minBlockZ = chunk.chunkZ() * 16;
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            const std::int32_t blockX = minBlockX + x;
            const std::int32_t blockZ = minBlockZ + z;
            const std::int32_t startingHeight = worldSurface(chunk, x, z) + 1;
            const std::string& surfaceBiome = biomes(blockX, startingHeight, blockZ);
            if (surfaceBiome == "minecraft:eroded_badlands") {
                erodedBadlandsExtension(chunk, x, z, blockX, blockZ, startingHeight);
            }
            const std::int32_t height = worldSurface(chunk, x, z) + 1;
            context.updateXZ(blockX, blockZ, gradientX(chunk, x, z), gradientZ(chunk, x, z));
            std::int32_t stoneAboveDepth = 0;
            std::int32_t waterHeight = std::numeric_limits<std::int32_t>::min();
            std::int32_t nextCeilingStoneY = std::numeric_limits<std::int32_t>::max();
            for (std::int32_t y = height; y >= minY; --y) {
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
                        for (std::int32_t lookahead = y - 1; lookahead >= minY - 1; --lookahead) {
                            const std::int32_t next = blockAt(chunk, x, lookahead, z);
                            if (registry::blocks::isAir(next) || isFluid(next)) {
                                nextCeilingStoneY = lookahead + 1;
                                break;
                            }
                        }
                    }
                    ++stoneAboveDepth;
                    context.updateY(stoneAboveDepth, y - nextCeilingStoneY + 1, waterHeight, y);
                    if (y >= minY && y <= maxY) {
                        if (const std::optional<std::int32_t> state = evaluate(blockX, y, blockZ); state.has_value()) {
                            chunk.setBlockStateId(x, y, z, *state);
                            if (isFluid(*state)) {
                                chunk.markFluidUpdate(blockX, y, blockZ);
                            }
                        }
                    }
                }
            }
            if (surfaceBiome == "minecraft:frozen_ocean" || surfaceBiome == "minecraft:deep_frozen_ocean") {
                frozenOceanExtension(chunk, context.minSurfaceLevel(), surfaceBiome, x, z, blockX, blockZ,
                                     startingHeight);
            }
        }
    }
}

std::optional<std::int32_t> MaterialSystem::topMaterial(const MaterialRule& rule, const Chunk& chunk,
                                                        densityfunction::SamplerContext& samplers,
                                                        const BiomeGetter& biomes, std::int32_t x, std::int32_t y,
                                                        std::int32_t z, bool underFluid, std::int32_t minGenY,
                                                        std::int32_t genDepth) {
    const DensityVolume volume{1, 1, 1, x, y, z};
    MaterialRuleContext context{*this, *state_, volume, samplers, biomes, minGenY, genDepth};
    const RuleEvaluator evaluate = rule.compile(context);
    context.updateXZ(x, z, gradientX(chunk, x & 15, z & 15), gradientZ(chunk, x & 15, z & 15));
    context.updateY(1, 1, underFluid ? y + 1 : std::numeric_limits<std::int32_t>::min(), y);
    return evaluate(x, y, z);
}

void MaterialSystem::erodedBadlandsExtension(Chunk& chunk, std::int32_t localX, std::int32_t localZ,
                                             std::int32_t blockX, std::int32_t blockZ, std::int32_t height) const {
    const double surface =
        static_cast<double>(badlandsSurface_->get(static_cast<double>(blockX), 0.0, static_cast<double>(blockZ)));
    const float pillar =
        badlandsPillar_->get(static_cast<double>(blockX) * 0.2, 0.0, static_cast<double>(blockZ) * 0.2);
    const double pillarBuffer = std::min(std::fabs(surface * 8.25), static_cast<double>(pillar * 15.0F));
    if (pillarBuffer <= 0.0) {
        return;
    }
    const double pillarFloor =
        std::fabs(static_cast<double>(badlandsPillarRoof_->get(static_cast<double>(blockX) * 0.75, 0.0,
                                                               static_cast<double>(blockZ) * 0.75)) *
                  1.5);
    const double extensionTop =
        64.0 + std::min(pillarBuffer * pillarBuffer * 2.5, std::ceil(pillarFloor * 50.0) + 24.0);
    const std::int32_t startY = jm::floor(extensionTop);
    if (height > startY) {
        return;
    }
    const std::int32_t minY = chunk.verticalBounds().minY();
    for (std::int32_t y = startY; y >= minY; --y) {
        const std::int32_t old = blockAt(chunk, localX, y, localZ);
        // parity: a block match, which for the single-state default blocks is a state match.
        if (old == defaultBlock_) {
            break;
        }
        if (old == registry::blocks::water().stateId) {
            return;
        }
    }
    for (std::int32_t y = startY; y >= minY && registry::blocks::isAir(blockAt(chunk, localX, y, localZ)); --y) {
        if (y <= chunk.verticalBounds().maxY()) {
            chunk.setBlockStateId(localX, y, localZ, defaultBlock_);
        }
    }
}

void MaterialSystem::frozenOceanExtension(Chunk& chunk, std::int32_t minSurfaceLevel, const std::string& biome,
                                          std::int32_t localX, std::int32_t localZ, std::int32_t blockX,
                                          std::int32_t blockZ, std::int32_t height) const {
    const double surface =
        static_cast<double>(icebergSurface_->get(static_cast<double>(blockX), 0.0, static_cast<double>(blockZ)));
    const float pillar =
        icebergPillar_->get(static_cast<double>(blockX) * 1.28, 0.0, static_cast<double>(blockZ) * 1.28);
    const double iceberg = std::min(std::fabs(surface * 8.25), static_cast<double>(pillar * 15.0F));
    if (iceberg <= 1.8) {
        return;
    }
    const double roof = std::fabs(static_cast<double>(icebergPillarRoof_->get(static_cast<double>(blockX) * 1.17, 0.0,
                                                                              static_cast<double>(blockZ) * 1.17)) *
                                  1.5);
    double top = std::min(iceberg * iceberg * 1.2, std::ceil(roof * 40.0) + 14.0);
    if (biomeTemperature::shouldMeltFrozenOceanIcebergSlightly(biome, blockX, seaLevel_, blockZ)) {
        top -= 2.0;
    }
    if (top <= 2.0) {
        return;
    }
    const double extensionBottom = static_cast<double>(seaLevel_) - top - 7.0;
    top += static_cast<double>(seaLevel_);
    const double extensionTop = top;
    random::RandomSource random = state_->random().at(blockX, 0, blockZ);
    const std::int32_t maxSnowDepth = 2 + random.nextInt(4);
    const std::int32_t minSnowHeight = seaLevel_ + 18 + random.nextInt(10);
    std::int32_t snowDepth = 0;
    const std::int32_t snow = stateNamed("snow_block");
    const std::int32_t packedIce = stateNamed("packed_ice");
    for (std::int32_t y = std::max(height, jm::toInt(top) + 1); y >= minSurfaceLevel; --y) {
        const std::int32_t old = blockAt(chunk, localX, y, localZ);
        const bool air = registry::blocks::isAir(old) && y < jm::toInt(extensionTop) && random.nextDouble() > 0.01;
        const bool water = !air && old == registry::blocks::water().stateId && y > jm::toInt(extensionBottom) &&
                           y < seaLevel_ && random.nextDouble() > 0.15;
        if (air || water) {
            const std::int32_t state = snowDepth <= maxSnowDepth && y > minSnowHeight ? snow : packedIce;
            if (state == snow) {
                ++snowDepth;
            }
            if (y >= chunk.verticalBounds().minY() && y <= chunk.verticalBounds().maxY()) {
                chunk.setBlockStateId(localX, y, localZ, state);
            }
        }
    }
}

} // namespace basalt::server::world::terrain::material
