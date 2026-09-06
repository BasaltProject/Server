#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/runtime/world/BiomeTagLoader.hpp>
#include <basalt/server/runtime/world/BiomeZoomSeed.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/NetherSurfaceEnvironment.hpp>
#include <basalt/server/runtime/world/NoiseParametersJson.hpp>
#include <basalt/server/runtime/world/OriginalMobSpawner.hpp>
#include <basalt/server/runtime/world/RealBiomeSpawns.hpp>
#include <basalt/server/runtime/world/RealNoiseParameters.hpp>
#include <basalt/server/runtime/world/RealSurfaceEnvironment.hpp>
#include <basalt/server/runtime/world/RuinedPortalVariants.hpp>
#include <basalt/server/runtime/world/SpawnEquipment.hpp>
#include <basalt/server/runtime/world/VanillaWorldGeneratorFactory.hpp>
#include <basalt/server/runtime/world/VillageDataLoader.hpp>
#include <basalt/server/world/climate/Sampler.hpp>
#include <basalt/server/world/climate/SpawnFinder.hpp>
#include <basalt/server/world/terrain/BuriedTreasureStructureGenerator.hpp>
#include <basalt/server/world/terrain/DesertPyramidStructureGenerator.hpp>
#include <basalt/server/world/terrain/DimensionTerrainGenerator.hpp>
#include <basalt/server/world/terrain/EndCityStructureGenerator.hpp>
#include <basalt/server/world/terrain/IglooStructureGenerator.hpp>
#include <basalt/server/world/terrain/JigsawStructureGenerator.hpp>
#include <basalt/server/world/terrain/JungleTempleStructureGenerator.hpp>
#include <basalt/server/world/terrain/MineshaftStructureGenerator.hpp>
#include <basalt/server/world/terrain/NetherBiomeSource.hpp>
#include <basalt/server/world/terrain/NetherStructureGenerator.hpp>
#include <basalt/server/world/terrain/NoiseColumnHeights.hpp>
#include <basalt/server/world/terrain/NoiseRouterData.hpp>
#include <basalt/server/world/terrain/OceanMonumentStructureGenerator.hpp>
#include <basalt/server/world/terrain/OceanRuinStructureGenerator.hpp>
#include <basalt/server/world/terrain/RealTerrainGenerator.hpp>
#include <basalt/server/world/terrain/RuinedPortalStructureGenerator.hpp>
#include <basalt/server/world/terrain/ShipwreckStructureGenerator.hpp>
#include <basalt/server/world/terrain/StrongholdStructureGenerator.hpp>
#include <basalt/server/world/terrain/SwampHutStructureGenerator.hpp>
#include <basalt/server/world/terrain/TheEndBiomeSource.hpp>
#include <basalt/server/world/terrain/VillageStructureGenerator.hpp>
#include <basalt/server/world/terrain/WoodlandMansionStructureGenerator.hpp>

#include <map>
#include <memory>
#include <set>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

namespace densityfunction = server::world::densityfunction;
namespace structure = server::world::structure;
namespace terrain = server::world::terrain;

using wire::common::Identifier;

/// The settings of the four Overworld jigsaw structures, which are one generator apiece.
/// The tag loader answers a plain set; every generator takes the transparent-comparator one.
[[nodiscard]] std::set<std::string, std::less<>> biomesOf(BiomeTagLoader& tags, const std::string& name) {
    const std::set<std::string>& members = tags.biomesIn(name);
    return {members.begin(), members.end()};
}

// The settings live in `server-world` beside the generator that reads them, and the tests exercise
// those same objects. Keeping a second copy here is what let a wrong step and a wrong terrain
// adaptation ship while the suite stayed green.
[[nodiscard]] const terrain::JigsawStructureSettings& pillagerOutpostSettings() {
    return terrain::jigsawStructures::pillagerOutpost();
}

[[nodiscard]] const terrain::JigsawStructureSettings& trailRuinsSettings() {
    return terrain::jigsawStructures::trailRuins();
}

[[nodiscard]] const terrain::JigsawStructureSettings& ancientCitySettings() {
    return terrain::jigsawStructures::ancientCity();
}

[[nodiscard]] const terrain::JigsawStructureSettings& trialChamberSettings() {
    return terrain::jigsawStructures::trialChamber();
}

} // namespace

/// One world's generator and everything it borrows, kept alive together.
struct VanillaWorldGeneratorFactory::Built {
    // Declared in construction order: the router is read by the environment, which is read by every
    // structure generator, and destruction has to run the other way round.
    std::unique_ptr<StructureTemplates> templates;
    std::unique_ptr<VillageDataLoader> loader;
    std::unique_ptr<BiomeTagLoader> tags;
    std::unique_ptr<RealBiomeSpawns> biomeSpawns;
    std::unique_ptr<terrain::OverworldNoiseRouter> overworldRouter;
    std::unique_ptr<terrain::NetherNoiseRouter> netherRouter;
    std::unique_ptr<terrain::EndNoiseRouter> endRouter;
    std::unique_ptr<terrain::NetherBiomeSource> netherBiomes;
    std::unique_ptr<terrain::TheEndBiomeSource> endBiomes;
    std::unique_ptr<RealSurfaceEnvironment> overworldSurface;
    std::unique_ptr<NetherSurfaceEnvironment> netherSurface;
    std::unique_ptr<terrain::NoiseColumnHeights> columns;
    std::unique_ptr<terrain::VillageStructureGenerator> village;
    std::unique_ptr<terrain::MineshaftStructureGenerator> mineshafts;
    std::unique_ptr<terrain::RuinedPortalStructureGenerator> ruinedPortals;
    std::unique_ptr<terrain::SwampHutStructureGenerator> swampHuts;
    std::unique_ptr<terrain::DesertPyramidStructureGenerator> desertPyramids;
    std::unique_ptr<terrain::JungleTempleStructureGenerator> jungleTemples;
    std::unique_ptr<terrain::WoodlandMansionStructureGenerator> mansions;
    std::unique_ptr<terrain::OceanMonumentStructureGenerator> oceanMonuments;
    std::unique_ptr<terrain::StrongholdStructureGenerator> strongholds;
    std::unique_ptr<terrain::JigsawStructureGenerator> pillagerOutposts;
    std::unique_ptr<terrain::JigsawStructureGenerator> trailRuins;
    std::unique_ptr<terrain::BuriedTreasureStructureGenerator> buriedTreasures;
    std::unique_ptr<terrain::ShipwreckStructureGenerator> shipwrecks;
    std::unique_ptr<terrain::OceanRuinStructureGenerator> oceanRuins;
    std::unique_ptr<terrain::IglooStructureGenerator> igloos;
    std::unique_ptr<terrain::JigsawStructureGenerator> ancientCities;
    std::unique_ptr<terrain::JigsawStructureGenerator> trialChambers;
    std::unique_ptr<terrain::NetherStructureGenerator> netherStructures;
    std::unique_ptr<terrain::EndCityStructureGenerator> endCities;
    std::unique_ptr<terrain::NeighbourTerrainCache> neighbours;
    /// Its own, not the one the features read: a structure height query generates chunks a feature
    /// never asked for, and sharing the cache moves what the features find in it.
    std::unique_ptr<terrain::NeighbourTerrainCache> structureHeights;
    std::unique_ptr<SpawnEquipmentFinalizer> equipment;
    terrain::TerrainStructureGenerators generators;
    std::int64_t seed = 0;
    bool spawnMobs = false;
};

VanillaWorldGeneratorFactory::VanillaWorldGeneratorFactory(Jar jar, wire::common::Logger logger)
    : jar_(std::move(jar)), logger_(std::move(logger)) {}

VanillaWorldGeneratorFactory::~VanillaWorldGeneratorFactory() = default;

VanillaWorldGeneratorFactory::CacheStats VanillaWorldGeneratorFactory::cacheStats() const {
    CacheStats stats;
    for (const std::unique_ptr<Built>& built : built_) {
        if (built->village != nullptr) {
            const auto [count, pieces] = built->village->assemblyStats();
            stats.assemblies += count;
            stats.assemblyPieces += pieces;
        }
        for (const terrain::JigsawStructureGenerator* jigsaw :
             {built->pillagerOutposts.get(), built->trailRuins.get(), built->ancientCities.get(),
              built->trialChambers.get()}) {
            if (jigsaw != nullptr) {
                const auto [count, pieces] = jigsaw->assemblyStats();
                stats.assemblies += count;
                stats.assemblyPieces += pieces;
            }
        }
    }
    return stats;
}

VanillaWorldGeneratorFactory::Generators
VanillaWorldGeneratorFactory::create(const config::WorldDeclaration& declaration, const WorldMetadata& metadata) {
    const bool spawnMobs = metadata.gameRules.get(gameRules::kSpawnMobs);
    switch (declaration.dimension) {
    case config::WorldDimension::Overworld:
        return overworld(metadata.seed, spawnMobs);
    case config::WorldDimension::Nether:
        return nether(metadata.seed, spawnMobs);
    case config::WorldDimension::End:
        return end(metadata.seed);
    }
    return {};
}

VanillaWorldGeneratorFactory::Generators VanillaWorldGeneratorFactory::overworld(std::int64_t seed, bool spawnMobs) {
    auto built = std::make_unique<Built>();
    built->seed = seed;
    built->spawnMobs = spawnMobs;
    built->templates = std::make_unique<StructureTemplates>(
        StructureTemplates::Jar{.readEntry = jar_.readEntry, .dataIds = jar_.dataIds});
    built->loader = std::make_unique<VillageDataLoader>(*built->templates, jar_.readEntry);
    built->tags = std::make_unique<BiomeTagLoader>(jar_.readText);
    built->biomeSpawns = std::make_unique<RealBiomeSpawns>(RealBiomeSpawns::Jar{.readText = jar_.readText});
    built->equipment = std::make_unique<SpawnEquipmentFinalizer>(
        SpawnEquipmentJar{.entriesUnder = nullptr, .readText = jar_.readText});

    const std::int64_t biomeZoomSeed = obfuscateBiomeSeed(seed);
    // parity: `RandomState` forks the factory off a seeded source, so the two longs it draws are what
    // seeds every noise. Building the factory straight from the level seed generates another world.
    const random::PositionalRandomFactory positional = random::RandomSource::xoroshiro(seed).forkPositional();
    // parity: `RandomState` wires `BlendedNoise` from `random.fromHashOf("minecraft:terrain")`.
    random::RandomSource terrainSource = positional.fromHashOf("minecraft:terrain");
    const densityfunction::Function base3d =
        densityfunction::functions::blendedNoise(terrainSource, 0.25, 0.125, 80.0, 160.0, 8.0);

    // The noises outlive the router that samples them, so they are shared rather than borrowed
    // from a local.
    const auto noises = std::make_shared<RealNoiseParameters>(jar_.readText, noiseParametersJson::parse, positional);
    const auto shared = std::make_shared<
        std::map<std::string, std::shared_ptr<const server::world::noise::NormalNoise>, std::less<>>>();
    // parity: the overworld's shape is the jar's own noise files. Without them there is no terrain
    // to make, so the world says so rather than handing back a generator that dereferences nothing.
    if (noises->get("continentalness") == nullptr) {
        logger_.warn([] {
            return std::string{"No worldgen noise in the server jar, so no normal terrain can be generated. "
                               "The download the EULA step performs is what carries it."};
        });
        return {};
    }
    built->overworldRouter = std::make_unique<terrain::OverworldNoiseRouter>(terrain::noiseRouterData::overworld(
        [noises, shared](std::string_view id) -> std::shared_ptr<const server::world::noise::NormalNoise> {
            if (const auto found = shared->find(id); found != shared->end()) {
                return found->second;
            }
            const server::world::noise::NormalNoise* made = noises->get(id);
            std::shared_ptr<const server::world::noise::NormalNoise> held =
                made == nullptr ? nullptr : std::make_shared<const server::world::noise::NormalNoise>(*made);
            shared->emplace(std::string{id}, held);
            return held;
        },
        base3d));
    built->overworldSurface = std::make_unique<RealSurfaceEnvironment>(
        jar_.readText, noiseParametersJson::parse, positional, *built->overworldRouter, biomeZoomSeed);

    const RealSurfaceEnvironment& surface = *built->overworldSurface;
    // The return type is spelled out because `auto` would deduce a copy, and a consumer that takes
    // a `std::function` returning `string_view` would then be handed a view of a temporary that dies
    // on the way out. The environment interns these names, so the reference outlives every reader.
    const auto blockBiome = [&surface](std::int32_t x, std::int32_t y, std::int32_t z) -> const std::string& {
        return surface.biomeAt(x, y, z);
    };
    const auto noiseBiome = [&surface](std::int32_t x, std::int32_t y, std::int32_t z) -> const std::string& {
        return surface.noiseBiomeAt(x, y, z);
    };
    const auto quartBiome = [&surface](std::int32_t x, std::int32_t y, std::int32_t z) -> const std::string& {
        return surface.noiseBiomeAt(x >> 2, y >> 2, z >> 2);
    };
    // Two shapes of the same lookup: the pool assembly names templates by string, and the
    // dimension generators name them by identifier.
    StructureTemplates& templates = *built->templates;
    const auto templateOf = [&templates](std::string_view name) -> const structure::StructureTemplate* {
        const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(name);
        return id.has_value() ? templates.find(*id) : nullptr;
    };
    const auto poolLookup = [&loader = *built->loader](std::string_view name) { return loader.pool(name); };

    const structure::StructurePlacementGrid villageGrid{34, 8, 10387312};
    built->village = std::make_unique<terrain::VillageStructureGenerator>(*built->overworldRouter, villageGrid,
                                                                          quartBiome, 6, 80, poolLookup, templateOf);
    built->columns = std::make_unique<terrain::NoiseColumnHeights>(*built->overworldRouter);

    BiomeTagLoader& tags = *built->tags;
    const terrain::NoiseColumnHeights& columns = *built->columns;
    const auto surfaceHeight = [&columns](std::int32_t x, std::int32_t z) { return columns.surfaceHeight(x, z); };
    const auto oceanFloor = [&columns](std::int32_t x, std::int32_t z) { return columns.oceanFloorHeight(x, z); };
    // A cache and not the bounded view the features read: a piece settling on the mean of its
    // footprint is placed by every chunk it crosses, and a view that sometimes refuses would tear it
    // in two. Its own cache, because generating a chunk nobody asked for moves what the features
    // find in theirs.
    const auto worldHeight = [holder = built.get()](std::int32_t x, std::int32_t z, server::world::HeightmapType type) {
        return holder->structureHeights->worldgenHeightAt(x, z, type);
    };

    built->mineshafts = std::make_unique<terrain::MineshaftStructureGenerator>(
        blockBiome, biomesOf(tags, "minecraft:has_structure/mineshaft"),
        biomesOf(tags, "minecraft:has_structure/mineshaft_mesa"), surfaceHeight);
    built->ruinedPortals = std::make_unique<terrain::RuinedPortalStructureGenerator>(
        ruinedPortalVariants::overworld(tags), templateOf, blockBiome, surfaceHeight, oceanFloor,
        [&columns](std::int32_t x, std::int32_t y, std::int32_t z) { return columns.isSolid(x, y, z); });
    built->swampHuts = std::make_unique<terrain::SwampHutStructureGenerator>(blockBiome, surfaceHeight);
    built->desertPyramids = std::make_unique<terrain::DesertPyramidStructureGenerator>(blockBiome, surfaceHeight);
    built->jungleTemples = std::make_unique<terrain::JungleTempleStructureGenerator>(
        blockBiome, biomesOf(tags, "minecraft:has_structure/jungle_temple"), surfaceHeight);
    built->mansions = std::make_unique<terrain::WoodlandMansionStructureGenerator>(
        blockBiome, biomesOf(tags, "minecraft:has_structure/woodland_mansion"), templateOf, surfaceHeight);
    built->oceanMonuments = std::make_unique<terrain::OceanMonumentStructureGenerator>(
        noiseBiome, biomesOf(tags, "minecraft:has_structure/ocean_monument"),
        biomesOf(tags, "minecraft:required_ocean_monument_surrounding"), oceanFloor);
    built->strongholds = std::make_unique<terrain::StrongholdStructureGenerator>(
        structure::ConcentricRingsPlacement{32, 3, 128, biomesOf(tags, "minecraft:stronghold_biased_to"), noiseBiome},
        seed, noiseBiome, biomesOf(tags, "minecraft:has_structure/stronghold"));
    // An outpost takes a fifth of its candidates and keeps ten chunks clear of any village, which is
    // why it needs the villages' own grid to answer at all.
    // The village grid is captured by value and handed back by reference: the exclusion zone reads
    // it long after this function returns, so it cannot borrow the local one.
    const auto villages = std::make_shared<structure::StructurePlacementGrid>(villageGrid);
    const structure::StructurePlacementGrid outpostGrid{
        32,
        8,
        165745296,
        structure::SpreadType::Linear,
        0.2F,
        structure::FrequencyReduction::LegacyType1,
        std::make_shared<const structure::ExclusionZone>(
            10, [villages]() -> const structure::StructurePlacementGrid& { return *villages; })};
    built->pillagerOutposts = std::make_unique<terrain::JigsawStructureGenerator>(
        pillagerOutpostSettings(), *built->overworldRouter, outpostGrid, quartBiome,
        biomesOf(tags, "minecraft:has_structure/pillager_outpost"), poolLookup, templateOf);
    built->trailRuins = std::make_unique<terrain::JigsawStructureGenerator>(
        trailRuinsSettings(), *built->overworldRouter, structure::StructurePlacementGrid{34, 8, 83469867}, quartBiome,
        biomesOf(tags, "minecraft:has_structure/trail_ruins"), poolLookup, templateOf);
    built->buriedTreasures = std::make_unique<terrain::BuriedTreasureStructureGenerator>(
        quartBiome, biomesOf(tags, "minecraft:has_structure/buried_treasure"), surfaceHeight);
    built->shipwrecks = std::make_unique<terrain::ShipwreckStructureGenerator>(
        quartBiome, biomesOf(tags, "minecraft:has_structure/shipwreck"),
        biomesOf(tags, "minecraft:has_structure/shipwreck_beached"), templateOf, surfaceHeight, oceanFloor,
        worldHeight);
    built->oceanRuins = std::make_unique<terrain::OceanRuinStructureGenerator>(
        quartBiome, biomesOf(tags, "minecraft:has_structure/ocean_ruin_cold"),
        biomesOf(tags, "minecraft:has_structure/ocean_ruin_warm"), templateOf, oceanFloor, worldHeight);
    built->igloos = std::make_unique<terrain::IglooStructureGenerator>(
        quartBiome, biomesOf(tags, "minecraft:has_structure/igloo"), templateOf, surfaceHeight, worldHeight);
    built->ancientCities = std::make_unique<terrain::JigsawStructureGenerator>(
        ancientCitySettings(), *built->overworldRouter, structure::StructurePlacementGrid{24, 8, 20083232}, blockBiome,
        biomesOf(tags, "minecraft:has_structure/ancient_city"), poolLookup, templateOf);
    built->trialChambers = std::make_unique<terrain::JigsawStructureGenerator>(
        trialChamberSettings(), *built->overworldRouter, structure::StructurePlacementGrid{34, 12, 94251327},
        blockBiome, biomesOf(tags, "minecraft:has_structure/trial_chambers"), poolLookup, templateOf);

    built->generators = terrain::TerrainStructureGenerators{.village = built->village.get(),
                                                            .mineshaft = built->mineshafts.get(),
                                                            .ruinedPortal = built->ruinedPortals.get(),
                                                            .swampHut = built->swampHuts.get(),
                                                            .desertPyramid = built->desertPyramids.get(),
                                                            .jungleTemple = built->jungleTemples.get(),
                                                            .mansion = built->mansions.get(),
                                                            .oceanMonument = built->oceanMonuments.get(),
                                                            .stronghold = built->strongholds.get(),
                                                            .pillagerOutpost = built->pillagerOutposts.get(),
                                                            .trailRuins = built->trailRuins.get(),
                                                            .buriedTreasure = built->buriedTreasures.get(),
                                                            .shipwreck = built->shipwrecks.get(),
                                                            .oceanRuin = built->oceanRuins.get(),
                                                            .igloo = built->igloos.get(),
                                                            .ancientCity = built->ancientCities.get(),
                                                            .trialChamber = built->trialChambers.get()};

    // Step one's bodies read past the chunk edge, so decoration needs the neighbours' undecorated
    // terrain. One cache per world: a neighbour generated for one chunk is read by the next.
    Built& world = *built;
    built->neighbours =
        std::make_unique<terrain::NeighbourTerrainCache>([&world](std::int32_t neighbourX, std::int32_t neighbourZ) {
            // Moved out rather than copied: a chunk owns its sections, and the terrain that made
            // it is finished with them the moment it answers.
            terrain::realTerrainGenerator::GeneratedTerrain terrain = terrain::realTerrainGenerator::generateTerrain(
                *world.overworldRouter, *world.overworldSurface, neighbourX, neighbourZ, world.seed, world.generators);
            return std::move(terrain.chunk());
        });

    built->structureHeights =
        std::make_unique<terrain::NeighbourTerrainCache>([&world](std::int32_t neighbourX, std::int32_t neighbourZ) {
            terrain::realTerrainGenerator::GeneratedTerrain terrain = terrain::realTerrainGenerator::generateTerrain(
                *world.overworldRouter, *world.overworldSurface, neighbourX, neighbourZ, world.seed, world.generators);
            return std::move(terrain.chunk());
        });

    Generators answer;
    answer.useQueuedWrites = [&world](terrain::NeighbourTerrainCache::QueuedAt queuedAt) {
        if (world.neighbours != nullptr) {
            world.neighbours->setQueuedAt(std::move(queuedAt));
        }
    };
    answer.villages = built->village.get();
    answer.strongholds = built->strongholds.get();
    answer.generate = [&world](std::int32_t chunkX, std::int32_t chunkZ, std::int64_t gameTime,
                               const std::string& difficulty,
                               const std::function<void(server::world::Chunk&)>& beforeDecoration) {
        server::world::Chunk chunk = terrain::realTerrainGenerator::generate(
            *world.overworldRouter, *world.overworldSurface, chunkX, chunkZ, world.seed, world.generators,
            world.neighbours.get(), world.loader->templateLoader(), beforeDecoration);
        if (world.spawnMobs) {
            const std::expected<Identifier, wire::common::IdentifierError> biome = Identifier::parse(
                world.overworldSurface->biomeAt(chunkX << 4, chunk.verticalBounds().maxY(), chunkZ << 4));
            if (biome.has_value()) {
                random::RandomSource random = random::RandomSource::legacy(0);
                static_cast<void>(random::setDecorationSeed(random, world.seed, chunkX << 4, chunkZ << 4));
                static_cast<void>(originalMobSpawner::spawnInto(
                    chunk, world.biomeSpawns->listFor(*biome, "creature"),
                    world.biomeSpawns->creatureProbabilityFor(*biome), random,
                    {.gameTime = gameTime, .difficulty = difficulty, .equipment = world.equipment.get()}));
            }
        }
        return chunk;
    };
    answer.spawnChunk = [&world] {
        const terrain::OverworldNoiseRouter& router = *world.overworldRouter;
        const auto [blockX, blockZ] =
            server::world::climate::spawnFinder::findSpawnPosition(server::world::climate::Sampler{
                router.temperature, router.vegetation, router.continents, router.erosion, router.depth, router.ridges});
        return std::pair{blockX >> 4, blockZ >> 4};
    };
    built_.push_back(std::move(built));
    return answer;
}

VanillaWorldGeneratorFactory::Generators VanillaWorldGeneratorFactory::nether(std::int64_t seed, bool spawnMobs) {
    auto built = std::make_unique<Built>();
    built->seed = seed;
    built->spawnMobs = spawnMobs;
    built->templates = std::make_unique<StructureTemplates>(
        StructureTemplates::Jar{.readEntry = jar_.readEntry, .dataIds = jar_.dataIds});
    built->loader = std::make_unique<VillageDataLoader>(*built->templates, jar_.readEntry);
    built->tags = std::make_unique<BiomeTagLoader>(jar_.readText);
    built->biomeSpawns = std::make_unique<RealBiomeSpawns>(RealBiomeSpawns::Jar{.readText = jar_.readText});
    built->equipment = std::make_unique<SpawnEquipmentFinalizer>(
        SpawnEquipmentJar{.entriesUnder = nullptr, .readText = jar_.readText});

    const std::int64_t biomeZoomSeed = obfuscateBiomeSeed(seed);
    // parity: `noise_settings/nether.json` has `legacy_random_source = true`.
    random::RandomSource legacy = random::RandomSource::legacy(seed);
    const densityfunction::Function base3d =
        densityfunction::functions::blendedNoise(legacy, 0.25, 0.375, 80.0, 60.0, 8.0);
    // parity: the Nether's noises are legacy-seeded per id, and its vegetation noise takes seed + 1.
    const auto netherNoise = [this](std::string_view id,
                                    std::int64_t noiseSeed) -> std::optional<server::world::noise::NormalNoise> {
        const std::optional<std::string> json =
            jar_.readText ? jar_.readText("data/minecraft/worldgen/noise/" + std::string{id} + ".json") : std::nullopt;
        const std::optional<server::world::noise::NormalNoise::Parameters> parameters =
            json.has_value() ? noiseParametersJson::parse(*json) : std::nullopt;
        if (!parameters.has_value()) {
            return std::nullopt;
        }
        random::RandomSource source = random::RandomSource::legacy(noiseSeed);
        return server::world::noise::NormalNoise::createLegacyNetherBiome(source, parameters->firstOctave,
                                                                          parameters->amplitudes);
    };
    // Held here so the router's own lookups keep pointing at a live noise.
    auto netherNoises = std::make_shared<
        std::map<std::string, std::shared_ptr<const server::world::noise::NormalNoise>, std::less<>>>();
    if (!netherNoise("nether/temperature", seed).has_value()) {
        logger_.warn([] {
            return std::string{"No Nether worldgen noise in the server jar, so no Nether terrain can be generated."};
        });
        return {};
    }
    built->netherRouter = std::make_unique<terrain::NetherNoiseRouter>(terrain::noiseRouterData::nether(
        [netherNoises, netherNoise,
         seed](std::string_view id) -> std::shared_ptr<const server::world::noise::NormalNoise> {
            if (const auto found = netherNoises->find(id); found != netherNoises->end()) {
                return found->second;
            }
            std::optional<server::world::noise::NormalNoise> made =
                netherNoise(id, seed + (id == "nether/vegetation" ? 1 : 0));
            std::shared_ptr<const server::world::noise::NormalNoise> held =
                made.has_value() ? std::make_shared<const server::world::noise::NormalNoise>(std::move(*made))
                                 : nullptr;
            netherNoises->emplace(std::string{id}, held);
            return held;
        },
        base3d));
    built->netherBiomes = std::make_unique<terrain::NetherBiomeSource>(built->netherRouter->temperature,
                                                                       built->netherRouter->vegetation, biomeZoomSeed);
    built->netherSurface = std::make_unique<NetherSurfaceEnvironment>(
        jar_.readText, noiseParametersJson::parse, random::RandomSource::legacy(seed).forkPositional(),
        *built->netherRouter, *built->netherBiomes, biomeZoomSeed);
    built->netherStructures = std::make_unique<terrain::NetherStructureGenerator>(
        *built->netherRouter, *built->netherBiomes, built->loader->templateLoader(),
        [&loader = *built->loader](std::string_view name) { return loader.pool(name); });
    built->mineshafts = std::make_unique<terrain::MineshaftStructureGenerator>(
        [&surface = *built->netherSurface](std::int32_t x, std::int32_t y, std::int32_t z) {
            return surface.biomeAt(x, y, z);
        },
        biomesOf(*built->tags, "minecraft:has_structure/mineshaft"),
        biomesOf(*built->tags, "minecraft:has_structure/mineshaft_mesa"));

    Built& world = *built;
    // The Nether's springs, patches and blobs read the block next to them, so decoration needs the
    // neighbours' undecorated terrain, exactly like the Overworld's.
    built->neighbours =
        std::make_unique<terrain::NeighbourTerrainCache>([&world](std::int32_t neighbourX, std::int32_t neighbourZ) {
            return terrain::dimensionTerrainGenerator::netherTerrain(*world.netherRouter, *world.netherBiomes,
                                                                     neighbourX, neighbourZ, world.netherSurface.get(),
                                                                     world.seed, world.netherStructures.get());
        });
    Generators answer;
    answer.useQueuedWrites = [&world](terrain::NeighbourTerrainCache::QueuedAt queuedAt) {
        if (world.neighbours != nullptr) {
            world.neighbours->setQueuedAt(std::move(queuedAt));
        }
    };
    answer.generate = [&world](std::int32_t chunkX, std::int32_t chunkZ, std::int64_t gameTime,
                               const std::string& difficulty,
                               const std::function<void(server::world::Chunk&)>& beforeDecoration) {
        server::world::Chunk chunk = terrain::dimensionTerrainGenerator::nether(
            *world.netherRouter, *world.netherBiomes, chunkX, chunkZ, world.netherSurface.get(), world.seed,
            world.netherStructures.get(), world.neighbours.get(), beforeDecoration);
        if (world.spawnMobs) {
            const std::expected<Identifier, wire::common::IdentifierError> biome =
                Identifier::parse(world.netherBiomes->biomeAt(chunkX << 4, chunk.verticalBounds().maxY(), chunkZ << 4));
            if (biome.has_value()) {
                random::RandomSource random = random::RandomSource::legacy(0);
                static_cast<void>(random::setDecorationSeed(random, world.seed, chunkX << 4, chunkZ << 4));
                static_cast<void>(originalMobSpawner::spawnInto(chunk, world.biomeSpawns->listFor(*biome, "creature"),
                                                                world.biomeSpawns->creatureProbabilityFor(*biome),
                                                                random,
                                                                {.hasCeiling = true,
                                                                 .gameTime = gameTime,
                                                                 .difficulty = difficulty,
                                                                 .equipment = world.equipment.get()}));
            }
        }
        return chunk;
    };
    built_.push_back(std::move(built));
    return answer;
}

VanillaWorldGeneratorFactory::Generators VanillaWorldGeneratorFactory::end(std::int64_t seed) {
    auto built = std::make_unique<Built>();
    built->seed = seed;
    built->templates = std::make_unique<StructureTemplates>(
        StructureTemplates::Jar{.readEntry = jar_.readEntry, .dataIds = jar_.dataIds});
    built->loader = std::make_unique<VillageDataLoader>(*built->templates, jar_.readEntry);

    const std::int64_t biomeZoomSeed = obfuscateBiomeSeed(seed);
    // parity: `noise_settings/end.json` has `legacy_random_source = true`.
    random::RandomSource legacy = random::RandomSource::legacy(seed);
    const densityfunction::Function base3d =
        densityfunction::functions::blendedNoise(legacy, 0.25, 0.25, 80.0, 160.0, 4.0);
    built->endRouter = std::make_unique<terrain::EndNoiseRouter>(terrain::noiseRouterData::end(base3d, seed));
    built->endBiomes = std::make_unique<terrain::TheEndBiomeSource>(built->endRouter->erosion, biomeZoomSeed);
    built->endCities = std::make_unique<terrain::EndCityStructureGenerator>(*built->endRouter, *built->endBiomes,
                                                                            built->loader->templateLoader());

    Built& world = *built;
    Generators answer;
    answer.useQueuedWrites = [&world](terrain::NeighbourTerrainCache::QueuedAt queuedAt) {
        if (world.neighbours != nullptr) {
            world.neighbours->setQueuedAt(std::move(queuedAt));
        }
    };
    answer.generate = [&world](std::int32_t chunkX, std::int32_t chunkZ, std::int64_t, const std::string&,
                               const std::function<void(server::world::Chunk&)>& beforeDecoration) {
        return terrain::dimensionTerrainGenerator::end(*world.endRouter, *world.endBiomes, chunkX, chunkZ, world.seed,
                                                       world.endCities.get(), beforeDecoration);
    };
    built_.push_back(std::move(built));
    return answer;
}

} // namespace basalt::server::runtime::world
