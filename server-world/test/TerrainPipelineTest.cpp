#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/noise/NormalNoise.hpp>
#include <basalt/server/world/terrain/DimensionTerrainGenerator.hpp>
#include <basalt/server/world/terrain/RealTerrainGenerator.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <doctest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain;
namespace registry = basalt::server::registry;
namespace df = basalt::server::world::densityfunction;

namespace {

/// FNV-1a over a name, folded small: a stable per-noise offset both implementations reproduce.
[[nodiscard]] std::int32_t nameOffset(std::string_view name) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const char character : name) {
        hash = (hash ^ static_cast<std::uint64_t>(static_cast<unsigned char>(character))) * 1099511628211ULL;
    }
    return static_cast<std::int32_t>((static_cast<std::uint32_t>(hash >> 32) & 0x7FFFFFFFU) % 97U);
}

[[nodiscard]] std::int32_t floorMod(std::int32_t value, std::int32_t divisor) {
    const std::int32_t rest = value % divisor;
    return rest < 0 ? rest + divisor : rest;
}

/// An environment whose every answer moves with position. A flat one leaves the surface rules, the
/// steepness test and most of decoration with nothing to branch on, and then the phases can be
/// reordered without the result changing.
class VaryingEnvironment final : public surface::SurfaceRuleEnvironment {
public:
    explicit VaryingEnvironment(std::vector<std::string> biomes) : biomes_(std::move(biomes)) {}

    [[nodiscard]] std::int32_t surfaceDepthNoise(std::int32_t x, std::int32_t z) const override {
        return floorMod(x * 7 + z * 3, 5) - 2;
    }

    [[nodiscard]] double surfaceSecondaryNoise(std::int32_t x, std::int32_t z) const override {
        return floorMod(x * 3 - z, 11) / 10.0 - 0.5;
    }

    [[nodiscard]] double sample2d(std::string_view noiseId, std::int32_t x, std::int32_t z) const override {
        return floorMod(nameOffset(noiseId) + x * 5 + z * 7, 21) / 10.0 - 1.0;
    }

    [[nodiscard]] double sample3d(std::string_view noiseId, std::int32_t x, std::int32_t y,
                                  std::int32_t z) const override {
        return floorMod(nameOffset(noiseId) + x * 5 + y * 11 + z * 7, 21) / 10.0 - 1.0;
    }

    [[nodiscard]] float verticalGradientRandom(std::string_view name, std::int32_t x, std::int32_t y,
                                               std::int32_t z) const override {
        return static_cast<float>(floorMod(nameOffset(name) + x + y * 3 + z * 5, 100)) / 100.0F;
    }

    [[nodiscard]] basalt::server::random::RandomSource positionalRandom(std::int32_t x, std::int32_t y,
                                                                        std::int32_t z) const override {
        return basalt::server::random::RandomSource::xoroshiro(x * 31LL + y * 17LL + z);
    }

    [[nodiscard]] std::int32_t estimatedSurfaceHeight(std::int32_t x, std::int32_t z) const override {
        return 62 + floorMod(x * 2 + z, 9);
    }

    [[nodiscard]] const std::string& biomeAt(std::int32_t x, std::int32_t y, std::int32_t z) const override {
        return biomes_[static_cast<std::size_t>(
            floorMod((x >> 2) + (y >> 2) + (z >> 2), static_cast<std::int32_t>(biomes_.size())))];
    }

    [[nodiscard]] const std::string& noiseBiomeAt(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ,
                                                  densityfunction::DensityCache*) const override {
        return biomes_[static_cast<std::size_t>(
            floorMod(quartX + quartY + quartZ, static_cast<std::int32_t>(biomes_.size())))];
    }

    [[nodiscard]] bool isSteep(std::int32_t, std::int32_t) const override { return false; }

    [[nodiscard]] std::int32_t clayBand(std::int32_t x, std::int32_t y, std::int32_t z) const override {
        return floorMod(x + y + z, 4);
    }

private:
    std::vector<std::string> biomes_;
};

[[nodiscard]] std::vector<std::string> biomeNames(std::int32_t rotation) {
    std::vector<std::string> all{"minecraft:desert", "minecraft:jungle", "minecraft:snowy_plains", "minecraft:swamp"};
    std::rotate(all.begin(), all.begin() + rotation, all.end());
    return all;
}

/// A rolling floor around y 60. Interpolated on purpose: without the marker the sampler evaluates
/// every block and the cell sizes a dimension's settings choose stop mattering.
[[nodiscard]] df::Function terrainDensity(std::int64_t seed, std::int32_t low, std::int32_t high) {
    basalt::server::random::RandomSource source = basalt::server::random::RandomSource::xoroshiro(seed);
    static constexpr std::array<double, 2> kAmplitudes{1.0, 1.0};
    auto noise = std::make_shared<const noise::NormalNoise>(noise::NormalNoise::create(source, -6, kAmplitudes));
    return df::functions::interpolated(
        df::functions::add(df::functions::yClampedGradient(low, high, 1.0, -1.0),
                           df::functions::mul(df::functions::constant(2.0), df::functions::noise(noise, 0.6, 0.3))));
}

[[nodiscard]] OverworldNoiseRouter overworldRouter(std::int64_t seed) {
    const df::Function zero = df::functions::zero();
    return OverworldNoiseRouter{.barrierNoise = zero,
                                .fluidLevelFloodednessNoise = df::functions::constant(-0.2),
                                .fluidLevelSpreadNoise = zero,
                                .lavaNoise = zero,
                                .shiftX = zero,
                                .shiftZ = zero,
                                .temperature = zero,
                                .vegetation = zero,
                                .continents = zero,
                                .erosion = zero,
                                .depth = zero,
                                .ridges = zero,
                                .preliminarySurfaceLevel = df::functions::constant(64.0),
                                .finalDensity = terrainDensity(seed, 40, 90),
                                .veinToggle = df::functions::yClampedGradient(-60, 60, 1.0, -1.0),
                                .veinRidged = df::functions::constant(1.0),
                                .veinGap = df::functions::constant(-0.5)};
}

[[nodiscard]] std::tuple<std::int64_t, std::int32_t, std::int32_t> fold(const Chunk& chunk, std::int32_t lowY,
                                                                        std::int32_t highY) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t solid = 0;
    for (std::int32_t y = lowY; y <= highY; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                if (id != registry::blocks::air().stateId) {
                    ++solid;
                }
            }
        }
    }
    std::int32_t biomes = 0;
    for (std::int32_t quartY = 0; quartY < chunk.verticalBounds().height() / 4; ++quartY) {
        for (std::int32_t quartX = 0; quartX < 4; ++quartX) {
            for (std::int32_t quartZ = 0; quartZ < 4; ++quartZ) {
                const std::int32_t id = chunk.biomeId(quartX, quartY, quartZ);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                if (id != 0) {
                    ++biomes;
                }
            }
        }
    }
    return {static_cast<std::int64_t>(checksum), solid, biomes};
}

struct Run {
    std::int32_t solid = 0;
    std::int32_t biomes = 0;
    std::int64_t checksum = 0;
};

[[nodiscard]] Run runOverworld(std::int64_t seed, std::int32_t rotation) {
    const OverworldNoiseRouter router = overworldRouter(seed);
    const VaryingEnvironment environment{biomeNames(rotation)};
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t index = 0; index < 4; ++index) {
        const Chunk chunk = realTerrainGenerator::generate(router, environment, index % 2, index / 2, seed);
        const auto [chunkChecksum, solid, biomes] = fold(chunk, -64, 200);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.solid += solid;
        result.biomes += biomes;
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

[[nodiscard]] Run runNether(std::int64_t seed, double temperature, double vegetation) {
    const NetherNoiseRouter router{.temperature = df::functions::constant(temperature),
                                   .vegetation = df::functions::constant(vegetation),
                                   .finalDensity = terrainDensity(seed, 0, 100)};
    const NetherBiomeSource biomeSource{router.temperature, router.vegetation};
    const VaryingEnvironment environment{{"minecraft:nether_wastes"}};
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t index = 0; index < 4; ++index) {
        const Chunk chunk =
            dimensionTerrainGenerator::nether(router, biomeSource, index % 2, index / 2, &environment, seed);
        const auto [chunkChecksum, solid, biomes] = fold(chunk, 0, 128);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.solid += solid;
        result.biomes += biomes;
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

[[nodiscard]] Run runEnd(std::int64_t seed) {
    const EndNoiseRouter router{.erosion = df::functions::constant(0.0), .finalDensity = terrainDensity(seed, 0, 100)};
    const TheEndBiomeSource biomeSource{router.erosion};
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t index = 0; index < 4; ++index) {
        const Chunk chunk = dimensionTerrainGenerator::end(router, biomeSource, index % 2, index / 2, seed);
        const auto [chunkChecksum, solid, biomes] = fold(chunk, 0, 128);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.solid += solid;
        result.biomes += biomes;
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

void check(const Run& result, std::int32_t solid, std::int32_t biomes, std::int64_t checksum) {
    CHECK(result.solid == solid);
    CHECK(result.biomes == biomes);
    CHECK(result.checksum == checksum);
}

} // namespace

TEST_CASE("the pipelines generate what the jar generates") {
    SUBCASE("plains") { check(runOverworld(1234, 0), 126910, 6144, 6779609785803754535LL); }

    SUBCASE("desert") { check(runOverworld(1234, 1), 126919, 6144, -6765849239462433688LL); }

    SUBCASE("otherSeed") { check(runOverworld(77, 0), 116477, 6144, -4215622627912708396LL); }

    SUBCASE("nether") { check(runNether(1234, 0.0, 0.0), 33082, 4096, -4176307358586819173LL); }

    SUBCASE("netherOther") { check(runNether(77, 0.4, 0.0), 34390, 4096, -36632428756019527LL); }

    SUBCASE("end") { check(runEnd(1234), 16295, 4096, -1846996668184024408LL); }

    SUBCASE("endOther") { check(runEnd(77), 25641, 4096, -2211422125057781826LL); }
}
