#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/FlowerFeatures.hpp>

#include <array>
#include <stdexcept>
#include <string>
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

[[nodiscard]] SimpleBlockFeature closedEyeblossom() {
    return SimpleBlockFeature{
        [](random::RandomSource&, const DecorationPos&) { return stateIdOf("closed_eyeblossom"); }, false, true};
}

[[nodiscard]] SimpleBlockFeature pinkPetals() {
    return SimpleBlockFeature{[](random::RandomSource& random, const DecorationPos&) {
        return cherryGroveDecoration::pinkPetalsStateId(random.nextInt(16));
    }};
}

} // namespace

const BiomeVegetalDecoration& paleGardenDecoration::flowerPatch() {
    static const BiomeVegetalDecoration decoration = placedVegetation(
        {rarityFilter(8), inSquarePlacement(), heightmapPlacement(HeightmapType::MotionBlockingNoLeaves),
         countPlacement(96), randomOffsetPlacement(7, 3), airTargetFilter()},
        closedEyeblossom());
    return decoration;
}

const BiomeVegetalDecoration& paleGardenDecoration::rareFlower() {
    static const BiomeVegetalDecoration decoration =
        placedVegetation({rarityFilter(32), inSquarePlacement(), heightmapPlacement()}, closedEyeblossom());
    return decoration;
}

void paleGardenDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    const SimpleBlockFeature feature = closedEyeblossom();
    placeFrom(chunk, random, origin, {countPlacement(96), randomOffsetPlacement(7, 3), airTargetFilter()},
              [&chunk, &random, &feature](const DecorationPos& position) { feature.place(chunk, random, position); });
}

void paleGardenDecoration::placeSingleAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    closedEyeblossom().place(chunk, random, origin);
}

std::int32_t cherryGroveDecoration::pinkPetalsStateId(std::int32_t choice) {
    // parity: the weighted provider walks amount 1 through 4 crossed with the horizontal plane order
    // north, east, south, west, and those facings are not four consecutive state ids.
    static constexpr std::array<std::int32_t, 4> kHorizontalFacingStateOffsets = {0, 12, 4, 8};
    const std::int32_t amountOffset = choice / 4;
    const std::int32_t facingOffset = kHorizontalFacingStateOffsets[static_cast<std::size_t>(choice % 4)];
    return stateIdOf("pink_petals") + facingOffset + amountOffset;
}

const BiomeVegetalDecoration& cherryGroveDecoration::flowersOnly() {
    static const BiomeVegetalDecoration decoration =
        placedVegetation({noiseThresholdCountPlacement(-0.8, 5, 10), inSquarePlacement(), heightmapPlacement(),
                          countPlacement(96), randomOffsetPlacement(6, 2), airTargetFilter()},
                         pinkPetals());
    return decoration;
}

void cherryGroveDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    const SimpleBlockFeature feature = pinkPetals();
    placeFrom(chunk, random, origin, {countPlacement(96), randomOffsetPlacement(6, 2), airTargetFilter()},
              [&chunk, &random, &feature](const DecorationPos& position) { feature.place(chunk, random, position); });
}

} // namespace basalt::server::world::terrain::decoration
