#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/GenerationMetrics.hpp>
#include <basalt/server/world/terrain/BubbleColumnFormation.hpp>
#include <basalt/server/world/terrain/NoiseChunk.hpp>
#include <basalt/server/world/terrain/RealTerrainGenerator.hpp>
#include <basalt/server/world/terrain/StructureBeardifier.hpp>
#include <basalt/server/world/terrain/carver/Carvers.hpp>
#include <basalt/server/world/terrain/decoration/CaveLocalModificationDecoration.hpp>
#include <basalt/server/world/terrain/decoration/CaveSpeleothemDecoration.hpp>
#include <basalt/server/world/terrain/decoration/DesertWellDecoration.hpp>
#include <basalt/server/world/terrain/decoration/ForestRockDecoration.hpp>
#include <basalt/server/world/terrain/decoration/FrozenOceanDecoration.hpp>
#include <basalt/server/world/terrain/decoration/IceSpikesDecoration.hpp>
#include <basalt/server/world/terrain/decoration/OverworldLakeDecoration.hpp>
#include <basalt/server/world/terrain/decoration/OverworldOreDecoration.hpp>
#include <basalt/server/world/terrain/decoration/OverworldSpringDecoration.hpp>
#include <basalt/server/world/terrain/decoration/OverworldUndergroundStructures.hpp>
#include <basalt/server/world/terrain/decoration/OverworldVegetalDecoration.hpp>
#include <basalt/server/world/terrain/decoration/SculkDecoration.hpp>
#include <basalt/server/world/terrain/decoration/SurfaceFreezingDecoration.hpp>
#include <basalt/server/world/terrain/decoration/tree/LeafDistances.hpp>
#include <basalt/server/world/terrain/material/MaterialRules.hpp>
#include <basalt/server/world/terrain/surface/ChunkBiomeEnvironment.hpp>

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <utility>

namespace basalt::server::world::terrain::realTerrainGenerator {
namespace {

/// Fills every quart cell of the chunk with its real biome network id, so the wire palette carries
/// the actual biome layout the client needs for grass, water and fog colours.
void fillBiomes(Chunk& chunk, const surface::SurfaceRuleEnvironment& environment,
                densityfunction::SamplerContext* context) {
    // A chunk holds a handful of distinct biomes, so the identifier parse and the registry lookup
    // are paid once per name instead of once per each of the 1536 quarts.
    std::map<std::string, std::int32_t, std::less<>> idByName;
    // parity: `ChunkAccess.fillBiomesFromNoise` goes section by section and
    // `LevelChunkSection.fillBiomesFromNoise` goes x, then y, then z inside each one. The order decides real
    // biomes: `Climate.RTree` seeds every search with the thread's previous leaf and keeps it when nothing is
    // strictly closer, so a column whose continentalness quantises onto a band edge takes the biome of the
    // cell asked for just before it.
    const std::int32_t sections = chunk.verticalBounds().height() / 16;
    for (std::int32_t section = 0; section < sections; ++section) {
        for (std::int32_t quartX = 0; quartX < 4; ++quartX) {
            for (std::int32_t withinSection = 0; withinSection < 4; ++withinSection) {
                const std::int32_t quartY = section * 4 + withinSection;
                const std::int32_t blockY = chunk.verticalBounds().minY() + quartY * 4;
                for (std::int32_t quartZ = 0; quartZ < 4; ++quartZ) {
                    const std::string& biome = environment.noiseBiomeAt(chunk.chunkX() * 4 + quartX, blockY >> 2,
                                                                        chunk.chunkZ() * 4 + quartZ, context);
                    auto found = idByName.find(biome);
                    if (found == idByName.end()) {
                        std::int32_t id = 0;
                        const std::expected<wire::common::Identifier, wire::common::IdentifierError> parsed =
                            wire::common::Identifier::parse(biome);
                        if (parsed.has_value()) {
                            id = registry::biomes::registry().idOf(*parsed).value_or(0);
                        }
                        found = idByName.emplace(biome, id).first;
                    }
                    chunk.setBiomeId(quartX, quartY, quartZ, found->second);
                }
            }
        }
    }
}

} // namespace

GeneratedTerrain::GeneratedTerrain(Chunk chunk, std::unique_ptr<densityfunction::SamplerContext> cache,
                                   std::unique_ptr<surface::SurfaceRuleEnvironment> environment)
    : chunk_(std::move(chunk)), cache_(std::move(cache)), environment_(std::move(environment)) {}

GeneratedTerrain::~GeneratedTerrain() = default;
GeneratedTerrain::GeneratedTerrain(GeneratedTerrain&&) noexcept = default;
GeneratedTerrain& GeneratedTerrain::operator=(GeneratedTerrain&&) noexcept = default;

GeneratedTerrain buildTerrain(const NoiseRouter& router, const surface::SurfaceRuleEnvironment& environment,
                              Chunk chunk, densityfunction::Sampler beardifier) {
    const std::int32_t chunkX = chunk.chunkX();
    const std::int32_t chunkZ = chunk.chunkZ();
    // parity: `ChunkGenerator.createBiomes` samples the climate through a cached context of its own.
    auto biomeContext = std::make_unique<densityfunction::SamplerContext>(
        densityfunction::SamplerContext::Options{.enableCaches = true});
    auto biomes = std::make_unique<surface::ChunkBiomeEnvironment>(environment, biomeContext.get());
    generationMetrics::measure(generationMetrics::Phase::BiomeFill,
                               [&] { fillBiomes(chunk, *biomes, biomeContext.get()); });
    NoiseChunk noiseChunk{router, std::move(beardifier),
                          densityfunction::DensityVolume{16, router.height, 16, chunkX * 16, router.minY, chunkZ * 16}};
    generationMetrics::measure(generationMetrics::Phase::TerrainFill, [&] { fillNoise(chunk, noiseChunk, router); });
    const material::BiomeGetter biomeGetter = [&biomes](std::int32_t x, std::int32_t y,
                                                        std::int32_t z) -> const std::string& {
        return biomes->biomeAt(x, y, z);
    };
    generationMetrics::measure(generationMetrics::Phase::SurfaceRules, [&] {
        router.materialSystem->buildSurface(chunk, noiseChunk.samplers(), noiseChunk.volume(), *router.materialRule,
                                            biomeGetter, router.minY, router.height);
    });
    generationMetrics::measure(generationMetrics::Phase::Carvers, [&] {
        // parity: the carvers' biome is the uncached climate resolver's, not the chunk's container.
        const carver::CarverBiome carverBiome = [&environment](std::int32_t quartX, std::int32_t quartY,
                                                               std::int32_t quartZ) -> const std::string& {
            return environment.noiseBiomeAt(quartX, quartY, quartZ, nullptr);
        };
        const carver::TopMaterial topMaterial = [&](std::int32_t x, std::int32_t y, std::int32_t z, bool underFluid) {
            return router.materialSystem->topMaterial(*router.materialRule, chunk, noiseChunk.samplers(), biomeGetter,
                                                      x, y, z, underFluid, router.minY, router.height);
        };
        carver::carve(chunk, router.state->seed(), noiseChunk, router, *router.carvers, carverBiome, topMaterial);
    });
    return GeneratedTerrain{std::move(chunk), std::move(biomeContext), std::move(biomes)};
}

GeneratedTerrain generateTerrain(const NoiseRouter& router, const surface::SurfaceRuleEnvironment& environment,
                                 std::int32_t chunkX, std::int32_t chunkZ, std::int64_t seed,
                                 const TerrainStructureGenerators& generators) {
    // parity: one `Beardifier` per chunk, summing every structure that adapts the terrain.
    StructureBeard beard;
    for (VillageStructureGenerator* set : {generators.camp, generators.village}) {
        if (set != nullptr) {
            set->addBeard(beard, chunkX, chunkZ, seed);
        }
    }
    for (JigsawStructureGenerator* bearded :
         {generators.pillagerOutpost, generators.trailRuins, generators.ancientCity, generators.trialChamber}) {
        if (bearded != nullptr) {
            bearded->addBeard(beard, chunkX, chunkZ, seed);
        }
    }
    if (generators.stronghold != nullptr) {
        generators.stronghold->addBeard(beard, chunkX, chunkZ, seed);
    }
    GeneratedTerrain terrain =
        buildTerrain(router, environment, Chunk{chunkX, chunkZ}, structureBeardifier(std::move(beard)));
    // parity: the `_WG` heightmaps stop being updated here, so everything that follows sees the bare
    // terrain rather than what the feature before it left standing.
    terrain.chunk().freezeWorldgenHeights();
    return terrain;
}

void decorate(GeneratedTerrain& terrain, std::int64_t seed, const NeighbourTerrain* neighbours,
              const TerrainStructureGenerators& generators, const structure::TemplateLoader& templates) {
    Chunk& chunk = terrain.chunk();
    const surface::SurfaceRuleEnvironment& environment = terrain.environment();
    const auto externalState = [neighbours](std::int32_t x, std::int32_t y,
                                            std::int32_t z) -> std::optional<std::int32_t> {
        return neighbours == nullptr ? std::nullopt : neighbours->blockStateIdAt(x, y, z);
    };
    const auto externalOceanFloor = [neighbours](std::int32_t x, std::int32_t z) -> std::optional<std::int32_t> {
        return neighbours == nullptr ? std::nullopt : neighbours->oceanFloorHeightAt(x, z);
    };
    generationMetrics::measure(generationMetrics::Phase::Decoration, [&] {
        // parity: one `WorldgenRandom` decorates the whole chunk in the jar, so the spare its
        // gaussian cached in one feature is handed to the next.
        const random::RandomSource::SharedGaussian sharedGaussian;
        // One reader for the whole phase. A feature body that reaches past the chunk edge asks the
        // chunk, and the chunk asks this: without it the outside reads as air, which makes a lake
        // preflight reject itself and a grass patch find no ground to stand on.
        const Chunk::ExternalStateScope scope{chunk, externalState};
        decoration::overworldLakeDecoration::decorate(chunk, seed, environment);
        decoration::frozenOceanDecoration::decorateLocalModifications(chunk, seed, environment);
        decoration::caveLocalModificationDecoration::decorate(chunk, seed, environment);
        decoration::forestRockDecoration::decorate(chunk, seed, environment);
        decoration::iceSpikesDecoration::decorate(chunk, seed, environment);
        decoration::desertWellDecoration::decorate(chunk, seed, environment, templates);
        // The fossil body takes its templates by name, while the world hands them out by identifier.
        const decoration::fossilFeature::TemplateLookup byName =
            templates ? decoration::fossilFeature::TemplateLookup{[&templates](std::string_view name) {
                const std::expected<wire::common::Identifier, wire::common::IdentifierError> parsed =
                    wire::common::Identifier::parse(name);
                return parsed.has_value() ? templates(*parsed) : nullptr;
            }}
                      : nullptr;
        // parity: `underground_structures` is step 3, and vanilla interleaves a step's structures with
        // that step's features rather than running every structure first. Running them all ahead of the
        // decoration lets a step 1 or 2 feature bury what a step 3 structure carved.
        if (generators.buriedTreasure != nullptr) {
            generators.buriedTreasure->generateInto(chunk, seed);
        }
        if (generators.trailRuins != nullptr) {
            generators.trailRuins->generateInto(chunk, chunk.chunkX(), chunk.chunkZ(), seed);
        }
        if (generators.trialChamber != nullptr) {
            generators.trialChamber->generateInto(chunk, chunk.chunkX(), chunk.chunkZ(), seed);
        }
        if (generators.mineshaft != nullptr) {
            generators.mineshaft->generateInto(chunk, seed);
        }
        decoration::overworldUndergroundStructures::decorate(chunk, seed, environment, byName, externalOceanFloor);
        // parity: `surface_structures` is step 4, the village included, so it follows step 3 rather
        // than opening the whole pass. The camps are its first eighteen structures.
        if (generators.camp != nullptr) {
            generators.camp->generateInto(chunk, chunk.chunkX(), chunk.chunkZ(), seed);
        }
        if (generators.village != nullptr) {
            generators.village->generateInto(chunk, chunk.chunkX(), chunk.chunkZ(), seed);
        }
        if (generators.desertPyramid != nullptr) {
            generators.desertPyramid->generateInto(chunk, seed);
        }
        if (generators.jungleTemple != nullptr) {
            generators.jungleTemple->generateInto(chunk, seed);
        }
        if (generators.mansion != nullptr) {
            generators.mansion->generateInto(chunk, seed);
        }
        if (generators.oceanMonument != nullptr) {
            generators.oceanMonument->generateInto(chunk, seed);
        }
        if (generators.stronghold != nullptr) {
            generators.stronghold->generateInto(chunk, seed);
        }
        if (generators.ruinedPortal != nullptr) {
            generators.ruinedPortal->generateInto(chunk, seed);
        }
        if (generators.swampHut != nullptr) {
            generators.swampHut->generateInto(chunk, seed);
        }
        if (generators.shipwreck != nullptr) {
            generators.shipwreck->generateInto(chunk, seed);
        }
        if (generators.oceanRuin != nullptr) {
            generators.oceanRuin->generateInto(chunk, seed);
        }
        if (generators.igloo != nullptr) {
            generators.igloo->generateInto(chunk, seed);
        }
        if (generators.pillagerOutpost != nullptr) {
            generators.pillagerOutpost->generateInto(chunk, chunk.chunkX(), chunk.chunkZ(), seed);
        }

        decoration::frozenOceanDecoration::decorateSurfaceStructures(chunk, seed, environment);
        // Ore and stone blobs are spheres around their origin, so most cross a chunk edge. The part
        // outside is deferred, and its guard has to be the block actually out there.
        decoration::overworldOreDecoration::decorate(chunk, seed, environment, externalOceanFloor, externalState);
        // Step 7 opens with its structures, and the ancient city is the first of the three, so the
        // sculk features below decorate a chunk that already has the city in it.
        if (generators.ancientCity != nullptr) {
            generators.ancientCity->generateInto(chunk, chunk.chunkX(), chunk.chunkZ(), seed);
        }
        decoration::sculkDecoration::decorate(chunk, seed, environment, externalState);
        decoration::caveSpeleothemDecoration::decorate(chunk, seed, environment, externalState);
        decoration::overworldOreDecoration::decorateInfested(chunk, seed, environment, externalOceanFloor,
                                                             externalState);
        decoration::overworldSpringDecoration::decorate(chunk, seed, environment, externalState);
        decoration::overworldVegetalDecoration::decorate(chunk, seed, environment, externalState);
    });
    // The water over a magma block or soul sand carries its column in the saved chunk, so this runs
    // once decoration has put both of them down.
    bubbleColumnFormation::formInto(chunk);
    generationMetrics::measure(generationMetrics::Phase::SurfaceFreeze,
                               [&] { decoration::surfaceFreezingDecoration::decorate(chunk, environment); });
    if (generationMetrics::enabled()) {
        generationMetrics::countChunk();
    }
}

Chunk generate(const NoiseRouter& router, const surface::SurfaceRuleEnvironment& environment, std::int32_t chunkX,
               std::int32_t chunkZ, std::int64_t seed, const TerrainStructureGenerators& generators,
               NeighbourTerrainCache* neighbours, const structure::TemplateLoader& templates,
               const BeforeDecoration& beforeDecoration) {
    GeneratedTerrain terrain = generateTerrain(router, environment, chunkX, chunkZ, seed, generators);
    if (beforeDecoration) {
        beforeDecoration(terrain.chunk());
    }
    const std::unique_ptr<NeighbourTerrain> around =
        neighbours == nullptr ? nullptr : neighbours->around(chunkX, chunkZ);
    decorate(terrain, seed, around.get(), generators, templates);
    return std::move(terrain.chunk());
}

} // namespace basalt::server::world::terrain::realTerrainGenerator
