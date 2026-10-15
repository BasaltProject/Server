#include <basalt/server/world/terrain/decoration/BiomeDecoration.hpp>

#include <set>
#include <string>
#include <utility>

namespace basalt::server::world::terrain::decoration {
namespace {

const std::set<std::string, std::less<>>& knownBiomes() {
    static const std::set<std::string, std::less<>> paths = {"plains",
                                                             "sunflower_plains",
                                                             "forest",
                                                             "flower_forest",
                                                             "birch_forest",
                                                             "old_growth_birch_forest",
                                                             "taiga",
                                                             "snowy_taiga",
                                                             "old_growth_spruce_taiga",
                                                             "old_growth_pine_taiga",
                                                             "snowy_plains",
                                                             "ice_spikes",
                                                             "savanna",
                                                             "savanna_plateau",
                                                             "windswept_savanna",
                                                             "jungle",
                                                             "sparse_jungle",
                                                             "windswept_hills",
                                                             "windswept_gravelly_hills",
                                                             "windswept_forest",
                                                             "grove",
                                                             "snowy_slopes",
                                                             "frozen_peaks",
                                                             "jagged_peaks",
                                                             "stony_peaks",
                                                             "wooded_badlands",
                                                             "badlands",
                                                             "eroded_badlands",
                                                             "swamp",
                                                             "desert",
                                                             "dark_forest",
                                                             "pale_garden",
                                                             "meadow",
                                                             "cherry_grove",
                                                             "river",
                                                             "frozen_river",
                                                             "beach",
                                                             "snowy_beach",
                                                             "stony_shore",
                                                             "bamboo_jungle",
                                                             "dripstone_caves",
                                                             "deep_dark",
                                                             "lush_caves",
                                                             "sulfur_caves",
                                                             "mangrove_swamp",
                                                             "mushroom_fields",
                                                             "ocean",
                                                             "deep_ocean",
                                                             "cold_ocean",
                                                             "deep_cold_ocean",
                                                             "lukewarm_ocean",
                                                             "deep_lukewarm_ocean",
                                                             "warm_ocean",
                                                             "frozen_ocean",
                                                             "deep_frozen_ocean",
                                                             "dappled_forest"};
    return paths;
}

} // namespace

BiomeVegetalDecoration combine(std::vector<BiomeVegetalDecoration> steps) {
    return [steps = std::move(steps)](Chunk& chunk, random::RandomSource& random) {
        for (const BiomeVegetalDecoration& step : steps) {
            step(chunk, random);
        }
    };
}

void runFromChunkCorner(Chunk& chunk, random::RandomSource& random, const std::vector<PlacementModifier>& modifiers,
                        const SimpleBlockFeature& feature) {
    const DecorationPos origin{chunk.chunkX() * 16, 0, chunk.chunkZ() * 16};
    placeFrom(chunk, random, origin, modifiers,
              [&chunk, &random, &feature](const DecorationPos& position) { feature.place(chunk, random, position); });
}

BiomeVegetalDecoration placedVegetation(std::vector<PlacementModifier> modifiers, SimpleBlockFeature feature) {
    return [modifiers = std::move(modifiers), feature = std::move(feature)](
               Chunk& chunk, random::RandomSource& random) { runFromChunkCorner(chunk, random, modifiers, feature); };
}

std::optional<BiomeVegetalDecoration> biomeDecoration::forBiome(const wire::common::Identifier& id) {
    if (knownBiomes().find(id.path()) == knownBiomes().end()) {
        return std::nullopt;
    }
    return BiomeVegetalDecoration{[](Chunk&, random::RandomSource&) {}};
}

} // namespace basalt::server::world::terrain::decoration
