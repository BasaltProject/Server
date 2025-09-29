#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/noise/NormalNoise.hpp>
#include <basalt/server/world/terrain/decoration/ForestFloorDecoration.hpp>
#include <basalt/server/world/terrain/decoration/MeadowDecoration.hpp>
#include <basalt/server/world/terrain/decoration/SimpleBlockFeature.hpp>
#include <basalt/server/world/terrain/decoration/TreeDecoration.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeConfigs.hpp>

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
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

const std::vector<std::int32_t>& flowerStateIds() {
    static const std::vector<std::int32_t> states = {
        stateIdOf("tall_grass"), stateIdOf("allium"),     stateIdOf("poppy"),       stateIdOf("azure_bluet"),
        stateIdOf("dandelion"),  stateIdOf("cornflower"), stateIdOf("oxeye_daisy"), stateIdOf("short_grass")};
    return states;
}

/// parity: `DualNoiseProvider`'s two `NormalNoise` instances, both seeded 2345 at different octaves.
const noise::NormalNoise& fastNoise() {
    static const noise::NormalNoise noise = [] {
        random::RandomSource source = random::RandomSource::legacy(2345LL);
        constexpr std::array<double, 1> amplitudes{1.0};
        return noise::NormalNoise::create(source, -3, amplitudes);
    }();
    return noise;
}

const noise::NormalNoise& slowNoise() {
    static const noise::NormalNoise noise = [] {
        random::RandomSource source = random::RandomSource::legacy(2345LL);
        constexpr std::array<double, 1> amplitudes{1.0};
        return noise::NormalNoise::create(source, -10, amplitudes);
    }();
    return noise;
}

[[nodiscard]] double slowNoiseAt(std::int32_t x, std::int32_t y, std::int32_t z) {
    return slowNoise().getValue(static_cast<double>(x), static_cast<double>(y), static_cast<double>(z));
}

[[nodiscard]] std::int32_t stateFromNoise(const std::vector<std::int32_t>& states, double noiseValue) {
    const double placement = std::clamp((1.0 + noiseValue) / 2.0, 0.0, 0.9999);
    return states[static_cast<std::size_t>(placement * static_cast<double>(states.size()))];
}

[[nodiscard]] double clampedMap(double value, double outputMin, double outputMax) {
    return outputMin + std::clamp((value + 1.0) / 2.0, 0.0, 1.0) * (outputMax - outputMin);
}

void placeFlower(Chunk& chunk, random::RandomSource& random, const DecorationPos& position) {
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

} // namespace

std::int32_t meadowDecoration::flowerStateAt(const DecorationPos& position) {
    const double slowValue = slowNoiseAt(position.x, position.y, position.z);
    const auto localVariety = static_cast<std::size_t>(static_cast<std::int32_t>(clampedMap(slowValue, 1.0, 4.0)));
    std::vector<std::int32_t> possible;
    possible.reserve(localVariety);
    for (std::size_t index = 0; index < localVariety; ++index) {
        const auto step = static_cast<std::int32_t>(index);
        possible.push_back(stateFromNoise(
            flowerStateIds(), slowNoiseAt(position.x + step * 54545, position.y, position.z + step * 34234)));
    }
    return stateFromNoise(possible,
                          fastNoise().getValue(static_cast<double>(position.x), static_cast<double>(position.y),
                                               static_cast<double>(position.z)));
}

void meadowDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    const std::vector<PlacementModifier> modifiers{countPlacement(96), randomOffsetPlacement(6, 2), airTargetFilter()};
    placeFrom(chunk, random, origin, modifiers,
              [&](const DecorationPos& position) { placeFlower(chunk, random, position); });
}

const BiomeVegetalDecoration& meadowDecoration::flowers() {
    static const BiomeVegetalDecoration decoration = placed(
        {inSquarePlacement(), heightmapPlacement(), countPlacement(96), randomOffsetPlacement(6, 2), airTargetFilter()},
        placeFlower);
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
