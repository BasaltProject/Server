#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/world/terrain/decoration/BiomeFlowerFeatures.hpp>
#include <basalt/server/world/terrain/decoration/FlowerFeatures.hpp>
#include <basalt/server/world/terrain/decoration/FlowerPatchDecoration.hpp>
#include <basalt/server/world/terrain/decoration/FlowerVegetation.hpp>
#include <basalt/server/world/terrain/decoration/GroundVegetation.hpp>
#include <basalt/server/world/terrain/decoration/PlainsFlowerFeatures.hpp>

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace basalt::server::world::terrain::decoration {
namespace {

constexpr std::int32_t kVegetalDecorationStep = 9;

[[nodiscard]] std::string_view biomePath(std::string_view id) {
    const std::size_t colon = id.find(':');
    return colon == std::string_view::npos ? id : id.substr(colon + 1);
}

struct FeatureSpec {
    std::string_view name;
    std::int32_t index;
    std::vector<PlacementModifier> beforeBiome;
    std::vector<std::string> biomes;
    std::function<void(Chunk&, random::RandomSource&, const DecorationPos&)> place;
};

const std::vector<FeatureSpec>& specs() {
    static const std::vector<FeatureSpec> built = {
        {.name = "flower_warm",
         .index = 10,
         .beforeBiome = {rarityFilter(16), inSquarePlacement(), heightmapPlacement()},
         .biomes = {"bamboo_jungle", "jungle", "savanna", "savanna_plateau", "sparse_jungle"},
         .place =
             [](Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
                 defaultVegetationDecoration::placeDefaultPatchAt(chunk, random, origin);
             }},
        {.name = "pale_garden_flowers",
         .index = 15,
         .beforeBiome = {rarityFilter(8), inSquarePlacement(),
                         heightmapPlacement(HeightmapType::MotionBlockingNoLeaves)},
         .biomes = {"pale_garden"},
         .place = [](Chunk& chunk, random::RandomSource& random,
                     const DecorationPos& origin) { paleGardenDecoration::placeAt(chunk, random, origin); }},
        {.name = "flower_pale_garden",
         .index = 16,
         .beforeBiome = {rarityFilter(32), inSquarePlacement(), heightmapPlacement()},
         .biomes = {"pale_garden"},
         .place = [](Chunk& chunk, random::RandomSource& random,
                     const DecorationPos& origin) { paleGardenDecoration::placeSingleAt(chunk, random, origin); }},
        {.name = "flower_forest_flowers",
         .index = 18,
         .beforeBiome = {rarityFilter(7), inSquarePlacement(), heightmapPlacement(),
                         countPlacement(IntProvider::clamped(IntProvider::uniform(-1, 3), 0, 3))},
         .biomes = {"flower_forest"},
         .place = [](Chunk& chunk, random::RandomSource& random,
                     const DecorationPos& origin) { forestFlowerDecoration::placeAt(chunk, random, origin); }},
        {.name = "flower_flower_forest",
         .index = 20,
         .beforeBiome = {countPlacement(3), rarityFilter(2), inSquarePlacement(), heightmapPlacement()},
         .biomes = {"flower_forest"},
         .place = [](Chunk& chunk, random::RandomSource& random,
                     const DecorationPos& origin) { flowerForestDecoration::placeAt(chunk, random, origin); }},
        {.name = "forest_flowers",
         .index = 21,
         .beforeBiome = {rarityFilter(7), inSquarePlacement(), heightmapPlacement(),
                         countPlacement(IntProvider::clamped(IntProvider::uniform(-3, 1), 0, 1))},
         .biomes = {"birch_forest", "dark_forest", "forest", "old_growth_birch_forest"},
         .place = [](Chunk& chunk, random::RandomSource& random,
                     const DecorationPos& origin) { forestFlowerDecoration::placeAt(chunk, random, origin); }},
        {.name = "wildflowers_birch_forest",
         .index = 22,
         .beforeBiome = {countPlacement(3), rarityFilter(2), inSquarePlacement(), heightmapPlacement()},
         .biomes = {"birch_forest", "old_growth_birch_forest"},
         .place =
             [](Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
                 forestFloorDecoration::placeWildflowersAt(chunk, random, origin, 64);
             }},
        {.name = "flower_meadow",
         .index = 36,
         .beforeBiome = {inSquarePlacement(), heightmapPlacement()},
         .biomes = {"meadow"},
         .place = [](Chunk& chunk, random::RandomSource& random,
                     const DecorationPos& origin) { meadowDecoration::placeAt(chunk, random, origin); }},
        {.name = "wildflowers_meadow",
         .index = 38,
         .beforeBiome = {noiseThresholdCountPlacement(-0.8, 5, 10), inSquarePlacement(), heightmapPlacement()},
         .biomes = {"meadow"},
         .place =
             [](Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
                 forestFloorDecoration::placeWildflowersAt(chunk, random, origin, 8);
             }},
        {.name = "flower_swamp",
         .index = 49,
         .beforeBiome = {rarityFilter(32), inSquarePlacement(), heightmapPlacement()},
         .biomes = {"swamp"},
         .place = [](Chunk& chunk, random::RandomSource& random,
                     const DecorationPos& origin) { swampDecoration::placeAt(chunk, random, origin); }},
        {.name = "flower_plains",
         .index = 53,
         .beforeBiome = {noiseThresholdCountPlacement(-0.8, 15, 4), rarityFilter(32), inSquarePlacement(),
                         heightmapPlacement()},
         .biomes = {"deep_dark", "dripstone_caves", "plains", "sunflower_plains"},
         .place =
             [](Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
                 plainsDecoration::placeFlowerPlainAt(chunk, random, origin.x, origin.y, origin.z);
             }},
        {.name = "flower_cherry",
         .index = 55,
         .beforeBiome = {noiseThresholdCountPlacement(-0.8, 5, 10), inSquarePlacement(), heightmapPlacement()},
         .biomes = {"cherry_grove"},
         .place = [](Chunk& chunk, random::RandomSource& random,
                     const DecorationPos& origin) { cherryGroveDecoration::placeAt(chunk, random, origin); }},
        {.name = "flower_default",
         .index = 57,
         .beforeBiome = {rarityFilter(32), inSquarePlacement(), heightmapPlacement()},
         .biomes = {"beach",
                    "birch_forest",
                    "cold_ocean",
                    "dark_forest",
                    "deep_cold_ocean",
                    "deep_frozen_ocean",
                    "deep_lukewarm_ocean",
                    "deep_ocean",
                    "desert",
                    "forest",
                    "frozen_ocean",
                    "frozen_river",
                    "ice_spikes",
                    "lukewarm_ocean",
                    "ocean",
                    "old_growth_birch_forest",
                    "old_growth_pine_taiga",
                    "old_growth_spruce_taiga",
                    "river",
                    "snowy_beach",
                    "snowy_plains",
                    "snowy_taiga",
                    "stony_shore",
                    "taiga",
                    "warm_ocean",
                    "windswept_forest",
                    "windswept_gravelly_hills",
                    "windswept_hills",
                    "windswept_savanna"},
         .place = [](Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
             defaultVegetationDecoration::placeDefaultPatchAt(chunk, random, origin);
         }}};
    return built;
}

void decorateSpecs(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                   const std::vector<const FeatureSpec*>& chosen) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    const DecorationPos chunkOrigin{chunk.chunkX() * 16, 0, chunk.chunkZ() * 16};
    for (const FeatureSpec* spec : chosen) {
        random::setFeatureSeed(random, decorationSeed, spec->index, kVegetalDecorationStep);
        // The biome test sits between the modifiers and the body, so a position the modifiers
        // produced still costs its draws even when the biome rejects it.
        placeFrom(chunk, random, chunkOrigin, spec->beforeBiome, [&](const DecorationPos& position) {
            if (std::ranges::find(spec->biomes, biomePath(environment.biomeAt(position.x, position.y, position.z))) ==
                spec->biomes.end()) {
                return;
            }
            spec->place(chunk, random, position);
        });
    }
}

} // namespace

void flowerPatchDecoration::decorate(Chunk& chunk, std::int64_t seed,
                                     const surface::SurfaceRuleEnvironment& environment) {
    std::vector<const FeatureSpec*> chosen;
    chosen.reserve(specs().size());
    for (const FeatureSpec& spec : specs()) {
        chosen.push_back(&spec);
    }
    decorateSpecs(chunk, seed, environment, chosen);
}

void flowerPatchDecoration::decorateFeature(Chunk& chunk, std::int64_t seed,
                                            const surface::SurfaceRuleEnvironment& environment, std::string_view name) {
    std::vector<const FeatureSpec*> chosen;
    for (const FeatureSpec& spec : specs()) {
        if (spec.name == name) {
            chosen.push_back(&spec);
        }
    }
    if (chosen.size() != 1) {
        throw std::invalid_argument{"unknown flower patch '" + std::string{name} + "'"};
    }
    decorateSpecs(chunk, seed, environment, chosen);
}

} // namespace basalt::server::world::terrain::decoration
