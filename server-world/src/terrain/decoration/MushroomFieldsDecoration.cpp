#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/world/terrain/decoration/FlowerVegetation.hpp>
#include <basalt/server/world/terrain/decoration/HugeMushroomFeature.hpp>
#include <basalt/server/world/terrain/decoration/MushroomFieldsDecoration.hpp>

#include <cstdint>
#include <string_view>
#include <vector>

namespace basalt::server::world::terrain::decoration {
namespace {

constexpr std::int32_t kVegetalDecorationStep = 9;

[[nodiscard]] std::string_view biomePath(std::string_view id) {
    const std::size_t colon = id.find(':');
    return colon == std::string_view::npos ? id : id.substr(colon + 1);
}

// parity: `mushroom_island_vegetation` has no count modifier, one position per chunk.
const std::vector<PlacementModifier>& islandModifiers() {
    static const std::vector<PlacementModifier> modifiers = {inSquarePlacement(), heightmapPlacement()};
    return modifiers;
}

} // namespace

void mushroomFieldsDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    const hugeMushroomFeature::Kind kind =
        random.nextBoolean() ? hugeMushroomFeature::Kind::Red : hugeMushroomFeature::Kind::Brown;
    hugeMushroomFeature::place(chunk, random, origin.x, origin.y, origin.z, kind);
}

BiomeVegetalDecoration mushroomFieldsDecoration::hugeOnly() {
    return [](Chunk& chunk, random::RandomSource& random) {
        const DecorationPos chunkOrigin{.x = chunk.chunkX() * 16, .y = 0, .z = chunk.chunkZ() * 16};
        placeFrom(chunk, random, chunkOrigin, islandModifiers(),
                  [&](const DecorationPos& origin) { placeAt(chunk, random, origin); });
    };
}

BiomeVegetalDecoration mushroomFieldsDecoration::vegetalDecoration() {
    return combine({hugeOnly(), mushroomDecoration::taiga()});
}

void mushroomFieldsDecoration::decorate(Chunk& chunk, const std::int64_t seed,
                                        const surface::SurfaceRuleEnvironment& environment) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, kMushroomIslandVegetationIndex, kVegetalDecorationStep);
    const DecorationPos chunkOrigin{.x = chunk.chunkX() * 16, .y = 0, .z = chunk.chunkZ() * 16};
    placeFrom(chunk, random, chunkOrigin, islandModifiers(), [&](const DecorationPos& origin) {
        if (biomePath(environment.biomeAt(origin.x, origin.y, origin.z)) != "mushroom_fields") {
            return;
        }
        placeAt(chunk, random, origin);
    });
}

} // namespace basalt::server::world::terrain::decoration
