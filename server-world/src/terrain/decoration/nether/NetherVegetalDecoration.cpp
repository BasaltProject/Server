#include "NetherVegetalShared.hpp"

#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/world/terrain/decoration/nether/NetherVegetalDecoration.hpp>

#include <cstdint>
#include <utility>

namespace basalt::server::world::terrain::decoration::nether {
namespace {

constexpr std::int32_t kVegetalDecorationStep = 9;

void placeFeature(Chunk& chunk, random::RandomSource& random, const NetherBiomeLookup& biomeSource,
                  const std::int32_t globalIndex) {
    const std::vector<std::string>& order = netherVegetalDecoration::featureOrder();
    if (globalIndex < 0 || globalIndex >= static_cast<std::int32_t>(order.size())) {
        return;
    }
    // Dispatch by name: an index on its own says nothing about which fungus it is, and the crimson
    // and warped ones were crossed here for exactly that reason.
    const std::string& name = order[static_cast<std::size_t>(globalIndex)];
    if (name == "spring_lava") {
        shared::placeLavaSprings(chunk, random, biomeSource);
    } else if (name == "brown_mushroom_normal") {
        shared::placeNormalMushroom(chunk, random, biomeSource, 256, "brown_mushroom");
    } else if (name == "red_mushroom_normal") {
        shared::placeNormalMushroom(chunk, random, biomeSource, 512, "red_mushroom");
    } else if (name == "warped_fungi") {
        shared::placeHugeFungi(chunk, random, biomeSource, shared::warpedFungus());
    } else if (name == "warped_forest_vegetation") {
        shared::placeForestVegetation(chunk, random, biomeSource, shared::ForestVegetation::Warped);
    } else if (name == "nether_sprouts") {
        shared::placeForestVegetation(chunk, random, biomeSource, shared::ForestVegetation::Sprouts);
    } else if (name == "twisting_vines") {
        shared::placeTwistingVines(chunk, random, biomeSource);
    } else if (name == "weeping_vines") {
        shared::placeWeepingVines(chunk, random, biomeSource);
    } else if (name == "crimson_fungi") {
        shared::placeHugeFungi(chunk, random, biomeSource, shared::crimsonFungus());
    } else if (name == "crimson_forest_vegetation") {
        shared::placeForestVegetation(chunk, random, biomeSource, shared::ForestVegetation::Crimson);
    }
}

} // namespace

const std::vector<std::string>& netherVegetalDecoration::featureOrder() {
    static const std::vector<std::string> order = {
        "spring_lava",    "brown_mushroom_normal", "red_mushroom_normal", "warped_fungi",  "warped_forest_vegetation",
        "nether_sprouts", "twisting_vines",        "weeping_vines",       "crimson_fungi", "crimson_forest_vegetation"};
    return order;
}

const std::set<std::string, std::less<>>& netherVegetalDecoration::placedIds() {
    static const std::set<std::string, std::less<>> ids = {
        "crimson_fungi",  "warped_fungi",  "crimson_forest_vegetation", "warped_forest_vegetation", "nether_sprouts",
        "twisting_vines", "weeping_vines", "spring_lava_nether"};
    return ids;
}

void netherVegetalDecoration::decorate(Chunk& chunk, const std::int64_t seed, const NetherBiomeLookup& biomeSource) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    for (std::size_t index = 0; index < featureOrder().size(); ++index) {
        const auto globalIndex = static_cast<std::int32_t>(index);
        random::setFeatureSeed(random, decorationSeed, globalIndex, kVegetalDecorationStep);
        placeFeature(chunk, random, biomeSource, globalIndex);
    }
}

void netherVegetalDecoration::decorateFeature(Chunk& chunk, const std::int64_t seed,
                                              const NetherBiomeLookup& biomeSource, const std::int32_t globalIndex) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, globalIndex, kVegetalDecorationStep);
    placeFeature(chunk, random, biomeSource, globalIndex);
}

bool netherVegetalDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const std::int32_t x,
                                      const std::int32_t y, const std::int32_t z,
                                      const std::string_view placedFeature) {
    const shared::Pos origin{.x = x, .y = y, .z = z};
    if (placedFeature == "crimson_fungi") {
        shared::placeHugeFungus(chunk, random, origin, shared::crimsonFungus());
    } else if (placedFeature == "warped_fungi") {
        shared::placeHugeFungus(chunk, random, origin, shared::warpedFungus());
    } else if (placedFeature == "crimson_forest_vegetation") {
        shared::placeVegetationAt(chunk, random, origin, shared::ForestVegetation::Crimson);
    } else if (placedFeature == "warped_forest_vegetation") {
        shared::placeVegetationAt(chunk, random, origin, shared::ForestVegetation::Warped);
    } else if (placedFeature == "nether_sprouts") {
        shared::placeVegetationAt(chunk, random, origin, shared::ForestVegetation::Sprouts);
    } else if (placedFeature == "twisting_vines") {
        shared::placeTwistingVinesFeature(chunk, random, origin);
    } else if (placedFeature == "weeping_vines") {
        shared::placeWeepingVinesFeature(chunk, random, origin);
    } else if (placedFeature == "spring_lava_nether") {
        shared::placeLavaSpring(chunk, x, y, z);
    } else {
        return false;
    }
    return true;
}

} // namespace basalt::server::world::terrain::decoration::nether
