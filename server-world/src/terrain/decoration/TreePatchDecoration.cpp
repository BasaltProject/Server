#include <basalt/server/random/PositionalRandomFactory.hpp>
#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/world/terrain/decoration/BambooVegetationDecoration.hpp>
#include <basalt/server/world/terrain/decoration/TreeDecoration.hpp>
#include <basalt/server/world/terrain/decoration/TreePatchDecoration.hpp>

#include <stdexcept>
#include <utility>

namespace basalt::server::world::terrain::decoration {
namespace {

constexpr std::int32_t kVegetalDecorationStep = 9;
constexpr std::string_view kBambooVegetation = "bamboo_vegetation";

[[nodiscard]] std::string_view biomePath(std::string_view id) {
    const std::size_t colon = id.find(':');
    return colon == std::string_view::npos ? id : id.substr(colon + 1);
}

[[nodiscard]] PlacementModifier countExtra(std::int32_t count, float chance, std::int32_t extra) {
    return countExtraPlacement(count, chance, extra);
}

/// parity: `WorldGenRegion.random`, positioned at the centre chunk's minimum block corner.
[[nodiscard]] random::RandomSource worldgenRegionRandom(std::int64_t seed, const Chunk& chunk) {
    random::RandomSource source = random::RandomSource::xoroshiro(seed);
    random::RandomSource named = source.forkPositional().fromHashOf("minecraft:worldgen_region_random");
    return named.forkPositional().at(chunk.chunkX() * 16, 0, chunk.chunkZ() * 16);
}

void decorateSpecs(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                   const std::vector<const treePatchDecoration::FeatureSpec*>& specs,
                   const treePatchDecoration::PlacementObserver& observer) {
    // The reader the caller installed for the whole phase stays in force: a tree leaning over the
    // chunk edge has to see the neighbour's real blocks, which is what vanilla's WorldGenRegion
    // gives it.
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    random::RandomSource regionRandom = worldgenRegionRandom(seed, chunk);
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    const PlacementContext context{chunk};
    const DecorationPos origin{.x = chunk.chunkX() * 16, .y = 0, .z = chunk.chunkZ() * 16};

    for (const treePatchDecoration::FeatureSpec* spec : specs) {
        random::setFeatureSeed(random, decorationSeed, spec->index, kVegetalDecorationStep);
        const std::vector<PlacementModifier> modifiers{spec->frequency, inSquarePlacement(),
                                                       surfaceWaterDepthFilter(spec->maxWaterDepth),
                                                       heightmapPlacement(HeightmapType::OceanFloor)};
        placeFrom(chunk, random, origin, modifiers, [&](const DecorationPos& position) {
            if (!spec->biomes.contains(biomePath(environment.biomeAt(position.x, position.y, position.z)))) {
                return;
            }
            if (!treeDecoration::survivesPlacementPredicate(chunk, position, spec->survivalSapling)) {
                return;
            }
            if (observer != nullptr) {
                observer(spec->name, position);
            }
            if (spec->name == kBambooVegetation) {
                bambooVegetationDecoration::placeAt(chunk, random, position, context);
            } else {
                treeDecoration::placeAt(chunk, random, position, spec->name, regionRandom);
            }
        });
    }
}

} // namespace

const std::vector<treePatchDecoration::FeatureSpec>& treePatchDecoration::featureSpecs() {
    static const std::vector<FeatureSpec> specs = [] {
        std::vector<FeatureSpec> built;
        built.push_back({.name = "trees_windswept_savanna",
                         .index = 1,
                         .frequency = countExtra(2, 0.1F, 1),
                         .biomes = {"windswept_savanna"}});
        built.push_back({.name = std::string{kBambooVegetation},
                         .index = 3,
                         .frequency = countPlacement(IntProvider::weighted({{30, 9}, {31, 1}})),
                         .biomes = {"bamboo_jungle"}});
        built.push_back({.name = "trees_sparse_jungle",
                         .index = 4,
                         .frequency = countExtra(2, 0.1F, 1),
                         .biomes = {"sparse_jungle"}});
        built.push_back({.name = "trees_badlands",
                         .index = 5,
                         .frequency = countExtra(5, 0.1F, 1),
                         .biomes = {"wooded_badlands"},
                         .survivalSapling = "oak_sapling"});
        built.push_back(
            {.name = "trees_jungle", .index = 7, .frequency = countExtra(50, 0.1F, 1), .biomes = {"jungle"}});
        built.push_back({.name = "trees_savanna",
                         .index = 9,
                         .frequency = countExtra(1, 0.1F, 1),
                         .biomes = {"savanna", "savanna_plateau"}});
        built.push_back({.name = "pale_garden_vegetation",
                         .index = 13,
                         .frequency = countPlacement(16),
                         .biomes = {"pale_garden"}});
        built.push_back({.name = "dark_forest_vegetation",
                         .index = 17,
                         .frequency = countPlacement(16),
                         .biomes = {"dark_forest"}});
        built.push_back({.name = "trees_flower_forest",
                         .index = 19,
                         .frequency = countExtra(6, 0.1F, 1),
                         .biomes = {"flower_forest"}});
        built.push_back({.name = "birch_tall",
                         .index = 23,
                         .frequency = countExtra(10, 0.1F, 1),
                         .biomes = {"old_growth_birch_forest"}});
        built.push_back({.name = "trees_birch",
                         .index = 24,
                         .frequency = countExtra(10, 0.1F, 1),
                         .biomes = {"birch_forest"},
                         .survivalSapling = "birch_sapling"});
        built.push_back({.name = "trees_birch_and_oak_leaf_litter",
                         .index = 25,
                         .frequency = countExtra(10, 0.1F, 1),
                         .biomes = {"forest"}});
        built.push_back({.name = "trees_meadow", .index = 37, .frequency = rarityFilter(100), .biomes = {"meadow"}});
        built.push_back({.name = "trees_windswept_forest",
                         .index = 39,
                         .frequency = countExtra(3, 0.1F, 1),
                         .biomes = {"windswept_forest"}});
        built.push_back({.name = "trees_old_growth_pine_taiga",
                         .index = 41,
                         .frequency = countExtra(10, 0.1F, 1),
                         .biomes = {"old_growth_pine_taiga"}});
        built.push_back({.name = "trees_old_growth_spruce_taiga",
                         .index = 42,
                         .frequency = countExtra(10, 0.1F, 1),
                         .biomes = {"old_growth_spruce_taiga"}});
        built.push_back({.name = "trees_taiga",
                         .index = 43,
                         .frequency = countExtra(10, 0.1F, 1),
                         .biomes = {"snowy_taiga", "taiga"}});
        built.push_back(
            {.name = "trees_grove", .index = 44, .frequency = countExtra(10, 0.1F, 1), .biomes = {"grove"}});
        built.push_back({.name = "trees_windswept_hills",
                         .index = 45,
                         .frequency = countExtra(0, 0.1F, 1),
                         .biomes = {"windswept_gravelly_hills", "windswept_hills"}});
        built.push_back({.name = "trees_snowy",
                         .index = 46,
                         .frequency = countExtra(0, 0.1F, 1),
                         .biomes = {"ice_spikes", "snowy_plains"},
                         .survivalSapling = "spruce_sapling"});
        built.push_back({.name = "trees_mangrove",
                         .index = 47,
                         .frequency = countPlacement(25),
                         .biomes = {"mangrove_swamp"},
                         .maxWaterDepth = 5});
        built.push_back({.name = "trees_swamp",
                         .index = 48,
                         .frequency = countExtra(2, 0.1F, 1),
                         .biomes = {"swamp"},
                         .maxWaterDepth = 2,
                         .survivalSapling = "oak_sapling"});
        built.push_back(
            {.name = "trees_water",
             .index = 50,
             .frequency = countPlacement(IntProvider::weighted({{0, 9}, {1, 1}})),
             .biomes = {"cold_ocean", "deep_cold_ocean", "deep_frozen_ocean", "deep_lukewarm_ocean", "deep_ocean",
                        "frozen_ocean", "frozen_river", "lukewarm_ocean", "ocean", "river", "warm_ocean"}});
        built.push_back({.name = "trees_plains",
                         .index = 52,
                         .frequency = countExtra(0, 0.05F, 1),
                         .biomes = {"deep_dark", "dripstone_caves", "plains", "sunflower_plains"},
                         .survivalSapling = "oak_sapling"});
        built.push_back({.name = "trees_cherry",
                         .index = 56,
                         .frequency = countExtra(10, 0.1F, 1),
                         .biomes = {"cherry_grove"},
                         .survivalSapling = "cherry_sapling"});
        return built;
    }();
    return specs;
}

void treePatchDecoration::decorate(Chunk& chunk, const std::int64_t seed,
                                   const surface::SurfaceRuleEnvironment& environment) {
    std::vector<const FeatureSpec*> all;
    all.reserve(featureSpecs().size());
    for (const FeatureSpec& spec : featureSpecs()) {
        all.push_back(&spec);
    }
    decorateSpecs(chunk, seed, environment, all, nullptr);
}

void treePatchDecoration::decorateFeature(Chunk& chunk, const std::int64_t seed,
                                          const surface::SurfaceRuleEnvironment& environment,
                                          const std::string_view name, PlacementObserver observer) {
    for (const FeatureSpec& spec : featureSpecs()) {
        if (spec.name == name) {
            decorateSpecs(chunk, seed, environment, {&spec}, observer);
            return;
        }
    }
    throw std::runtime_error{"unknown tree patch feature '" + std::string{name} + "'"};
}

} // namespace basalt::server::world::terrain::decoration
