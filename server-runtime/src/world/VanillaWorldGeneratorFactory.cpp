#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/runtime/world/BiomeTagLoader.hpp>
#include <basalt/server/runtime/world/BiomeZoomSeed.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/NetherSurfaceEnvironment.hpp>
#include <basalt/server/runtime/world/OriginalMobSpawner.hpp>
#include <basalt/server/runtime/world/RealBiomeSpawns.hpp>
#include <basalt/server/runtime/world/RealSurfaceEnvironment.hpp>
#include <basalt/server/runtime/world/RuinedPortalVariants.hpp>
#include <basalt/server/runtime/world/SpawnEquipment.hpp>
#include <basalt/server/runtime/world/VanillaWorldGeneratorFactory.hpp>
#include <basalt/server/runtime/world/VillageDataLoader.hpp>
#include <basalt/server/world/SimpleWorldGenerators.hpp>
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
#include <basalt/server/world/terrain/ProtoRegion.hpp>
#include <basalt/server/world/terrain/RealTerrainGenerator.hpp>
#include <basalt/server/world/terrain/RuinedPortalStructureGenerator.hpp>
#include <basalt/server/world/terrain/ShipwreckStructureGenerator.hpp>
#include <basalt/server/world/terrain/StrongholdStructureGenerator.hpp>
#include <basalt/server/world/terrain/SwampHutStructureGenerator.hpp>
#include <basalt/server/world/terrain/TheEndBiomeSource.hpp>
#include <basalt/server/world/terrain/VillageStructureGenerator.hpp>
#include <basalt/server/world/terrain/WoodlandMansionStructureGenerator.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::world {
namespace {

namespace densityfunction = server::world::densityfunction;
namespace structure = server::world::structure;
namespace terrain = server::world::terrain;

using wire::common::Identifier;

[[nodiscard]] std::set<std::string, std::less<>> biomesOf(BiomeTagLoader& tags, const std::string& name) {
    const std::set<std::string>& members = tags.biomesIn(name);
    return {members.begin(), members.end()};
}

/// parity: the 26.3 `abandoned_camp` structure set's eighteen entries in file order.
[[nodiscard]] std::vector<terrain::VillageStructureGenerator::Variant> campVariants(BiomeTagLoader& tags) {
    static constexpr std::array<std::string_view, 18> kFileOrder{"bamboo_jungle",
                                                                 "birch_forest",
                                                                 "cherry_grove",
                                                                 "dappled_forest",
                                                                 "flower_forest",
                                                                 "forest",
                                                                 "meadow",
                                                                 "old_growth_pine_taiga",
                                                                 "old_growth_birch_forest",
                                                                 "old_growth_spruce_taiga",
                                                                 "pale_garden",
                                                                 "savanna",
                                                                 "snowy_taiga",
                                                                 "sparse_jungle",
                                                                 "swamp",
                                                                 "taiga",
                                                                 "windswept_forest",
                                                                 "wooded_badlands"};
    std::vector<std::string_view> registryOrder{kFileOrder.begin(), kFileOrder.end()};
    std::ranges::sort(registryOrder);
    std::vector<terrain::VillageStructureGenerator::Variant> variants;
    for (const std::string_view biome : kFileOrder) {
        const std::string name = "abandoned_camp_" + std::string{biome};
        std::set<std::string, std::less<>> biomes;
        for (const std::string& member : tags.biomesIn("minecraft:has_structure/" + name)) {
            const std::size_t colon = member.find(':');
            biomes.insert(colon == std::string::npos ? member : member.substr(colon + 1));
        }
        variants.push_back(terrain::VillageStructureGenerator::Variant{
            .id = Identifier::vanilla(name),
            .startPool = "minecraft:abandoned_camp/tent/" + std::string{biome},
            .biomes = std::move(biomes),
            .structureIndex =
                static_cast<std::int32_t>(std::ranges::find(registryOrder, biome) - registryOrder.begin())});
    }
    return variants;
}

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

template <typename Generator, typename BoundsFn, typename PlaceFn, typename... Bound>
[[nodiscard]] StructurePlacer makePlacer(Identifier id, Generator* generator, BoundsFn bounds, PlaceFn place,
                                         Bound... bound) {
    return StructurePlacer{
        .id = std::move(id),
        .boundsAt = [generator, bounds,
                     bound...](std::int64_t seed, std::int32_t chunkX,
                               std::int32_t chunkZ) { return (generator->*bounds)(seed, chunkX, chunkZ, bound...); },
        .place =
            [generator, place, bound...](server::world::Chunk& chunk, std::int64_t seed, std::int32_t startChunkX,
                                         std::int32_t startChunkZ) {
                (generator->*place)(chunk, seed, startChunkX, startChunkZ, bound...);
            }};
}

struct VanillaWorldGeneratorFactory::Built {
    std::unique_ptr<StructureTemplates> templates;
    std::unique_ptr<VillageDataLoader> loader;
    std::unique_ptr<BiomeTagLoader> tags;
    std::unique_ptr<RealBiomeSpawns> biomeSpawns;
    std::unique_ptr<terrain::NoiseRouter> overworldRouter;
    std::unique_ptr<terrain::NoiseRouter> netherRouter;
    std::unique_ptr<terrain::NoiseRouter> endRouter;
    std::unique_ptr<terrain::NetherBiomeSource> netherBiomes;
    std::unique_ptr<terrain::TheEndBiomeSource> endBiomes;
    std::unique_ptr<RealSurfaceEnvironment> overworldSurface;
    std::unique_ptr<NetherSurfaceEnvironment> netherSurface;
    std::unique_ptr<terrain::NoiseColumnHeights> columns;
    std::unique_ptr<terrain::VillageStructureGenerator> village;
    std::unique_ptr<terrain::VillageStructureGenerator> camps;
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

server::world::densityfunction::WorldgenReader VanillaWorldGeneratorFactory::worldgenReader() const {
    return [readText = jar_.readText](std::string_view path) -> std::optional<std::string> {
        return readText ? readText("data/" + std::string{path} + ".json") : std::nullopt;
    };
}

VanillaWorldGeneratorFactory::Generators
VanillaWorldGeneratorFactory::create(const config::WorldDeclaration& declaration, const WorldMetadata& metadata) {
    const bool spawnMobs = metadata.gameRules.get(gameRules::kSpawnMobs);
    switch (declaration.dimension) {
    case config::WorldDimension::Overworld:
        if (declaration.type == config::WorldType::Flat) {
            return flat(metadata.seed, spawnMobs);
        }
        // parity: the `world_preset`s differ only in the noise settings their overworld names.
        return overworld(metadata.seed, spawnMobs,
                         declaration.type == config::WorldType::Amplified     ? "minecraft:amplified"
                         : declaration.type == config::WorldType::LargeBiomes ? "minecraft:large_biomes"
                                                                              : "minecraft:overworld");
    case config::WorldDimension::Nether:
        return nether(metadata.seed, spawnMobs);
    case config::WorldDimension::End:
        return end(metadata.seed);
    }
    return {};
}

VanillaWorldGeneratorFactory::Generators VanillaWorldGeneratorFactory::overworld(std::int64_t seed, bool spawnMobs,
                                                                                 std::string_view settings) {
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
    // parity: the overworld's shape is the jar's own `noise_settings`.
    std::expected<terrain::NoiseRouter, std::string> router =
        terrain::noiseRouterData::load(settings, seed, worldgenReader());
    if (!router.has_value()) {
        logger_.warn([&router] { return "No normal terrain can be generated: " + router.error(); });
        return {};
    }
    built->overworldRouter = std::make_unique<terrain::NoiseRouter>(std::move(*router));
    built->overworldSurface = std::make_unique<RealSurfaceEnvironment>(*built->overworldRouter, biomeZoomSeed);

    const RealSurfaceEnvironment& surface = *built->overworldSurface;
    const auto blockBiome = [&surface](std::int32_t x, std::int32_t y, std::int32_t z) -> const std::string& {
        return surface.biomeAt(x, y, z);
    };
    const auto noiseBiome = [&surface](std::int32_t x, std::int32_t y, std::int32_t z) -> const std::string& {
        return surface.noiseBiomeAt(x, y, z);
    };
    const auto quartBiome = [&surface](std::int32_t x, std::int32_t y, std::int32_t z) -> const std::string& {
        return surface.noiseBiomeAt(x >> 2, y >> 2, z >> 2);
    };
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
    built->camps = std::make_unique<terrain::VillageStructureGenerator>(
        *built->overworldRouter, structure::StructurePlacementGrid{37, 8, 91231127}, quartBiome, 2, 80, poolLookup,
        templateOf, campVariants(tags));
    const terrain::NoiseColumnHeights& columns = *built->columns;
    const auto surfaceHeight = [&columns](std::int32_t x, std::int32_t z) { return columns.surfaceHeight(x, z); };
    const auto oceanFloor = [&columns](std::int32_t x, std::int32_t z) { return columns.oceanFloorHeight(x, z); };
    const auto worldHeight = [holder = built.get()](std::int32_t x, std::int32_t z, server::world::HeightmapType type) {
        return holder->structureHeights->worldgenHeightAt(x, z, type);
    };

    built->mineshafts = std::make_unique<terrain::MineshaftStructureGenerator>(
        blockBiome, biomesOf(tags, "minecraft:has_structure/mineshaft"),
        biomesOf(tags, "minecraft:has_structure/mineshaft_mesa"), surfaceHeight, 63, quartBiome);
    built->ruinedPortals = std::make_unique<terrain::RuinedPortalStructureGenerator>(
        ruinedPortalVariants::overworld(tags), templateOf, blockBiome, surfaceHeight, oceanFloor,
        [&columns](std::int32_t x, std::int32_t y, std::int32_t z) { return columns.isSolid(x, y, z); });
    built->swampHuts = std::make_unique<terrain::SwampHutStructureGenerator>(quartBiome, surfaceHeight);
    built->desertPyramids = std::make_unique<terrain::DesertPyramidStructureGenerator>(quartBiome, surfaceHeight);
    built->jungleTemples = std::make_unique<terrain::JungleTempleStructureGenerator>(
        quartBiome, biomesOf(tags, "minecraft:has_structure/jungle_temple"), surfaceHeight);
    built->mansions = std::make_unique<terrain::WoodlandMansionStructureGenerator>(
        quartBiome, biomesOf(tags, "minecraft:has_structure/woodland_mansion"), templateOf, surfaceHeight);
    built->oceanMonuments = std::make_unique<terrain::OceanMonumentStructureGenerator>(
        noiseBiome, biomesOf(tags, "minecraft:has_structure/ocean_monument"),
        biomesOf(tags, "minecraft:required_ocean_monument_surrounding"), oceanFloor);
    built->strongholds = std::make_unique<terrain::StrongholdStructureGenerator>(
        structure::ConcentricRingsPlacement{32, 3, 128, biomesOf(tags, "minecraft:stronghold_biased_to"), noiseBiome},
        seed, noiseBiome, biomesOf(tags, "minecraft:has_structure/stronghold"));
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
        ancientCitySettings(), *built->overworldRouter, structure::StructurePlacementGrid{24, 8, 20083232}, quartBiome,
        biomesOf(tags, "minecraft:has_structure/ancient_city"), poolLookup, templateOf);
    built->trialChambers = std::make_unique<terrain::JigsawStructureGenerator>(
        trialChamberSettings(), *built->overworldRouter, structure::StructurePlacementGrid{34, 12, 94251327},
        quartBiome, biomesOf(tags, "minecraft:has_structure/trial_chambers"), poolLookup, templateOf);

    built->generators = terrain::TerrainStructureGenerators{.village = built->village.get(),
                                                            .camp = built->camps.get(),
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

    Built& world = *built;
    built->structureHeights =
        std::make_unique<terrain::NeighbourTerrainCache>([&world](std::int32_t neighbourX, std::int32_t neighbourZ) {
            terrain::realTerrainGenerator::GeneratedTerrain terrain = terrain::realTerrainGenerator::generateTerrain(
                *world.overworldRouter, *world.overworldSurface, neighbourX, neighbourZ, world.seed, world.generators);
            return std::move(terrain.chunk());
        });

    Generators answer;
    answer.villages = built->village.get();
    answer.camps = built->camps.get();
    const auto onGrid = [](const structure::StructurePlacementGrid& grid, auto startsAt) {
        return [&grid, startsAt](std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ, std::int32_t cells,
                                 const ClaimedStart& skip) {
            return grid.locate(seed, chunkX, chunkZ, cells, [&](std::int32_t x, std::int32_t z) {
                return startsAt(seed, x, z) && !(skip && skip(x, z));
            });
        };
    };
    for (terrain::JigsawStructureGenerator* jigsaw : {built->pillagerOutposts.get(), built->trailRuins.get(),
                                                      built->ancientCities.get(), built->trialChambers.get()}) {
        answer.locators.push_back(
            StructureLocator{.id = Identifier::vanilla(jigsaw->name()),
                             .locate = [jigsaw](std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ,
                                                std::int32_t cells, const ClaimedStart& skip) {
                                 return jigsaw->locate(seed, chunkX, chunkZ, cells, skip);
                             }});
    }
    const terrain::MineshaftStructureGenerator* mineshafts = built->mineshafts.get();
    for (const auto& [name, type] : {std::pair{"mineshaft", structure::MineshaftType::Normal},
                                     std::pair{"mineshaft_mesa", structure::MineshaftType::Mesa}}) {
        answer.locators.push_back(StructureLocator{
            .id = Identifier::vanilla(name),
            .locate = onGrid(mineshafts->grid(), [mineshafts, type](std::int64_t seed, std::int32_t x, std::int32_t z) {
                return mineshafts->typeAt(seed, x, z) == type;
            })});
    }
    const terrain::SwampHutStructureGenerator* huts = built->swampHuts.get();
    answer.locators.push_back(StructureLocator{
        .id = Identifier::vanilla(std::string{terrain::SwampHutStructureGenerator::kName}),
        .locate =
            onGrid(terrain::SwampHutStructureGenerator::grid(),
                   [huts](std::int64_t seed, std::int32_t x, std::int32_t z) { return huts->startsAt(seed, x, z); })});
    const terrain::DesertPyramidStructureGenerator* pyramids = built->desertPyramids.get();
    answer.locators.push_back(
        StructureLocator{.id = Identifier::vanilla(std::string{terrain::DesertPyramidStructureGenerator::kName}),
                         .locate = onGrid(terrain::DesertPyramidStructureGenerator::grid(),
                                          [pyramids](std::int64_t seed, std::int32_t x, std::int32_t z) {
                                              return pyramids->startsAt(seed, x, z);
                                          })});
    const terrain::JungleTempleStructureGenerator* temples = built->jungleTemples.get();
    answer.locators.push_back(
        StructureLocator{.id = Identifier::vanilla(std::string{terrain::JungleTempleStructureGenerator::kName}),
                         .locate = onGrid(terrain::JungleTempleStructureGenerator::grid(),
                                          [temples](std::int64_t seed, std::int32_t x, std::int32_t z) {
                                              return temples->startsAt(seed, x, z);
                                          })});
    const terrain::WoodlandMansionStructureGenerator* mansions = built->mansions.get();
    answer.locators.push_back(
        StructureLocator{.id = Identifier::vanilla(std::string{terrain::WoodlandMansionStructureGenerator::kName}),
                         .locate = onGrid(terrain::WoodlandMansionStructureGenerator::grid(),
                                          [mansions](std::int64_t seed, std::int32_t x, std::int32_t z) {
                                              return mansions->startsAt(seed, x, z);
                                          })});
    const terrain::OceanMonumentStructureGenerator* monuments = built->oceanMonuments.get();
    answer.locators.push_back(
        StructureLocator{.id = Identifier::vanilla(std::string{terrain::OceanMonumentStructureGenerator::kName}),
                         .locate = onGrid(terrain::OceanMonumentStructureGenerator::grid(),
                                          [monuments](std::int64_t seed, std::int32_t x, std::int32_t z) {
                                              return monuments->startsAt(seed, x, z);
                                          })});
    const terrain::BuriedTreasureStructureGenerator* treasures = built->buriedTreasures.get();
    answer.locators.push_back(
        StructureLocator{.id = Identifier::vanilla(std::string{terrain::BuriedTreasureStructureGenerator::kName}),
                         .locate = onGrid(terrain::BuriedTreasureStructureGenerator::grid(),
                                          [treasures](std::int64_t seed, std::int32_t x, std::int32_t z) {
                                              return treasures->startsAt(seed, x, z);
                                          })});
    const terrain::OceanRuinStructureGenerator* ruins = built->oceanRuins.get();
    for (const auto& [name, warm] : {std::pair{"ocean_ruin_cold", false}, std::pair{"ocean_ruin_warm", true}}) {
        answer.locators.push_back(
            StructureLocator{.id = Identifier::vanilla(name),
                             .locate = onGrid(terrain::OceanRuinStructureGenerator::grid(),
                                              [ruins, warm](std::int64_t seed, std::int32_t x, std::int32_t z) {
                                                  return ruins->startsAt(seed, x, z, warm);
                                              })});
    }
    const terrain::ShipwreckStructureGenerator* wrecks = built->shipwrecks.get();
    for (const auto& [name, beached] : {std::pair{"shipwreck", false}, std::pair{"shipwreck_beached", true}}) {
        answer.locators.push_back(
            StructureLocator{.id = Identifier::vanilla(name),
                             .locate = onGrid(terrain::ShipwreckStructureGenerator::grid(),
                                              [wrecks, beached](std::int64_t seed, std::int32_t x, std::int32_t z) {
                                                  return wrecks->startsAt(seed, x, z, beached);
                                              })});
    }
    answer.strongholds = built->strongholds.get();
    answer.placers.push_back(makePlacer(Identifier::vanilla(std::string{terrain::StrongholdStructureGenerator::kName}),
                                        built->strongholds.get(), &terrain::StrongholdStructureGenerator::placeBoundsAt,
                                        &terrain::StrongholdStructureGenerator::placeAt));
    answer.placers.push_back(makePlacer(Identifier::vanilla(std::string{terrain::SwampHutStructureGenerator::kName}),
                                        built->swampHuts.get(), &terrain::SwampHutStructureGenerator::placeBoundsAt,
                                        &terrain::SwampHutStructureGenerator::placeAt));
    answer.placers.push_back(makePlacer(
        Identifier::vanilla(std::string{terrain::DesertPyramidStructureGenerator::kName}), built->desertPyramids.get(),
        &terrain::DesertPyramidStructureGenerator::placeBoundsAt, &terrain::DesertPyramidStructureGenerator::placeAt));
    answer.placers.push_back(makePlacer(
        Identifier::vanilla(std::string{terrain::JungleTempleStructureGenerator::kName}), built->jungleTemples.get(),
        &terrain::JungleTempleStructureGenerator::placeBoundsAt, &terrain::JungleTempleStructureGenerator::placeAt));
    answer.placers.push_back(
        makePlacer(Identifier::vanilla(std::string{terrain::WoodlandMansionStructureGenerator::kName}),
                   built->mansions.get(), &terrain::WoodlandMansionStructureGenerator::placeBoundsAt,
                   &terrain::WoodlandMansionStructureGenerator::placeAt));
    answer.placers.push_back(makePlacer(
        Identifier::vanilla(std::string{terrain::OceanMonumentStructureGenerator::kName}), built->oceanMonuments.get(),
        &terrain::OceanMonumentStructureGenerator::placeBoundsAt, &terrain::OceanMonumentStructureGenerator::placeAt));
    answer.placers.push_back(
        makePlacer(Identifier::vanilla(std::string{terrain::BuriedTreasureStructureGenerator::kName}),
                   built->buriedTreasures.get(), &terrain::BuriedTreasureStructureGenerator::placeBoundsAt,
                   &terrain::BuriedTreasureStructureGenerator::placeAt));
    answer.placers.push_back(makePlacer(Identifier::vanilla(std::string{terrain::IglooStructureGenerator::kName}),
                                        built->igloos.get(), &terrain::IglooStructureGenerator::placeBoundsAt,
                                        &terrain::IglooStructureGenerator::placeAt));
    for (const auto& [name, type] : {std::pair{"mineshaft", structure::MineshaftType::Normal},
                                     std::pair{"mineshaft_mesa", structure::MineshaftType::Mesa}}) {
        answer.placers.push_back(makePlacer(Identifier::vanilla(name), built->mineshafts.get(),
                                            &terrain::MineshaftStructureGenerator::placeBoundsAt,
                                            &terrain::MineshaftStructureGenerator::placeAt, type));
    }
    for (const auto& [name, warm] : {std::pair{"ocean_ruin_cold", false}, std::pair{"ocean_ruin_warm", true}}) {
        answer.placers.push_back(makePlacer(Identifier::vanilla(name), built->oceanRuins.get(),
                                            &terrain::OceanRuinStructureGenerator::placeBoundsAt,
                                            &terrain::OceanRuinStructureGenerator::placeAt, warm));
    }
    for (const auto& [name, beached] : {std::pair{"shipwreck", false}, std::pair{"shipwreck_beached", true}}) {
        answer.placers.push_back(makePlacer(Identifier::vanilla(name), built->shipwrecks.get(),
                                            &terrain::ShipwreckStructureGenerator::placeBoundsAt,
                                            &terrain::ShipwreckStructureGenerator::placeAt, beached));
    }
    for (const terrain::JigsawStructureGenerator* jigsaw : {built->pillagerOutposts.get(), built->trailRuins.get(),
                                                            built->ancientCities.get(), built->trialChambers.get()}) {
        answer.placers.push_back(
            makePlacer(Identifier::vanilla(jigsaw->name()), const_cast<terrain::JigsawStructureGenerator*>(jigsaw),
                       &terrain::JigsawStructureGenerator::placeBoundsAt, &terrain::JigsawStructureGenerator::placeAt));
    }
    for (const terrain::RuinedPortalVariant& variant : ruinedPortalVariants::overworld(tags)) {
        if (variant.name == "ruined_portal_nether") {
            continue;
        }
        answer.placers.push_back(makePlacer(Identifier::vanilla(variant.name), built->ruinedPortals.get(),
                                            &terrain::RuinedPortalStructureGenerator::placeBoundsAt,
                                            &terrain::RuinedPortalStructureGenerator::placeAt,
                                            std::string{variant.name}));
    }
    answer.pipeline.terrain = [&world](std::int32_t chunkX, std::int32_t chunkZ) {
        return terrain::realTerrainGenerator::generateTerrain(*world.overworldRouter, *world.overworldSurface, chunkX,
                                                              chunkZ, world.seed, world.generators);
    };
    answer.pipeline.adopt = [&world](server::world::Chunk chunk) {
        return terrain::realTerrainGenerator::adopt(*world.overworldSurface, std::move(chunk));
    };
    answer.pipeline.decorate = [&world](terrain::realTerrainGenerator::GeneratedTerrain& generated,
                                        terrain::ProtoRegion& region) {
        const server::world::Chunk::RegionScope scope{generated.chunk(), region};
        terrain::realTerrainGenerator::decorate(generated, world.seed, &region, world.generators,
                                                world.loader->templateLoader());
    };
    answer.spawnMobs = [&world](server::world::Chunk& chunk, std::int64_t gameTime, const std::string& difficulty) {
        if (!world.spawnMobs) {
            return;
        }
        const std::int32_t chunkX = chunk.chunkX();
        const std::int32_t chunkZ = chunk.chunkZ();
        const std::expected<Identifier, wire::common::IdentifierError> biome =
            Identifier::parse(world.overworldSurface->biomeAt(chunkX << 4, chunk.verticalBounds().maxY(), chunkZ << 4));
        if (biome.has_value()) {
            random::RandomSource random = random::RandomSource::legacy(0);
            static_cast<void>(random::setDecorationSeed(random, world.seed, chunkX << 4, chunkZ << 4));
            static_cast<void>(originalMobSpawner::spawnInto(
                chunk, world.biomeSpawns->listFor(*biome, "creature"),
                world.biomeSpawns->creatureProbabilityFor(*biome), random,
                {.gameTime = gameTime, .difficulty = difficulty, .equipment = world.equipment.get()}));
        }
    };
    answer.spawnChunk = [&world] {
        const terrain::NoiseRouter& router = *world.overworldRouter;
        const auto [blockX, blockZ] = server::world::climate::spawnFinder::findSpawnPosition(
            server::world::climate::Sampler{*router.state, router.temperature, router.vegetation, router.continents,
                                            router.erosion, router.depth, router.ridges});
        return std::pair{blockX >> 4, blockZ >> 4};
    };
    built_.push_back(std::move(built));
    return answer;
}

VanillaWorldGeneratorFactory::Generators VanillaWorldGeneratorFactory::flat(std::int64_t seed, bool spawnMobs) {
    auto built = std::make_unique<Built>();
    built->seed = seed;
    built->spawnMobs = spawnMobs;
    built->templates = std::make_unique<StructureTemplates>(
        StructureTemplates::Jar{.readEntry = jar_.readEntry, .dataIds = jar_.dataIds});
    built->loader = std::make_unique<VillageDataLoader>(*built->templates, jar_.readEntry);
    built->biomeSpawns = std::make_unique<RealBiomeSpawns>(RealBiomeSpawns::Jar{.readText = jar_.readText});
    built->equipment = std::make_unique<SpawnEquipmentFinalizer>(
        SpawnEquipmentJar{.entriesUnder = nullptr, .readText = jar_.readText});
    StructureTemplates& templates = *built->templates;
    const auto templateOf = [&templates](std::string_view name) -> const structure::StructureTemplate* {
        const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(name);
        return id.has_value() ? templates.find(*id) : nullptr;
    };
    const auto poolLookup = [&loader = *built->loader](std::string_view name) { return loader.pool(name); };
    static const std::string kPlains = "minecraft:plains";
    built->village = std::make_unique<terrain::VillageStructureGenerator>(
        terrain::NoiseRouter{}, structure::StructurePlacementGrid{34, 8, 10387312},
        [](std::int32_t, std::int32_t, std::int32_t) { return kPlains; }, 6, 80, poolLookup, templateOf);
    // parity: `FlatLevelSource.getBaseHeight`, one above the top of the four default layers.
    built->village->useFlatSurface(server::world::kOverworldHeight.minY() + 4);
    built->tags = std::make_unique<BiomeTagLoader>(jar_.readText);
    BiomeTagLoader& tags = *built->tags;
    const auto plainsEverywhere = [](std::int32_t, std::int32_t, std::int32_t) -> const std::string& {
        return kPlains;
    };
    // parity: `FlatLevelSource` answers sea level -63 and min y 0, so strongholds sit on top of the layers.
    built->strongholds = std::make_unique<terrain::StrongholdStructureGenerator>(
        structure::ConcentricRingsPlacement{32, 3, 128, biomesOf(tags, "minecraft:stronghold_biased_to"),
                                            plainsEverywhere},
        seed, plainsEverywhere, biomesOf(tags, "minecraft:has_structure/stronghold"), -63, 0);
    built->generators =
        terrain::TerrainStructureGenerators{.village = built->village.get(), .stronghold = built->strongholds.get()};

    Built& world = *built;
    Generators answer;
    answer.villages = built->village.get();
    answer.strongholds = built->strongholds.get();
    answer.pipeline.terrain = [](std::int32_t chunkX, std::int32_t chunkZ) {
        return terrain::realTerrainGenerator::GeneratedTerrain{
            server::world::flatWorldGenerator::generate(chunkX, chunkZ), nullptr, nullptr};
    };
    answer.pipeline.adopt = [](server::world::Chunk chunk) {
        return terrain::realTerrainGenerator::GeneratedTerrain{std::move(chunk), nullptr, nullptr};
    };
    // parity: `FlatLevelGeneratorSettings.adjustGenerationSettings` with `features` off keeps only the.
    answer.pipeline.decorate = [&world](terrain::realTerrainGenerator::GeneratedTerrain& generated,
                                        terrain::ProtoRegion& region) {
        server::world::Chunk& chunk = generated.chunk();
        const server::world::Chunk::RegionScope scope{chunk, region};
        // Both are `surface_structures`, each on its own feature seed, in the overworld's order.
        world.village->generateInto(chunk, chunk.chunkX(), chunk.chunkZ(), world.seed);
        world.strongholds->generateInto(chunk, world.seed);
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
    std::expected<terrain::NoiseRouter, std::string> router =
        terrain::noiseRouterData::load("minecraft:nether", seed, worldgenReader());
    if (!router.has_value()) {
        logger_.warn([&router] { return "No Nether terrain can be generated: " + router.error(); });
        return {};
    }
    built->netherRouter = std::make_unique<terrain::NoiseRouter>(std::move(*router));
    built->netherBiomes = std::make_unique<terrain::NetherBiomeSource>(*built->netherRouter, biomeZoomSeed);
    built->netherSurface = std::make_unique<NetherSurfaceEnvironment>(*built->netherBiomes, biomeZoomSeed);
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
    Generators answer;
    answer.pipeline.terrain = [&world](std::int32_t chunkX, std::int32_t chunkZ) {
        return terrain::realTerrainGenerator::GeneratedTerrain{
            terrain::dimensionTerrainGenerator::netherTerrain(*world.netherRouter, *world.netherBiomes, chunkX, chunkZ,
                                                              world.seed, world.netherStructures.get()),
            nullptr, nullptr};
    };
    answer.pipeline.adopt = [](server::world::Chunk chunk) {
        return terrain::realTerrainGenerator::GeneratedTerrain{std::move(chunk), nullptr, nullptr};
    };
    answer.pipeline.decorate = [&world](terrain::realTerrainGenerator::GeneratedTerrain& generated,
                                        terrain::ProtoRegion& region) {
        const server::world::Chunk::RegionScope scope{generated.chunk(), region};
        terrain::dimensionTerrainGenerator::decorateNether(generated.chunk(), *world.netherBiomes, world.seed, &region);
    };
    answer.spawnMobs = [&world](server::world::Chunk& chunk, std::int64_t gameTime, const std::string& difficulty) {
        if (!world.spawnMobs) {
            return;
        }
        const std::int32_t chunkX = chunk.chunkX();
        const std::int32_t chunkZ = chunk.chunkZ();
        const std::expected<Identifier, wire::common::IdentifierError> biome =
            Identifier::parse(world.netherBiomes->biomeAt(chunkX << 4, chunk.verticalBounds().maxY(), chunkZ << 4));
        if (biome.has_value()) {
            random::RandomSource random = random::RandomSource::legacy(0);
            static_cast<void>(random::setDecorationSeed(random, world.seed, chunkX << 4, chunkZ << 4));
            static_cast<void>(originalMobSpawner::spawnInto(chunk, world.biomeSpawns->listFor(*biome, "creature"),
                                                            world.biomeSpawns->creatureProbabilityFor(*biome), random,
                                                            {.hasCeiling = true,
                                                             .gameTime = gameTime,
                                                             .difficulty = difficulty,
                                                             .equipment = world.equipment.get()}));
        }
    };
    answer.placers.push_back(makePlacer(Identifier::vanilla("fortress"), built->netherStructures.get(),
                                        &terrain::NetherStructureGenerator::placeBoundsAt,
                                        &terrain::NetherStructureGenerator::placeAt,
                                        terrain::NetherPlacedStructure::Fortress));
    answer.placers.push_back(makePlacer(Identifier::vanilla("bastion_remnant"), built->netherStructures.get(),
                                        &terrain::NetherStructureGenerator::placeBoundsAt,
                                        &terrain::NetherStructureGenerator::placeAt,
                                        terrain::NetherPlacedStructure::Bastion));
    answer.placers.push_back(makePlacer(Identifier::vanilla("nether_fossil"), built->netherStructures.get(),
                                        &terrain::NetherStructureGenerator::placeBoundsAt,
                                        &terrain::NetherStructureGenerator::placeAt,
                                        terrain::NetherPlacedStructure::Fossil));
    answer.placers.push_back(makePlacer(Identifier::vanilla("ruined_portal_nether"), built->netherStructures.get(),
                                        &terrain::NetherStructureGenerator::netherPortalBoundsAt,
                                        &terrain::NetherStructureGenerator::netherPortalPlaceAt));
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
    std::expected<terrain::NoiseRouter, std::string> router =
        terrain::noiseRouterData::load("minecraft:end", seed, worldgenReader());
    if (!router.has_value()) {
        logger_.warn([&router] { return "No End terrain can be generated: " + router.error(); });
        return {};
    }
    built->endRouter = std::make_unique<terrain::NoiseRouter>(std::move(*router));
    built->endBiomes = std::make_unique<terrain::TheEndBiomeSource>(*built->endRouter, biomeZoomSeed);
    built->endCities = std::make_unique<terrain::EndCityStructureGenerator>(*built->endRouter, *built->endBiomes,
                                                                            built->loader->templateLoader());

    Built& world = *built;
    Generators answer;
    answer.pipeline.terrain = [&world](std::int32_t chunkX, std::int32_t chunkZ) {
        return terrain::realTerrainGenerator::GeneratedTerrain{
            terrain::dimensionTerrainGenerator::endTerrain(*world.endRouter, *world.endBiomes, chunkX, chunkZ,
                                                           world.seed, world.endCities.get()),
            nullptr, nullptr};
    };
    answer.pipeline.adopt = [](server::world::Chunk chunk) {
        return terrain::realTerrainGenerator::GeneratedTerrain{std::move(chunk), nullptr, nullptr};
    };
    answer.pipeline.decorate = [&world](terrain::realTerrainGenerator::GeneratedTerrain& generated,
                                        terrain::ProtoRegion& region) {
        const server::world::Chunk::RegionScope scope{generated.chunk(), region};
        terrain::dimensionTerrainGenerator::decorateEnd(generated.chunk(), *world.endRouter, *world.endBiomes,
                                                        world.seed);
    };
    answer.placers.push_back(makePlacer(Identifier::vanilla("end_city"), built->endCities.get(),
                                        &terrain::EndCityStructureGenerator::placeBoundsAt,
                                        &terrain::EndCityStructureGenerator::placeAt));
    built_.push_back(std::move(built));
    return answer;
}

} // namespace basalt::server::runtime::world
