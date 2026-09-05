#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/GenerationMetrics.hpp>
#include <basalt/server/world/terrain/BubbleColumnFormation.hpp>
#include <basalt/server/world/terrain/DensityChunkSampler.hpp>
#include <basalt/server/world/terrain/OverworldAquifer.hpp>
#include <basalt/server/world/terrain/OverworldOreVeinifier.hpp>
#include <basalt/server/world/terrain/RealTerrainGenerator.hpp>
#include <basalt/server/world/terrain/carver/OverworldCarvers.hpp>
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
#include <basalt/server/world/terrain/surface/ChunkBiomeEnvironment.hpp>
#include <basalt/server/world/terrain/surface/SurfaceBuilder.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleData.hpp>

#include <algorithm>
#include <map>
#include <string>
#include <utility>

namespace basalt::server::world::terrain::realTerrainGenerator {
namespace {

constexpr std::int32_t kCellWidth = 4;
constexpr std::int32_t kCellHeight = 8;

/// parity: `SurfaceRules.Context.SteepMaterialCondition` clamps its neighbour lookup to the chunk
/// under construction rather than crossing into the next one.
///
/// Biome zoom and raw-quart caching live in `ChunkBiomeEnvironment`. This final wrapper replaces
/// only the surface height and steepness queries that need the chunk currently under construction.
class ChunkScopedSurfaceEnvironment final : public surface::SurfaceRuleEnvironment {
public:
    ChunkScopedSurfaceEnvironment(std::unique_ptr<surface::SurfaceRuleEnvironment> delegate,
                                  std::vector<std::int32_t> heightmap, std::int32_t chunkX, std::int32_t chunkZ)
        : delegate_(std::move(delegate)), heightmap_(std::move(heightmap)), chunkX_(chunkX), chunkZ_(chunkZ) {}

    [[nodiscard]] std::optional<std::int64_t> biomeZoomSeed() const override { return delegate_->biomeZoomSeed(); }

    [[nodiscard]] std::int32_t surfaceDepthNoise(std::int32_t x, std::int32_t z) const override {
        return delegate_->surfaceDepthNoise(x, z);
    }

    [[nodiscard]] std::int32_t preliminarySurfaceLevel(std::int32_t x, std::int32_t z) const override {
        return delegate_->preliminarySurfaceLevel(x, z);
    }

    [[nodiscard]] double surfaceSecondaryNoise(std::int32_t x, std::int32_t z) const override {
        return delegate_->surfaceSecondaryNoise(x, z);
    }

    [[nodiscard]] double sample2d(std::string_view noiseId, std::int32_t x, std::int32_t z) const override {
        return delegate_->sample2d(noiseId, x, z);
    }

    [[nodiscard]] double sample2dScaled(std::string_view noiseId, double x, double z) const override {
        return delegate_->sample2dScaled(noiseId, x, z);
    }

    [[nodiscard]] double sample3d(std::string_view noiseId, std::int32_t x, std::int32_t y,
                                  std::int32_t z) const override {
        return delegate_->sample3d(noiseId, x, y, z);
    }

    [[nodiscard]] float verticalGradientRandom(std::string_view name, std::int32_t x, std::int32_t y,
                                               std::int32_t z) const override {
        return delegate_->verticalGradientRandom(name, x, y, z);
    }

    [[nodiscard]] random::RandomSource positionalRandom(std::int32_t x, std::int32_t y, std::int32_t z) const override {
        return delegate_->positionalRandom(x, y, z);
    }

    [[nodiscard]] const std::string& biomeAt(std::int32_t x, std::int32_t y, std::int32_t z) const override {
        return delegate_->biomeAt(x, y, z);
    }

    [[nodiscard]] const std::string& biomeAt(std::int32_t x, std::int32_t y, std::int32_t z,
                                             densityfunction::DensityCache* cache) const override {
        return delegate_->biomeAt(x, y, z, cache);
    }

    [[nodiscard]] const std::string& noiseBiomeAt(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ,
                                                  densityfunction::DensityCache* cache) const override {
        return delegate_->noiseBiomeAt(quartX, quartY, quartZ, cache);
    }

    [[nodiscard]] std::int32_t clayBand(std::int32_t x, std::int32_t y, std::int32_t z) const override {
        return delegate_->clayBand(x, y, z);
    }

    [[nodiscard]] std::int32_t estimatedSurfaceHeight(std::int32_t x, std::int32_t z) const override {
        if ((x >> 4) == chunkX_ && (z >> 4) == chunkZ_) {
            return heightmap_[static_cast<std::size_t>((x & 15) + (z & 15) * 16)];
        }
        return delegate_->estimatedSurfaceHeight(x, z);
    }

    [[nodiscard]] bool isSteep(std::int32_t x, std::int32_t z) const override {
        const std::int32_t localX = x & 15;
        const std::int32_t localZ = z & 15;
        const std::int32_t zNorth = std::max(localZ - 1, 0);
        const std::int32_t zSouth = std::min(localZ + 1, 15);
        if (heightAt(localX, zSouth) >= heightAt(localX, zNorth) + 4) {
            return true;
        }
        const std::int32_t xWest = std::max(localX - 1, 0);
        const std::int32_t xEast = std::min(localX + 1, 15);
        return heightAt(xWest, localZ) >= heightAt(xEast, localZ) + 4;
    }

private:
    [[nodiscard]] std::int32_t heightAt(std::int32_t localX, std::int32_t localZ) const {
        return heightmap_[static_cast<std::size_t>(localX + localZ * 16)];
    }

    std::unique_ptr<surface::SurfaceRuleEnvironment> delegate_;
    std::vector<std::int32_t> heightmap_;
    std::int32_t chunkX_;
    std::int32_t chunkZ_;
};

/// Fills every quart cell of the chunk with its real biome network id, so the wire palette carries
/// the actual biome layout the client needs for grass, water and fog colours.
void fillBiomes(Chunk& chunk, const surface::SurfaceRuleEnvironment& environment,
                densityfunction::DensityCache* cache) {
    // A chunk holds a handful of distinct biomes, so the identifier parse and the registry lookup
    // are paid once per name instead of once per each of the 1536 quarts.
    std::map<std::string, std::int32_t, std::less<>> idByName;
    // parity: `ChunkAccess.fillBiomesFromNoise` goes section by section and
    // `LevelChunkSection.fillBiomesFromNoise` goes x, then y, then z inside each one. The order decides real
    // biomes: `Climate.RTree` seeds every search with the thread's previous leaf and keeps it when nothing is
    // strictly closer, so a column whose continentalness quantises onto a band edge takes the biome of the
    // cell asked for just before it.
    const std::int32_t sections = kOverworldHeight.height() / 16;
    for (std::int32_t section = 0; section < sections; ++section) {
        for (std::int32_t quartX = 0; quartX < 4; ++quartX) {
            for (std::int32_t withinSection = 0; withinSection < 4; ++withinSection) {
                const std::int32_t quartY = section * 4 + withinSection;
                const std::int32_t blockY = kOverworldHeight.minY() + quartY * 4;
                for (std::int32_t quartZ = 0; quartZ < 4; ++quartZ) {
                    const std::string& biome = environment.noiseBiomeAt(chunk.chunkX() * 4 + quartX, blockY >> 2,
                                                                        chunk.chunkZ() * 4 + quartZ, cache);
                    auto found = idByName.find(biome);
                    if (found == idByName.end()) {
                        std::int32_t id = 0;
                        const std::expected<wire::common::Identifier, wire::common::IdentifierError> parsed =
                            wire::common::Identifier::parse(biome);
                        if (parsed.has_value()) {
                            const std::optional<std::int32_t> registered = registry::biomes::registry().idOf(*parsed);
                            id = registered.value_or(0);
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

GeneratedTerrain::GeneratedTerrain(Chunk chunk, std::unique_ptr<densityfunction::DensityCache> cache,
                                   std::unique_ptr<surface::SurfaceRuleEnvironment> environment)
    : chunk_(std::move(chunk)), cache_(std::move(cache)), environment_(std::move(environment)) {}

GeneratedTerrain::~GeneratedTerrain() = default;
GeneratedTerrain::GeneratedTerrain(GeneratedTerrain&&) noexcept = default;
GeneratedTerrain& GeneratedTerrain::operator=(GeneratedTerrain&&) noexcept = default;

GeneratedTerrain generateTerrain(const OverworldNoiseRouter& router, const surface::SurfaceRuleEnvironment& environment,
                                 std::int32_t chunkX, std::int32_t chunkZ, std::int64_t seed,
                                 const TerrainStructureGenerators& generators) {
    Chunk chunk{chunkX, chunkZ};
    // One cache for the whole chunk, shared by terrain shape, surface rules and biome filling: they
    // all walk the same router subtrees, so a cache per phase would throw away most of it.
    auto cache = std::make_unique<densityfunction::DensityCache>(chunkX, chunkZ, densityfunction::markerSlotCount());
    densityfunction::Function density = generators.village != nullptr
                                            ? generators.village->finalDensityForChunk(chunkX, chunkZ, seed)
                                            : router.finalDensity;
    for (JigsawStructureGenerator* bearded :
         {generators.pillagerOutpost, generators.trailRuins, generators.ancientCity, generators.trialChamber}) {
        if (bearded != nullptr) {
            density = bearded->beardified(std::move(density), chunkX, chunkZ, seed);
        }
    }
    if (generators.stronghold != nullptr) {
        density = generators.stronghold->beardified(std::move(density), chunkX, chunkZ, seed);
    }
    // parity: `ChunkStatus.BIOMES` runs before `NOISE`, and everything after it reads the biome container
    // rather than the climate tree. Filling first and letting the surface rules and the decorators read the
    // same memo is what reproduces that: `Climate.RTree` seeds each search with the thread's previous leaf
    // and keeps it when nothing is strictly closer, so a lookup served from a cache instead of the tree
    // silently moves every tie that follows it.
    auto biomeEnvironment = std::make_unique<surface::ChunkBiomeEnvironment>(environment, cache.get());
    generationMetrics::measure(generationMetrics::Phase::BiomeFill,
                               [&] { fillBiomes(chunk, *biomeEnvironment, cache.get()); });
    DensityChunkSampler sampler{std::move(density),        chunkX,     chunkZ,      kOverworldHeight.minY(),
                                kOverworldHeight.height(), kCellWidth, kCellHeight, cache.get()};
    OverworldAquifer aquifer{router,     seed, chunkX, chunkZ, kOverworldHeight.minY(), kOverworldHeight.height(),
                             cache.get()};
    OverworldOreVeinifier oreVeins{router, seed, cache.get()};
    generationMetrics::measure(generationMetrics::Phase::TerrainFill, [&] {
        // Cell by cell rather than column by column, so a cell's eight interpolation corners are
        // computed once and every block inside pays only the lerp. Cells are never skipped
        // wholesale: interpolation is marker-scoped, so corner signs alone cannot prove a cell
        // empty. Within a cell the order is y, then z, then x, matching the section's own layout.
        for (std::int32_t cellY = 0; cellY < sampler.verticalCells(); ++cellY) {
            const std::int32_t cellMinY = kOverworldHeight.minY() + cellY * kCellHeight;
            const std::int32_t cellMaxY = cellMinY + kCellHeight - 1;
            for (std::int32_t cellX = 0; cellX < sampler.horizontalCells(); ++cellX) {
                for (std::int32_t cellZ = 0; cellZ < sampler.horizontalCells(); ++cellZ) {
                    for (std::int32_t y = cellMinY; y <= cellMaxY; ++y) {
                        for (std::int32_t z = cellZ * kCellWidth; z < (cellZ + 1) * kCellWidth; ++z) {
                            for (std::int32_t x = cellX * kCellWidth; x < (cellX + 1) * kCellWidth; ++x) {
                                const double value = sampler.sample(x, y, z);
                                const registry::BlockState* material =
                                    aquifer.computeSubstance(chunkX * 16 + x, y, chunkZ * 16 + z, value);
                                if (material == nullptr) {
                                    const std::optional<std::int32_t> vein =
                                        oreVeins.compute(chunkX * 16 + x, y, chunkZ * 16 + z);
                                    chunk.setBlockStateId(x, y, z, vein.value_or(registry::blocks::stone().stateId));
                                } else if (material->stateId != registry::blocks::air().stateId) {
                                    chunk.setBlockStateId(x, y, z, material->stateId);
                                }
                            }
                        }
                    }
                }
            }
        }
    });
    std::vector<std::int32_t> heightmap = chunk.heightmap(HeightmapType::OceanFloor);
    auto chunkEnvironment = std::make_unique<ChunkScopedSurfaceEnvironment>(std::move(biomeEnvironment),
                                                                            std::move(heightmap), chunkX, chunkZ);
    const surface::RuleSource& surfaceRule = surface::surfaceRuleData::overworld();
    generationMetrics::measure(generationMetrics::Phase::SurfaceRules,
                               [&] { surface::surfaceBuilder::buildSurface(chunk, *chunkEnvironment, surfaceRule); });
    generationMetrics::measure(generationMetrics::Phase::Carvers,
                               [&] { carver::carve(chunk, seed, aquifer, *chunkEnvironment, surfaceRule); });
    // parity: the `_WG` heightmaps stop being updated here, so everything that follows sees the bare
    // terrain rather than what the feature before it left standing.
    chunk.freezeWorldgenHeights();
    return GeneratedTerrain{std::move(chunk), std::move(cache), std::move(chunkEnvironment)};
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
        // One reader for the whole phase. A feature body that reaches past the chunk edge asks the
        // chunk, and the chunk asks this: without it the outside reads as air, which makes a lake
        // preflight reject itself and a grass patch find no ground to stand on.
        const Chunk::ExternalStateScope scope{chunk, externalState};
        decoration::overworldLakeDecoration::decorate(chunk, seed, environment);
        decoration::frozenOceanDecoration::decorateLocalModifications(chunk, seed, environment);
        decoration::caveLocalModificationDecoration::decorate(chunk, seed, environment);
        decoration::forestRockDecoration::decorate(chunk, seed, environment);
        decoration::iceSpikesDecoration::decorate(chunk, seed, environment);
        decoration::desertWellDecoration::decorate(chunk, seed, environment);
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
        // than opening the whole pass.
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
        decoration::overworldOreDecoration::decorateInfested(chunk, seed, environment, nullptr, externalState);
        decoration::overworldSpringDecoration::decorate(chunk, seed, environment, externalState);
        decoration::overworldVegetalDecoration::decorate(chunk, seed, environment);
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

Chunk generate(const OverworldNoiseRouter& router, const surface::SurfaceRuleEnvironment& environment,
               std::int32_t chunkX, std::int32_t chunkZ, std::int64_t seed,
               const TerrainStructureGenerators& generators, NeighbourTerrainCache* neighbours,
               const structure::TemplateLoader& templates, const BeforeDecoration& beforeDecoration) {
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
