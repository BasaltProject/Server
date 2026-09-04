#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/GenerationMetrics.hpp>
#include <basalt/server/world/terrain/DensityChunkSampler.hpp>
#include <basalt/server/world/terrain/DimensionTerrainGenerator.hpp>
#include <basalt/server/world/terrain/carver/NetherCaveCarver.hpp>
#include <basalt/server/world/terrain/decoration/end/EndDecoration.hpp>
#include <basalt/server/world/terrain/decoration/nether/NetherLocalDecoration.hpp>
#include <basalt/server/world/terrain/decoration/nether/NetherSurfaceStructuresDecoration.hpp>
#include <basalt/server/world/terrain/decoration/nether/NetherUndergroundDecoration.hpp>
#include <basalt/server/world/terrain/decoration/nether/NetherVegetalDecoration.hpp>
#include <basalt/server/world/terrain/surface/ChunkBiomeEnvironment.hpp>
#include <basalt/server/world/terrain/surface/SurfaceBuilder.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleData.hpp>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>

namespace basalt::server::world::terrain::dimensionTerrainGenerator {
namespace {

/// Everything one dimension's noise settings decide about the base shape.
struct Settings {
    ChunkHeight verticalBounds = kNetherHeight;
    std::int32_t noiseMinY = 0;
    std::int32_t noiseHeight = 128;
    std::int32_t cellWidth = 4;
    std::int32_t cellHeight = 8;
    std::int32_t defaultBlock = 0;
    std::int32_t defaultFluid = 0;
    std::int32_t fluidLevel = 0;
};

using ChunkPass = std::function<void(Chunk&, densityfunction::DensityCache&)>;
using BiomeAt =
    std::function<std::string_view(std::int32_t, std::int32_t, std::int32_t, densityfunction::DensityCache&)>;

[[nodiscard]] Chunk generate(const densityfunction::Function& finalDensity, const Settings& settings,
                             std::int32_t chunkX, std::int32_t chunkZ, const ChunkPass& surfacePass,
                             const ChunkPass& carverPass, const ChunkPass& structurePass,
                             const ChunkPass& decorationPass, const BiomeAt& biomeAt) {
    Chunk chunk{chunkX, chunkZ, settings.verticalBounds};
    densityfunction::DensityCache cache{chunkX, chunkZ, densityfunction::markerSlotCount()};
    DensityChunkSampler sampler{
        finalDensity,        chunkX, chunkZ, settings.noiseMinY, settings.noiseHeight, settings.cellWidth,
        settings.cellHeight, &cache};
    // parity: `ChunkStatus.BIOMES` runs before `NOISE` and `SURFACE`, so the biome container is filled
    // before any terrain exists. The order is not cosmetic: every climate lookup seeds the next one
    // through `Climate.RTree`'s per-thread last result, so filling last shifts the ties the surface
    // rules and the decorators go on to resolve.
    generationMetrics::measure(generationMetrics::Phase::BiomeFill, [&] {
        std::map<std::string, std::int32_t, std::less<>> idByName;
        // parity: `ChunkAccess.fillBiomesFromNoise` walks section by section, and
        // `LevelChunkSection.fillBiomesFromNoise` walks x, then y, then z inside each one. The order is
        // observable because `Climate.RTree` seeds each search with the thread's previous result and keeps
        // it on an exact tie, so a column that sits on a parameter boundary takes the biome of whatever
        // was asked for just before it.
        const std::int32_t sections = settings.verticalBounds.height() / 16;
        for (std::int32_t section = 0; section < sections; ++section) {
            for (std::int32_t quartX = 0; quartX < 4; ++quartX) {
                for (std::int32_t withinSection = 0; withinSection < 4; ++withinSection) {
                    const std::int32_t quartY = section * 4 + withinSection;
                    const std::int32_t blockY = settings.verticalBounds.minY() + quartY * 4;
                    for (std::int32_t quartZ = 0; quartZ < 4; ++quartZ) {
                        const std::string_view biome =
                            biomeAt(chunkX * 16 + quartX * 4, blockY, chunkZ * 16 + quartZ * 4, cache);
                        auto found = idByName.find(biome);
                        if (found == idByName.end()) {
                            std::int32_t id = 0;
                            const std::expected<wire::common::Identifier, wire::common::IdentifierError> parsed =
                                wire::common::Identifier::parse(biome);
                            if (parsed.has_value()) {
                                id = registry::biomes::registry().idOf(*parsed).value_or(0);
                            }
                            found = idByName.emplace(std::string{biome}, id).first;
                        }
                        chunk.setBiomeId(quartX, quartY, quartZ, found->second);
                    }
                }
            }
        }
    });
    generationMetrics::measure(generationMetrics::Phase::TerrainFill, [&] {
        for (std::int32_t cellY = 0; cellY < sampler.verticalCells(); ++cellY) {
            const std::int32_t cellMinY = settings.noiseMinY + cellY * settings.cellHeight;
            const std::int32_t cellMaxY = cellMinY + settings.cellHeight - 1;
            for (std::int32_t cellX = 0; cellX < sampler.horizontalCells(); ++cellX) {
                for (std::int32_t cellZ = 0; cellZ < sampler.horizontalCells(); ++cellZ) {
                    for (std::int32_t y = cellMinY; y <= cellMaxY; ++y) {
                        for (std::int32_t z = cellZ * settings.cellWidth; z < (cellZ + 1) * settings.cellWidth; ++z) {
                            for (std::int32_t x = cellX * settings.cellWidth; x < (cellX + 1) * settings.cellWidth;
                                 ++x) {
                                std::int32_t stateId = registry::blocks::air().stateId;
                                if (sampler.sample(x, y, z) > 0.0) {
                                    stateId = settings.defaultBlock;
                                } else if (y < settings.fluidLevel) {
                                    stateId = settings.defaultFluid;
                                }
                                if (!registry::blocks::isAir(stateId)) {
                                    chunk.setBlockStateId(x, y, z, stateId);
                                }
                            }
                        }
                    }
                }
            }
        }
    });
    generationMetrics::measure(generationMetrics::Phase::SurfaceRules, [&] { surfacePass(chunk, cache); });
    generationMetrics::measure(generationMetrics::Phase::Carvers, [&] { carverPass(chunk, cache); });
    generationMetrics::measure(generationMetrics::Phase::Structures, [&] { structurePass(chunk, cache); });
    generationMetrics::measure(generationMetrics::Phase::Decoration, [&] { decorationPass(chunk, cache); });
    if (generationMetrics::enabled()) {
        generationMetrics::countChunk();
    }
    return chunk;
}

[[nodiscard]] std::int32_t plainState(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    return found == nullptr ? registry::blocks::air().stateId : found->stateId;
}

} // namespace

Chunk nether(const NetherNoiseRouter& router, const NetherBiomeSource& biomeSource, std::int32_t chunkX,
             std::int32_t chunkZ, const surface::SurfaceRuleEnvironment* surfaceEnvironment, std::int64_t seed,
             NetherStructureGenerator* structureGenerator) {
    static const std::int32_t netherrack = plainState("netherrack");
    const Settings settings{.verticalBounds = kNetherHeight,
                            .noiseMinY = 0,
                            .noiseHeight = 128,
                            .cellWidth = 4,
                            .cellHeight = 8,
                            .defaultBlock = netherrack,
                            .defaultFluid = registry::blocks::lava().stateId,
                            .fluidLevel = 32};
    const densityfunction::Function density = structureGenerator == nullptr
                                                  ? router.finalDensity
                                                  : structureGenerator->finalDensityForChunk(chunkX, chunkZ, seed);
    return generate(
        density, settings, chunkX, chunkZ,
        [surfaceEnvironment](Chunk& chunk, densityfunction::DensityCache& cache) {
            if (surfaceEnvironment == nullptr) {
                return;
            }
            const surface::ChunkBiomeEnvironment chunkEnvironment{*surfaceEnvironment, &cache};
            surface::surfaceBuilder::buildSurface(chunk, chunkEnvironment, surface::surfaceRuleData::nether(),
                                                  netherrack, 0, 128);
        },
        [seed](Chunk& chunk, densityfunction::DensityCache&) { carver::carveNether(chunk, seed); },
        [structureGenerator, seed](Chunk& chunk, densityfunction::DensityCache&) {
            if (structureGenerator != nullptr) {
                structureGenerator->generateInto(chunk, seed);
            }
        },
        [&biomeSource, seed](Chunk& chunk, densityfunction::DensityCache& cache) {
            const NetherBiomeLookup biomeLookup = biomeSource.cached(cache);
            decoration::nether::netherLocalDecoration::decorate(chunk, seed, biomeLookup);
            decoration::nether::netherSurfaceStructuresDecoration::decorate(chunk, seed, biomeLookup);
            decoration::nether::netherUndergroundDecoration::decorate(chunk, seed, biomeLookup);
            decoration::nether::netherVegetalDecoration::decorate(chunk, seed, biomeLookup);
        },
        [&biomeSource](std::int32_t blockX, std::int32_t blockY, std::int32_t blockZ,
                       densityfunction::DensityCache& cache) {
            return biomeSource.biomeAtQuart(blockX >> 2, blockY >> 2, blockZ >> 2, &cache);
        });
}

Chunk end(const EndNoiseRouter& router, const TheEndBiomeSource& biomeSource, std::int32_t chunkX, std::int32_t chunkZ,
          std::int64_t seed, EndCityStructureGenerator* structureGenerator) {
    static const std::int32_t endStone = plainState("end_stone");
    const Settings settings{.verticalBounds = kEndHeight,
                            .noiseMinY = 0,
                            .noiseHeight = 128,
                            .cellWidth = 8,
                            .cellHeight = 4,
                            .defaultBlock = endStone,
                            .defaultFluid = registry::blocks::air().stateId,
                            .fluidLevel = 0};
    return generate(
        router.finalDensity, settings, chunkX, chunkZ, [](Chunk&, densityfunction::DensityCache&) {},
        [](Chunk&, densityfunction::DensityCache&) {},
        [structureGenerator, seed](Chunk& chunk, densityfunction::DensityCache&) {
            if (structureGenerator != nullptr) {
                structureGenerator->generateInto(chunk, seed);
            }
        },
        [&router, &biomeSource, seed](Chunk& chunk, densityfunction::DensityCache&) {
            decoration::end::endRawDecoration::decorate(chunk, seed, biomeSource);
            decoration::end::endSurfaceStructuresDecoration::decorate(chunk, seed, router, biomeSource);
            decoration::end::endVegetalDecoration::decorate(chunk, seed, router, biomeSource);
            decoration::end::endTopLayerDecoration::decorate(chunk, biomeSource);
        },
        [&biomeSource](std::int32_t blockX, std::int32_t blockY, std::int32_t blockZ, densityfunction::DensityCache&) {
            return biomeSource.biomeAtQuart(blockX >> 2, blockY >> 2, blockZ >> 2);
        });
}

} // namespace basalt::server::world::terrain::dimensionTerrainGenerator
