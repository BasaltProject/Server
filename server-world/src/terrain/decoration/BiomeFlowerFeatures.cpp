#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/noise/NormalNoise.hpp>
#include <basalt/server/world/terrain/decoration/BiomeFlowerFeatures.hpp>

#include <algorithm>
#include <array>
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

/// A fixed-seed noise built off the legacy source, the way `Biome`'s own static fields are.
[[nodiscard]] noise::NormalNoise fixedNoise(std::int64_t seed, std::int32_t firstOctave) {
    random::RandomSource source = random::RandomSource::legacy(seed);
    const std::array<double, 1> amplitudes = {1.0};
    return noise::NormalNoise::create(source, firstOctave, amplitudes);
}

/// parity: the index is taken from the noise mapped into [0, 0.9999], so the list order decides
/// which flower a position gets and reordering it silently changes the world.
[[nodiscard]] std::int32_t stateFromNoise(const std::vector<std::int32_t>& states, double noiseValue) {
    const double placement = std::clamp((1.0 + noiseValue) / 2.0, 0.0, 0.9999);
    return states[static_cast<std::size_t>(placement * static_cast<double>(states.size()))];
}

const std::vector<std::int32_t>& forestFlowerStates() {
    static const std::vector<std::int32_t> ids =
        stateIdsOf({"dandelion", "poppy", "allium", "azure_bluet", "red_tulip", "orange_tulip", "white_tulip",
                    "pink_tulip", "oxeye_daisy", "cornflower", "lily_of_the_valley"});
    return ids;
}

const noise::NormalNoise& forestFlowerNoise() {
    static const noise::NormalNoise built = fixedNoise(2345LL, 0);
    return built;
}

constexpr double kForestFlowerScale = 0.020833334;

const std::vector<std::int32_t>& meadowFlowerStates() {
    static const std::vector<std::int32_t> ids = stateIdsOf(
        {"tall_grass", "allium", "poppy", "azure_bluet", "dandelion", "cornflower", "oxeye_daisy", "short_grass"});
    return ids;
}

const noise::NormalNoise& meadowFastNoise() {
    static const noise::NormalNoise built = fixedNoise(2345LL, -3);
    return built;
}

const noise::NormalNoise& meadowSlowNoise() {
    static const noise::NormalNoise built = fixedNoise(2345LL, -10);
    return built;
}

[[nodiscard]] double clampedMap(double value, double outputMin, double outputMax) {
    return outputMin + std::clamp((value + 1.0) / 2.0, 0.0, 1.0) * (outputMax - outputMin);
}

} // namespace

std::int32_t flowerForestDecoration::flowerAt(std::int32_t x, std::int32_t y, std::int32_t z) {
    const double noiseValue =
        forestFlowerNoise().getValue(x * kForestFlowerScale, y * kForestFlowerScale, z * kForestFlowerScale);
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
    const double slowValue = meadowSlowNoise().getValue(position.x, position.y, position.z);
    const auto localVariety = static_cast<std::int32_t>(clampedMap(slowValue, 1.0, 4.0));
    std::vector<std::int32_t> possible;
    possible.reserve(static_cast<std::size_t>(localVariety));
    for (std::int32_t index = 0; index < localVariety; ++index) {
        // parity: the slow noise is resampled at a large fixed stride per index, which is what gives
        // one meadow a handful of species rather than all eight everywhere.
        const double value =
            meadowSlowNoise().getValue(position.x + index * 54545, position.y, position.z + index * 34234);
        possible.push_back(stateFromNoise(meadowFlowerStates(), value));
    }
    return stateFromNoise(possible, meadowFastNoise().getValue(position.x, position.y, position.z));
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

} // namespace basalt::server::world::terrain::decoration
