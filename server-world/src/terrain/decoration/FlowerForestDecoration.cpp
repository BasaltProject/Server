#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/noise/NormalNoise.hpp>
#include <basalt/server/world/terrain/decoration/FlowerForestDecoration.hpp>
#include <basalt/server/world/terrain/decoration/SimpleBlockFeature.hpp>
#include <basalt/server/world/terrain/decoration/TreeDecoration.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeConfigs.hpp>

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::world::terrain::decoration {
namespace {

// parity: NoiseBasedStateProvider(seed 2345, NoiseParameters(0, [1.0]), 0.020833334F).
constexpr double kScale = 0.020833334;

[[nodiscard]] std::int32_t stateIdOf(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"unknown block '" + std::string{path} + "'"};
    }
    return found->stateId;
}

// The order matters: the noise index selects into this list.
const std::vector<std::int32_t>& flowerStateIds() {
    static const std::vector<std::int32_t> states = {stateIdOf("dandelion"),
                                                     stateIdOf("poppy"),
                                                     stateIdOf("allium"),
                                                     stateIdOf("azure_bluet"),
                                                     stateIdOf("red_tulip"),
                                                     stateIdOf("orange_tulip"),
                                                     stateIdOf("white_tulip"),
                                                     stateIdOf("pink_tulip"),
                                                     stateIdOf("oxeye_daisy"),
                                                     stateIdOf("cornflower"),
                                                     stateIdOf("lily_of_the_valley")};
    return states;
}

const noise::NormalNoise& flowerNoise() {
    static const noise::NormalNoise noise = [] {
        random::RandomSource source = random::RandomSource::legacy(2345LL);
        constexpr std::array<double, 1> amplitudes{1.0};
        return noise::NormalNoise::create(source, 0, amplitudes);
    }();
    return noise;
}

[[nodiscard]] SimpleBlockFeature flowerFeature() {
    return SimpleBlockFeature{[](random::RandomSource&, const DecorationPos& position) {
        return flowerForestDecoration::flowerAt(position.x, position.y, position.z);
    }};
}

// parity: placed_feature/flower_flower_forest.
void placeFlowers(Chunk& chunk, random::RandomSource& random) {
    const SimpleBlockFeature feature = flowerFeature();
    const DecorationPos origin{.x = chunk.chunkX() * 16, .y = 0, .z = chunk.chunkZ() * 16};
    const std::vector<PlacementModifier> modifiers{
        countPlacement(3),           rarityFilter(2),  inSquarePlacement(), heightmapPlacement(), countPlacement(96),
        randomOffsetPlacement(6, 2), airTargetFilter()};
    placeFrom(chunk, random, origin, modifiers,
              [&](const DecorationPos& position) { feature.place(chunk, random, position); });
}

// parity: VegetationFeatures.TREES_FLOWER_FOREST with treePlacement(countExtra(6, 0.1, 1)).
const BiomeVegetalDecoration& trees() {
    static const BiomeVegetalDecoration decoration = treeDecoration::candidateSequenceCountExtra(
        {{0.0025F, treeDecoration::fallen(tree::fallenTreeConfigs::birch())},
         {0.2F, treeDecoration::tree(tree::treeConfigs::birchBees002())},
         {0.1F, treeDecoration::tree(tree::treeConfigs::fancyOakBees002())}},
        treeDecoration::tree(tree::treeConfigs::oakBees002()), 6, 0.1F, 1);
    return decoration;
}

} // namespace

std::int32_t flowerForestDecoration::flowerAt(const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    const double noiseValue = flowerNoise().getValue(static_cast<double>(x) * kScale, static_cast<double>(y) * kScale,
                                                     static_cast<double>(z) * kScale);
    const double placementValue = std::max(0.0, std::min((1.0 + noiseValue) / 2.0, 0.9999));
    return flowerStateIds()[static_cast<std::size_t>(placementValue * static_cast<double>(flowerStateIds().size()))];
}

void flowerForestDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    const SimpleBlockFeature feature = flowerFeature();
    const std::vector<PlacementModifier> modifiers{countPlacement(96), randomOffsetPlacement(6, 2), airTargetFilter()};
    placeFrom(chunk, random, origin, modifiers,
              [&](const DecorationPos& position) { feature.place(chunk, random, position); });
}

const BiomeVegetalDecoration& flowerForestDecoration::flowersOnly() {
    static const BiomeVegetalDecoration decoration = placeFlowers;
    return decoration;
}

void flowerForestDecoration::decorate(Chunk& chunk, random::RandomSource& random) {
    trees()(chunk, random);
    placeFlowers(chunk, random);
}

} // namespace basalt::server::world::terrain::decoration
