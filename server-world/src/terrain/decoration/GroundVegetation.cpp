#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/GroundVegetation.hpp>

#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace basalt::server::world::terrain::decoration {
namespace {

[[nodiscard]] std::int32_t stateIdOf(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"unknown block '" + std::string{path} + "'"};
    }
    return found->stateId;
}

[[nodiscard]] SimpleBlockFeature::StateProvider constant(std::string_view path) {
    return [path](random::RandomSource&, const DecorationPos&) { return stateIdOf(path); };
}

/// parity: the weighted dry-grass provider is short 1, tall 1, so `nextInt(2)` decides.
[[nodiscard]] SimpleBlockFeature::StateProvider dryGrassProvider() {
    return [](random::RandomSource& random, const DecorationPos&) {
        return random.nextInt(2) == 0 ? stateIdOf("short_dry_grass") : stateIdOf("tall_dry_grass");
    };
}

} // namespace

const std::set<std::string, std::less<>>& defaultVegetationDecoration::placedIds() {
    static const std::set<std::string, std::less<>> ids = {"flower_default", "flower_warm"};
    return ids;
}

void defaultVegetationDecoration::placeDefaultPatchAt(Chunk& chunk, random::RandomSource& random,
                                                      const DecorationPos& origin) {
    const SimpleBlockFeature feature{[](random::RandomSource& r, const DecorationPos&) {
        return r.nextInt(3) < 2 ? stateIdOf("poppy") : stateIdOf("dandelion");
    }};
    placeFrom(chunk, random, origin, {countPlacement(64), randomOffsetPlacement(7, 3), airTargetFilter()},
              [&chunk, &random, &feature](const DecorationPos& position) { feature.place(chunk, random, position); });
}

const std::set<std::string, std::less<>>& dryGrassDecoration::placedIds() {
    static const std::set<std::string, std::less<>> ids = {"patch_dry_grass_desert", "patch_dry_grass_badlands"};
    return ids;
}

void dryGrassDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    const SimpleBlockFeature feature{dryGrassProvider()};
    placeFrom(chunk, random, origin, {countPlacement(64), randomOffsetPlacement(7, 3), airTargetFilter()},
              [&chunk, &random, &feature](const DecorationPos& position) { feature.place(chunk, random, position); });
}

namespace {

[[nodiscard]] BiomeVegetalDecoration dryGrassPlaced(std::int32_t rarity) {
    return placedVegetation({rarityFilter(rarity), inSquarePlacement(), heightmapPlacement(), countPlacement(64),
                             randomOffsetPlacement(7, 3), airTargetFilter()},
                            SimpleBlockFeature{dryGrassProvider()});
}

} // namespace

const BiomeVegetalDecoration& dryGrassDecoration::desert() {
    static const BiomeVegetalDecoration decoration = dryGrassPlaced(3);
    return decoration;
}

const BiomeVegetalDecoration& dryGrassDecoration::badlands() {
    static const BiomeVegetalDecoration decoration = dryGrassPlaced(6);
    return decoration;
}

const BiomeVegetalDecoration& forestGroundDecoration::bush() {
    static const BiomeVegetalDecoration decoration =
        placedVegetation({rarityFilter(4), inSquarePlacement(), heightmapPlacement(), countPlacement(24),
                          randomOffsetPlacement(5, 3), airTargetFilter()},
                         SimpleBlockFeature{constant("bush")});
    return decoration;
}

const BiomeVegetalDecoration& forestGroundDecoration::grass() {
    static const BiomeVegetalDecoration decoration =
        placedVegetation({countPlacement(2), inSquarePlacement(), heightmapPlacement(HeightmapType::WorldSurface),
                          countPlacement(32), randomOffsetPlacement(7, 3), airTargetFilter()},
                         SimpleBlockFeature{constant("short_grass")});
    return decoration;
}

const BiomeVegetalDecoration& savannaGroundDecoration::tallGrass() {
    static const BiomeVegetalDecoration decoration =
        placedVegetation({rarityFilter(5), inSquarePlacement(), heightmapPlacement(), countPlacement(96),
                          randomOffsetPlacement(7, 3), airTargetFilter(true)},
                         SimpleBlockFeature{constant("tall_grass"), true});
    return decoration;
}

namespace {

[[nodiscard]] BiomeVegetalDecoration savannaGrass(std::int32_t outerCount) {
    return placedVegetation({countPlacement(outerCount), inSquarePlacement(),
                             heightmapPlacement(HeightmapType::WorldSurface), countPlacement(32),
                             randomOffsetPlacement(7, 3), airTargetFilter()},
                            SimpleBlockFeature{constant("short_grass")});
}

} // namespace

const BiomeVegetalDecoration& savannaGroundDecoration::savanna() {
    static const BiomeVegetalDecoration decoration = savannaGrass(20);
    return decoration;
}

const BiomeVegetalDecoration& savannaGroundDecoration::windswept() {
    static const BiomeVegetalDecoration decoration = savannaGrass(5);
    return decoration;
}

void desertDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    const SimpleBlockFeature feature{constant("dead_bush")};
    placeFrom(chunk, random, origin, {countPlacement(4), randomOffsetPlacement(7, 3), airTargetFilter()},
              [&chunk, &random, &feature](const DecorationPos& position) { feature.place(chunk, random, position); });
}

const BiomeVegetalDecoration& desertDecoration::decoration() {
    static const BiomeVegetalDecoration built = [](Chunk& chunk, random::RandomSource& random) {
        dryGrassDecoration::desert()(chunk, random);
        // parity: placed_feature/patch_dead_bush_2.json, worldSurfaceSquaredWithCount(2) plus
        // copyAndAdd. The outer count is two here and twenty in the badlands variant.
        const SimpleBlockFeature feature{constant("dead_bush")};
        runFromChunkCorner(chunk, random,
                           {countPlacement(2), inSquarePlacement(), heightmapPlacement(), countPlacement(4),
                            randomOffsetPlacement(7, 3), airTargetFilter()},
                           feature);
    };
    return built;
}

const BiomeVegetalDecoration& badlandsDecoration::decoration() {
    static const BiomeVegetalDecoration built = [](Chunk& chunk, random::RandomSource& random) {
        dryGrassDecoration::badlands()(chunk, random);
        // parity: placed_feature/patch_dead_bush_badlands.json, worldSurfaceSquaredWithCount(20)
        // plus copyAndAdd.
        const SimpleBlockFeature feature{constant("dead_bush")};
        runFromChunkCorner(chunk, random,
                           {countPlacement(20), inSquarePlacement(), heightmapPlacement(), countPlacement(4),
                            randomOffsetPlacement(7, 3), airTargetFilter()},
                           feature);
    };
    return built;
}

} // namespace basalt::server::world::terrain::decoration
