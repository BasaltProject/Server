#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/noise/Noise.hpp>
#include <basalt/server/world/noise/NormalNoise.hpp>
#include <basalt/server/world/terrain/decoration/BiomeFlowerFeatures.hpp>
#include <basalt/server/world/terrain/decoration/PlainsFlowerFeatures.hpp>
#include <basalt/server/world/terrain/decoration/TreeDecoration.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeConfigs.hpp>

#include <algorithm>
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

/// parity: the providers' noise, a one-octave parity noise seeded off the legacy source.
[[nodiscard]] std::shared_ptr<const noise::NoiseStack> fixedNoise(std::int64_t seed, std::int32_t firstOctave) {
    random::RandomSource source = random::RandomSource::legacy(seed);
    const std::array<double, 1> amplitudes = {1.0};
    return noise::NormalNoise::createParity(firstOctave, amplitudes).create(source);
}

/// parity: `NoiseProvider.getRandomState`.
[[nodiscard]] std::int32_t stateFromNoise(const std::vector<std::int32_t>& states, float noiseValue) {
    const float placement = std::clamp((1.0F + noiseValue) / 2.0F, 0.0F, 0.9999F);
    return states[static_cast<std::size_t>(placement * static_cast<float>(states.size()))];
}

const std::vector<std::int32_t>& forestFlowerStates() {
    static const std::vector<std::int32_t> ids =
        stateIdsOf({"dandelion", "poppy", "allium", "azure_bluet", "red_tulip", "orange_tulip", "white_tulip",
                    "pink_tulip", "oxeye_daisy", "cornflower", "lily_of_the_valley"});
    return ids;
}

const noise::Noise& forestFlowerNoise() {
    static const std::shared_ptr<const noise::NoiseStack> built = fixedNoise(2345LL, 0);
    return *built;
}

// parity: the provider's float scale, widened.
constexpr auto kForestFlowerScale = static_cast<double>(0.020833334F);

const std::vector<std::int32_t>& meadowFlowerStates() {
    static const std::vector<std::int32_t> ids = stateIdsOf(
        {"tall_grass", "allium", "poppy", "azure_bluet", "dandelion", "cornflower", "oxeye_daisy", "short_grass"});
    return ids;
}

const noise::Noise& meadowFastNoise() {
    static const std::shared_ptr<const noise::NoiseStack> built = fixedNoise(2345LL, -3);
    return *built;
}

const noise::Noise& meadowSlowNoise() {
    static const std::shared_ptr<const noise::NoiseStack> built = fixedNoise(2345LL, -10);
    return *built;
}

[[nodiscard]] double clampedMap(double value, double outputMin, double outputMax) {
    return outputMin + std::clamp((value + 1.0) / 2.0, 0.0, 1.0) * (outputMax - outputMin);
}

} // namespace

std::int32_t flowerForestDecoration::flowerAt(std::int32_t x, std::int32_t y, std::int32_t z) {
    const float noiseValue =
        forestFlowerNoise().get(x * kForestFlowerScale, y * kForestFlowerScale, z * kForestFlowerScale);
    return stateFromNoise(forestFlowerStates(), noiseValue);
}

void flowerForestDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    const SimpleBlockFeature feature{[](random::RandomSource&, const DecorationPos& position) {
        return flowerAt(position.x, position.y, position.z);
    }};
    placeFrom(chunk, random, origin, {countPlacement(96), randomOffsetPlacement(6, 2), airTargetFilter()},
              [&chunk, &random, &feature](const DecorationPos& position) { feature.place(chunk, random, position); });
}

void swampDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    const SimpleBlockFeature feature{
        [](random::RandomSource&, const DecorationPos&) { return stateIdOf("blue_orchid"); }};
    placeFrom(chunk, random, origin, {countPlacement(64), randomOffsetPlacement(6, 2), airTargetFilter()},
              [&chunk, &random, &feature](const DecorationPos& position) { feature.place(chunk, random, position); });
}

std::int32_t meadowDecoration::flowerStateAt(const DecorationPos& position) {
    const auto slowValue = static_cast<double>(meadowSlowNoise().get(position.x, position.y, position.z));
    const auto localVariety = static_cast<std::int32_t>(clampedMap(slowValue, 1.0, 4.0));
    std::vector<std::int32_t> possible;
    possible.reserve(static_cast<std::size_t>(localVariety));
    for (std::int32_t index = 0; index < localVariety; ++index) {
        // parity: the slow noise is resampled at a large fixed stride per index.
        const float value = meadowSlowNoise().get(position.x + index * 54545, position.y, position.z + index * 34234);
        possible.push_back(stateFromNoise(meadowFlowerStates(), value));
    }
    return stateFromNoise(possible, meadowFastNoise().get(position.x, position.y, position.z));
}

void meadowDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    placeFrom(chunk, random, origin, {countPlacement(96), randomOffsetPlacement(6, 2), airTargetFilter()},
              [&chunk, &random](const DecorationPos& position) {
                  const std::int32_t stateId = flowerStateAt(position);
                  const SimpleBlockFeature feature{
                      [stateId](random::RandomSource&, const DecorationPos&) { return stateId; },
                      stateId == stateIdOf("tall_grass")};
                  feature.place(chunk, random, position);
              });
}

namespace {

void placeMeadowFlower(Chunk& chunk, random::RandomSource& random, const DecorationPos& position) {
    const std::int32_t stateId = meadowDecoration::flowerStateAt(position);
    const SimpleBlockFeature feature{[stateId](random::RandomSource&, const DecorationPos&) { return stateId; },
                                     stateId == stateIdOf("tall_grass")};
    feature.place(chunk, random, position);
}

[[nodiscard]] BiomeVegetalDecoration
placed(std::vector<PlacementModifier> modifiers,
       std::function<void(Chunk&, random::RandomSource&, const DecorationPos&)> place) {
    return [modifiers = std::move(modifiers), place = std::move(place)](Chunk& chunk, random::RandomSource& random) {
        const DecorationPos origin{.x = chunk.chunkX() * 16, .y = 0, .z = chunk.chunkZ() * 16};
        placeFrom(chunk, random, origin, modifiers,
                  [&](const DecorationPos& position) { place(chunk, random, position); });
    };
}

// parity: VegetationFeatures.TREES_FLOWER_FOREST with treePlacement(countExtra(6, 0.1, 1)).
const BiomeVegetalDecoration& flowerForestTrees() {
    static const BiomeVegetalDecoration decoration = treeDecoration::candidateSequenceCountExtra(
        {{0.0025F, treeDecoration::fallen(tree::fallenTreeConfigs::birch())},
         {0.2F, treeDecoration::tree(tree::treeConfigs::birchBees002())},
         {0.1F, treeDecoration::tree(tree::treeConfigs::fancyOakBees002())}},
        treeDecoration::tree(tree::treeConfigs::oakBees002()), 6, 0.1F, 1);
    return decoration;
}

} // namespace

const BiomeVegetalDecoration& flowerForestDecoration::flowersOnly() {
    static const BiomeVegetalDecoration decoration =
        placed({countPlacement(3), rarityFilter(2), inSquarePlacement(), heightmapPlacement(), countPlacement(96),
                randomOffsetPlacement(6, 2), airTargetFilter()},
               [](Chunk& chunk, random::RandomSource& random, const DecorationPos& position) {
                   const SimpleBlockFeature feature{[](random::RandomSource&, const DecorationPos& at) {
                       return flowerForestDecoration::flowerAt(at.x, at.y, at.z);
                   }};
                   feature.place(chunk, random, position);
               });
    return decoration;
}

void flowerForestDecoration::decorate(Chunk& chunk, random::RandomSource& random) {
    flowerForestTrees()(chunk, random);
    flowersOnly()(chunk, random);
}

const BiomeVegetalDecoration& meadowDecoration::flowers() {
    static const BiomeVegetalDecoration decoration = placed(
        {inSquarePlacement(), heightmapPlacement(), countPlacement(96), randomOffsetPlacement(6, 2), airTargetFilter()},
        placeMeadowFlower);
    return decoration;
}

const BiomeVegetalDecoration& meadowDecoration::grass() {
    static const BiomeVegetalDecoration decoration =
        placed({noiseThresholdCountPlacement(-0.8, 5, 10), inSquarePlacement(), heightmapPlacement(),
                countPlacement(16), randomOffsetPlacement(7, 3), airTargetFilter()},
               [](Chunk& chunk, random::RandomSource& random, const DecorationPos& position) {
                   const SimpleBlockFeature feature{
                       [](random::RandomSource&, const DecorationPos&) { return stateIdOf("short_grass"); }};
                   feature.place(chunk, random, position);
               });
    return decoration;
}

const BiomeVegetalDecoration& meadowDecoration::trees() {
    static const BiomeVegetalDecoration decoration =
        treeDecoration::rarityChoice(tree::treeConfigs::fancyOakBees(), 0.5F, tree::treeConfigs::superBirchBees(), 100);
    return decoration;
}

const BiomeVegetalDecoration& meadowDecoration::withoutTrees() {
    static const BiomeVegetalDecoration decoration = combine({flowers(), forestFloorDecoration::meadowWildflowers()});
    return decoration;
}

void meadowDecoration::decorate(Chunk& chunk, random::RandomSource& random) {
    flowers()(chunk, random);
    trees()(chunk, random);
    forestFloorDecoration::meadowWildflowers()(chunk, random);
}

} // namespace basalt::server::world::terrain::decoration
