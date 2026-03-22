#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/SyntheticRouter.hpp"

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
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

[[nodiscard]] std::int32_t floorMod(std::int32_t value, std::int32_t divisor) {
    const std::int32_t rest = value % divisor;
    return rest < 0 ? rest + divisor : rest;
}

/// A biome layout that moves with position, so decoration has something to branch on.
class VaryingEnvironment final : public surface::SurfaceRuleEnvironment {
public:
    explicit VaryingEnvironment(std::vector<std::string> biomes) : biomes_(std::move(biomes)) {}

    [[nodiscard]] const std::string& biomeAt(std::int32_t x, std::int32_t y, std::int32_t z) const override {
        return biomes_[static_cast<std::size_t>(
            floorMod((x >> 2) + (y >> 2) + (z >> 2), static_cast<std::int32_t>(biomes_.size())))];
    }

    [[nodiscard]] const std::string& noiseBiomeAt(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ,
                                                  densityfunction::SamplerContext*) const override {
        return biomes_[static_cast<std::size_t>(
            floorMod(quartX + quartY + quartZ, static_cast<std::int32_t>(biomes_.size())))];
    }

private:
    std::vector<std::string> biomes_;
};

[[nodiscard]] std::vector<std::string> biomeNames(std::int32_t rotation) {
    std::vector<std::string> all{"minecraft:desert", "minecraft:jungle", "minecraft:snowy_plains", "minecraft:swamp"};
    std::rotate(all.begin(), all.begin() + rotation, all.end());
    return all;
}

/// A rolling floor, interpolated so the dimension's cell sizes matter.
[[nodiscard]] std::string terrainDensity(std::int32_t low, std::int32_t high, std::int32_t cellY) {
    return R"({"type": "minecraft:interpolated", "cell_size_xz": 4, "cell_size_y": )" + std::to_string(cellY) +
           R"(, "input": )" + testsupport::rollingFloor(low, high, 0.6, 0.3) + "}";
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
    const NoiseRouter router = testsupport::syntheticRouter(seed, terrainDensity(40, 90, 8));
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

[[nodiscard]] Run runNether(std::int64_t seed) {
    const NoiseRouter router = testsupport::syntheticRouter(seed, terrainDensity(0, 100, 8),
                                                            {.minY = 0,
                                                             .height = 128,
                                                             .seaLevel = 32,
                                                             .defaultBlock = "minecraft:netherrack",
                                                             .defaultFluid = "minecraft:lava"});
    const NetherBiomeSource biomeSource{router};
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t index = 0; index < 4; ++index) {
        const Chunk chunk = dimensionTerrainGenerator::nether(router, biomeSource, index % 2, index / 2, seed);
        const auto [chunkChecksum, solid, biomes] = fold(chunk, 0, 128);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.solid += solid;
        result.biomes += biomes;
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

[[nodiscard]] Run runEnd(std::int64_t seed) {
    const NoiseRouter router = testsupport::syntheticRouter(
        seed, terrainDensity(0, 100, 4), {.minY = 0, .height = 128, .defaultBlock = "minecraft:end_stone"});
    const TheEndBiomeSource biomeSource{router};
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
    SUBCASE("plains") { check(runOverworld(1234, 0), 129735, 6144, 5662884701126142804LL); }

    SUBCASE("desert") { check(runOverworld(1234, 1), 129742, 6144, 5992392677590901482LL); }

    SUBCASE("otherSeed") { check(runOverworld(77, 0), 145682, 6144, 6519172991108402450LL); }

    SUBCASE("nether") { check(runNether(1234), 36938, 4096, -277241547479252943LL); }

    SUBCASE("netherOther") { check(runNether(77), 45062, 4096, -4508100663735340715LL); }

    SUBCASE("end") { check(runEnd(1234), 65632, 4096, 334055278663270597LL); }

    SUBCASE("endOther") { check(runEnd(77), 64712, 4096, -8596209259509102991LL); }
}
