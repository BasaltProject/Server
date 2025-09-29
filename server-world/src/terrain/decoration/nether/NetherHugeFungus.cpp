#include "NetherVegetalShared.hpp"

#include <basalt/server/registry/Blocks.hpp>

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <string>

namespace basalt::server::world::terrain::decoration::nether::shared {
namespace {

constexpr std::int32_t kNetherGenerationDepth = 128;
constexpr float kHugeFungusProbability = 0.06F;
constexpr float kHugeStemCornerProbability = 0.1F;
constexpr float kHugeHatDropProbability = 0.15F;

const PathSet& generallyReplaceable() {
    static const PathSet paths = {"water",
                                  "lava",
                                  "short_grass",
                                  "fern",
                                  "dead_bush",
                                  "bush",
                                  "short_dry_grass",
                                  "tall_dry_grass",
                                  "seagrass",
                                  "tall_seagrass",
                                  "fire",
                                  "soul_fire",
                                  "snow",
                                  "vine",
                                  "glow_lichen",
                                  "resin_clump",
                                  "light",
                                  "tall_grass",
                                  "large_fern",
                                  "structure_void",
                                  "bubble_column",
                                  "warped_roots",
                                  "nether_sprouts",
                                  "crimson_roots",
                                  "leaf_litter",
                                  "hanging_roots"};
    return paths;
}

const PathSet& stemReplaceable() {
    static const PathSet paths = {"oak_sapling",
                                  "spruce_sapling",
                                  "birch_sapling",
                                  "jungle_sapling",
                                  "acacia_sapling",
                                  "cherry_sapling",
                                  "dark_oak_sapling",
                                  "pale_oak_sapling",
                                  "mangrove_propagule",
                                  "dandelion",
                                  "torchflower",
                                  "poppy",
                                  "blue_orchid",
                                  "allium",
                                  "azure_bluet",
                                  "red_tulip",
                                  "orange_tulip",
                                  "white_tulip",
                                  "pink_tulip",
                                  "oxeye_daisy",
                                  "cornflower",
                                  "wither_rose",
                                  "lily_of_the_valley",
                                  "brown_mushroom",
                                  "red_mushroom",
                                  "wheat",
                                  "sugar_cane",
                                  "attached_pumpkin_stem",
                                  "attached_melon_stem",
                                  "pumpkin_stem",
                                  "melon_stem",
                                  "lily_pad",
                                  "nether_wart",
                                  "cocoa",
                                  "carrots",
                                  "potatoes",
                                  "chorus_plant",
                                  "chorus_flower",
                                  "torchflower_crop",
                                  "pitcher_crop",
                                  "beetroots",
                                  "sweet_berry_bush",
                                  "warped_fungus",
                                  "crimson_fungus",
                                  "weeping_vines",
                                  "weeping_vines_plant",
                                  "twisting_vines",
                                  "twisting_vines_plant",
                                  "cave_vines",
                                  "cave_vines_plant",
                                  "spore_blossom",
                                  "azalea",
                                  "flowering_azalea",
                                  "moss_carpet",
                                  "pink_petals",
                                  "wildflowers",
                                  "big_dripleaf",
                                  "big_dripleaf_stem",
                                  "small_dripleaf"};
    return paths;
}

[[nodiscard]] std::string_view biomePath(std::string_view id) {
    const std::size_t colon = id.find(':');
    return colon == std::string_view::npos ? id : id.substr(colon + 1);
}

[[nodiscard]] bool isReplaceable(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z,
                                 bool includeStemPlants) {
    const std::optional<std::int32_t> state = stateAt(chunk, x, y, z);
    if (!state.has_value()) {
        return false;
    }
    const std::string path = blockPath(*state);
    if (path.empty()) {
        return false;
    }
    return registry::blocks::isAir(*state) || generallyReplaceable().contains(path) ||
           (includeStemPlants && stemReplaceable().contains(path));
}

void tryPlaceVines(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z) {
    if (!isAir(chunk, x, y - 1, z)) {
        return;
    }
    std::int32_t height = 1 + random.nextInt(5);
    if (random.nextInt(7) == 0) {
        height *= 2;
    }
    placeWeepingColumn(chunk, random, x, y - 1, z, height, 23);
}

void placeHatBlock(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z,
                   const HugeFungus& config, float decorProbability, float hatProbability, float vinesProbability) {
    if (random.nextFloat() < decorProbability) {
        setBlock(chunk, x, y, z, stateId(config.decor));
        return;
    }
    if (random.nextFloat() < hatProbability) {
        setBlock(chunk, x, y, z, stateId(config.hat));
        if (random.nextFloat() < vinesProbability) {
            tryPlaceVines(chunk, random, x, y, z);
        }
    }
}

void placeDropBlock(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z,
                    const HugeFungus& config, bool placesVines) {
    if (blockPathAt(chunk, x, y - 1, z) == config.hat) {
        setBlock(chunk, x, y, z, stateId(config.hat));
        return;
    }
    if (random.nextFloat() < kHugeHatDropProbability) {
        setBlock(chunk, x, y, z, stateId(config.hat));
        if (placesVines && random.nextInt(11) == 0) {
            tryPlaceVines(chunk, random, x, y, z);
        }
    }
}

void placeStem(Chunk& chunk, random::RandomSource& random, const Pos& origin, std::int32_t totalHeight, bool isHuge,
               const HugeFungus& config) {
    const std::int32_t radius = isHuge ? 1 : 0;
    const std::int32_t stem = stateId(config.stem);
    for (std::int32_t dx = -radius; dx <= radius; ++dx) {
        for (std::int32_t dz = -radius; dz <= radius; ++dz) {
            const bool corner = isHuge && std::abs(dx) == radius && std::abs(dz) == radius;
            for (std::int32_t dy = 0; dy < totalHeight; ++dy) {
                const std::int32_t x = origin.x + dx;
                const std::int32_t y = origin.y + dy;
                const std::int32_t z = origin.z + dz;
                if (!isReplaceable(chunk, x, y, z, true)) {
                    continue;
                }
                if (!corner || random.nextFloat() < kHugeStemCornerProbability) {
                    setBlock(chunk, x, y, z, stem);
                }
            }
        }
    }
}

void placeHat(Chunk& chunk, random::RandomSource& random, const Pos& origin, std::int32_t totalHeight, bool isHuge,
              const HugeFungus& config) {
    const bool placesVines = config.hat == "nether_wart_block";
    const std::int32_t hatHeight = std::min(random.nextInt(1 + totalHeight / 3) + 5, totalHeight);
    const std::int32_t hatStartY = totalHeight - hatHeight;
    for (std::int32_t dy = hatStartY; dy <= totalHeight; ++dy) {
        std::int32_t radius = dy < totalHeight - random.nextInt(3) ? 2 : 1;
        if (hatHeight > 8 && dy < hatStartY + 4) {
            radius = 3;
        }
        if (isHuge) {
            radius++;
        }
        for (std::int32_t dx = -radius; dx <= radius; ++dx) {
            for (std::int32_t dz = -radius; dz <= radius; ++dz) {
                const bool edgeX = dx == -radius || dx == radius;
                const bool edgeZ = dz == -radius || dz == radius;
                const bool inside = !edgeX && !edgeZ && dy != totalHeight;
                const bool corner = edgeX && edgeZ;
                const bool hatBottom = dy < hatStartY + 3;
                const std::int32_t x = origin.x + dx;
                const std::int32_t y = origin.y + dy;
                const std::int32_t z = origin.z + dz;
                if (!isReplaceable(chunk, x, y, z, false)) {
                    continue;
                }
                if (hatBottom) {
                    if (!inside) {
                        placeDropBlock(chunk, random, x, y, z, config, placesVines);
                    }
                } else if (inside) {
                    placeHatBlock(chunk, random, x, y, z, config, 0.1F, 0.2F, placesVines ? 0.1F : 0.0F);
                } else if (corner) {
                    placeHatBlock(chunk, random, x, y, z, config, 0.01F, 0.7F, placesVines ? 0.083F : 0.0F);
                } else {
                    placeHatBlock(chunk, random, x, y, z, config, 0.0005F, 0.98F, placesVines ? 0.07F : 0.0F);
                }
            }
        }
    }
}

} // namespace

const HugeFungus& crimsonFungus() {
    static const HugeFungus config{.biome = "crimson_forest",
                                   .validBase = "crimson_nylium",
                                   .stem = "crimson_stem",
                                   .hat = "nether_wart_block",
                                   .decor = "shroomlight"};
    return config;
}

const HugeFungus& warpedFungus() {
    static const HugeFungus config{.biome = "warped_forest",
                                   .validBase = "warped_nylium",
                                   .stem = "warped_stem",
                                   .hat = "warped_wart_block",
                                   .decor = "shroomlight"};
    return config;
}

void placeHugeFungus(Chunk& chunk, random::RandomSource& random, const Pos& origin, const HugeFungus& config) {
    if (blockPathAt(chunk, origin.x, origin.y - 1, origin.z) != config.validBase) {
        return;
    }
    std::int32_t totalHeight = 4 + random.nextInt(10);
    if (random.nextInt(12) == 0) {
        totalHeight *= 2;
    }
    if (origin.y + totalHeight + 1 >= kNetherGenerationDepth) {
        return;
    }
    const bool isHuge = random.nextFloat() < kHugeFungusProbability;
    setBlock(chunk, origin.x, origin.y, origin.z, registry::blocks::air().stateId);
    placeStem(chunk, random, origin, totalHeight, isHuge, config);
    placeHat(chunk, random, origin, totalHeight, isHuge, config);
}

void placeHugeFungi(Chunk& chunk, random::RandomSource& random, const NetherBiomeLookup& biomeSource,
                    const HugeFungus& config) {
    for (const Pos& origin : positionsOnEveryLayer(chunk, random, 8)) {
        if (biomePath(biomeSource(origin.x, origin.y, origin.z, nullptr)) == config.biome) {
            placeHugeFungus(chunk, random, origin, config);
        }
    }
}

} // namespace basalt::server::world::terrain::decoration::nether::shared
