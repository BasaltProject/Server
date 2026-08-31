#include "NetherVegetalShared.hpp"

#include <basalt/server/registry/Blocks.hpp>

#include <string>

namespace basalt::server::world::terrain::decoration::nether::shared {
namespace {

const PathSet& nylium() {
    static const PathSet paths = {"crimson_nylium", "warped_nylium"};
    return paths;
}

const PathSet& vegetationSupport() {
    static const PathSet paths = {"dirt",       "coarse_dirt",     "rooted_dirt",   "mud",      "muddy_mangrove_roots",
                                  "moss_block", "pale_moss_block", "grass_block",   "mycelium", "podzol",
                                  "farmland",   "crimson_nylium",  "warped_nylium", "soul_soil"};
    return paths;
}

[[nodiscard]] std::string_view biomePath(std::string_view id) {
    const std::size_t colon = id.find(':');
    return colon == std::string_view::npos ? id : id.substr(colon + 1);
}

void scatterVegetation(Chunk& chunk, random::RandomSource& random, const Pos& origin, ForestVegetation config) {
    for (std::int32_t attempt = 0; attempt < 64; ++attempt) {
        const std::int32_t x = origin.x + random.nextInt(8) - random.nextInt(8);
        const std::int32_t y = origin.y + random.nextInt(4) - random.nextInt(4);
        const std::int32_t z = origin.z + random.nextInt(8) - random.nextInt(8);
        const std::string_view state = sampleForestState(config, random);
        if (isAir(chunk, x, y, z) && y > chunk.verticalBounds().minY() &&
            vegetationSupport().contains(blockPathAt(chunk, x, y - 1, z))) {
            setBlock(chunk, x, y, z, stateId(state));
        }
    }
}

} // namespace

std::int32_t forestLayerCount(const ForestVegetation config) {
    switch (config) {
    case ForestVegetation::Warped:
        return 5;
    case ForestVegetation::Sprouts:
        return 4;
    case ForestVegetation::Crimson:
        return 6;
    }
    return 0;
}

std::string_view forestBiome(const ForestVegetation config) {
    return config == ForestVegetation::Crimson ? "crimson_forest" : "warped_forest";
}

std::string_view sampleForestState(const ForestVegetation config, random::RandomSource& random) {
    if (config == ForestVegetation::Sprouts) {
        return "nether_sprouts";
    }
    if (config == ForestVegetation::Warped) {
        const std::int32_t roll = random.nextInt(100);
        if (roll <= 84) {
            return "warped_roots";
        }
        if (roll == 85) {
            return "crimson_roots";
        }
        return roll <= 98 ? "warped_fungus" : "crimson_fungus";
    }
    const std::int32_t roll = random.nextInt(99);
    if (roll <= 86) {
        return "crimson_roots";
    }
    return roll <= 97 ? "crimson_fungus" : "warped_fungus";
}

void placeVegetationAt(Chunk& chunk, random::RandomSource& random, const Pos& origin, const ForestVegetation config) {
    scatterVegetation(chunk, random, origin, config);
}

void placeForestVegetation(Chunk& chunk, random::RandomSource& random, const NetherBiomeLookup& biomeSource,
                           const ForestVegetation config) {
    const ChunkHeight bounds = chunk.verticalBounds();
    for (const Pos& origin : positionsOnEveryLayer(chunk, random, forestLayerCount(config))) {
        if (biomePath(biomeSource(origin.x, origin.y, origin.z, nullptr)) != forestBiome(config)) {
            continue;
        }
        if (!nylium().contains(blockPathAt(chunk, origin.x, origin.y - 1, origin.z))) {
            continue;
        }
        if (origin.y < bounds.minY() + 1 || origin.y + 1 > bounds.maxY()) {
            continue;
        }
        scatterVegetation(chunk, random, origin, config);
    }
}

} // namespace basalt::server::world::terrain::decoration::nether::shared
