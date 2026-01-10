#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/noise/SimplexNoise.hpp>
#include <basalt/server/world/terrain/decoration/PlainsFlowerFeatures.hpp>
#include <basalt/server/world/terrain/decoration/SimpleBlockFeature.hpp>

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::world::terrain::decoration {
namespace {

[[nodiscard]] std::int32_t stateIdOf(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"unknown block '" + std::string{path} + "'"};
    }
    return found->stateId;
}

[[nodiscard]] std::vector<std::int32_t> stateIdsOf(std::initializer_list<std::string_view> paths) {
    std::vector<std::int32_t> ids;
    ids.reserve(paths.size());
    for (const std::string_view path : paths) {
        ids.push_back(stateIdOf(path));
    }
    return ids;
}

const std::vector<std::int32_t>& highFlowerStates() {
    static const std::vector<std::int32_t> ids = stateIdsOf({"poppy", "azure_bluet", "oxeye_daisy", "cornflower"});
    return ids;
}

const std::vector<std::int32_t>& lowFlowerStates() {
    static const std::vector<std::int32_t> ids = stateIdsOf({"orange_tulip", "red_tulip", "pink_tulip", "white_tulip"});
    return ids;
}

/// parity: `Biome`'s own fixed-seed simplex, not the world seed and not a position hash.
const noise::SimplexNoise& flowerNoise() {
    static const noise::SimplexNoise built = [] {
        random::RandomSource source = random::RandomSource::legacy(2345LL);
        return noise::SimplexNoise{source};
    }();
    return built;
}

} // namespace

std::int32_t plainsDecoration::flowerStateId(random::RandomSource& random, const DecorationPos& origin) {
    const double noiseValue = flowerNoise().getValue(origin.x * 0.005, origin.z * 0.005);
    if (noiseValue < -0.8) {
        return lowFlowerStates()[static_cast<std::size_t>(
            random.nextInt(static_cast<std::int32_t>(lowFlowerStates().size())))];
    }
    if (random.nextFloat() < 1.0F / 3.0F) {
        return highFlowerStates()[static_cast<std::size_t>(
            random.nextInt(static_cast<std::int32_t>(highFlowerStates().size())))];
    }
    return stateIdOf("dandelion");
}

void plainsDecoration::placeFlowerPlainAt(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y,
                                          std::int32_t z) {
    const SimpleBlockFeature feature{[](random::RandomSource& providerRandom, const DecorationPos& position) {
        return flowerStateId(providerRandom, position);
    }};
    placeFrom(chunk, random, DecorationPos{x, y, z},
              {countPlacement(64), randomOffsetPlacement(6, 2), airTargetFilter()},
              [&chunk, &random, &feature](const DecorationPos& position) { feature.place(chunk, random, position); });
}

std::int32_t forestFloorDecoration::flowerBedStateId(std::int32_t defaultStateId, std::int32_t choice) {
    // parity: amount 1 through 4 crossed with the horizontal plane order north, east, south, west,
    // and those facings are not four consecutive state ids.
    static constexpr std::array<std::int32_t, 4> kHorizontalFacingStateOffsets = {0, 12, 4, 8};
    const std::int32_t amountOffset = choice / 4;
    const std::int32_t facingOffset = kHorizontalFacingStateOffsets[static_cast<std::size_t>(choice % 4)];
    return defaultStateId + facingOffset + amountOffset;
}

void forestFloorDecoration::placeWildflowersAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
                                               std::int32_t count) {
    const SimpleBlockFeature feature{[](random::RandomSource& providerRandom, const DecorationPos&) {
        return flowerBedStateId(stateIdOf("wildflowers"), providerRandom.nextInt(16));
    }};
    placeFrom(chunk, random, origin, {countPlacement(count), randomOffsetPlacement(6, 2), airTargetFilter()},
              [&chunk, &random, &feature](const DecorationPos& position) { feature.place(chunk, random, position); });
}

namespace {

/// parity: the leaf litter patch's own `block_predicate_filter`, which wants grass block below.
[[nodiscard]] PlacementModifier blockBelowFilter(std::int32_t stateId) {
    return filter([stateId](const PlacementContext& context, random::RandomSource&, const DecorationPos& origin) {
        return context.chunk().blockStateId(origin.x & 15, origin.y - 1, origin.z & 15) == stateId;
    });
}

[[nodiscard]] SimpleBlockFeature wildflowerFeature() {
    return SimpleBlockFeature{[](random::RandomSource& random, const DecorationPos&) {
        return forestFloorDecoration::flowerBedStateId(stateIdOf("wildflowers"), random.nextInt(16));
    }};
}

[[nodiscard]] std::int32_t leafLitterStateId(std::int32_t choice, std::int32_t maxSegmentAmount) {
    const std::int32_t segmentOffset = choice / 4;
    if (segmentOffset >= maxSegmentAmount) {
        throw std::logic_error{"leaf litter segment out of range"};
    }
    return forestFloorDecoration::flowerBedStateId(stateIdOf("leaf_litter"), choice);
}

[[nodiscard]] BiomeVegetalDecoration placed(std::vector<PlacementModifier> modifiers, SimpleBlockFeature feature) {
    return
        [modifiers = std::move(modifiers), feature = std::move(feature)](Chunk& chunk, random::RandomSource& random) {
            const DecorationPos origin{.x = chunk.chunkX() * 16, .y = 0, .z = chunk.chunkZ() * 16};
            placeFrom(chunk, random, origin, modifiers,
                      [&](const DecorationPos& position) { feature.place(chunk, random, position); });
        };
}

} // namespace

const BiomeVegetalDecoration& forestFloorDecoration::birchWildflowers() {
    static const BiomeVegetalDecoration decoration =
        placed({countPlacement(3), rarityFilter(2), inSquarePlacement(), heightmapPlacement(), countPlacement(64),
                randomOffsetPlacement(6, 2), airTargetFilter()},
               wildflowerFeature());
    return decoration;
}

const BiomeVegetalDecoration& forestFloorDecoration::meadowWildflowers() {
    static const BiomeVegetalDecoration decoration =
        placed({noiseThresholdCountPlacement(-0.8, 5, 10), inSquarePlacement(), heightmapPlacement(), countPlacement(8),
                randomOffsetPlacement(6, 2), airTargetFilter()},
               wildflowerFeature());
    return decoration;
}

const BiomeVegetalDecoration& forestFloorDecoration::leafLitter() {
    static const BiomeVegetalDecoration decoration = placed(
        {countPlacement(2), inSquarePlacement(), heightmapPlacement(HeightmapType::WorldSurface), countPlacement(32),
         randomOffsetPlacement(7, 3), airTargetFilter(), blockBelowFilter(stateIdOf("grass_block"))},
        SimpleBlockFeature{[](random::RandomSource& random, const DecorationPos&) {
            return leafLitterStateId(random.nextInt(12), 3);
        }});
    return decoration;
}

} // namespace basalt::server::world::terrain::decoration
