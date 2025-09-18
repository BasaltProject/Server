#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/noise/NormalNoise.hpp>
#include <basalt/server/world/terrain/NoiseRouterData.hpp>
#include <basalt/server/world/terrain/carver/OverworldCarvers.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleData.hpp>

#include <doctest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain;
namespace registry = basalt::server::registry;
namespace fn = basalt::server::world::densityfunction::functions;

namespace {

std::shared_ptr<const noise::NormalNoise> syntheticNoise(std::string_view id) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const char character : id) {
        hash = (hash ^ static_cast<std::uint64_t>(static_cast<unsigned char>(character))) * 1099511628211ULL;
    }
    basalt::server::random::RandomSource source =
        basalt::server::random::RandomSource::xoroshiro(static_cast<std::int64_t>(hash));
    const std::array<double, 3> amplitudes = {1.0, 0.6, 0.3};
    return std::make_shared<const noise::NormalNoise>(
        noise::NormalNoise::create(source, -(static_cast<std::int32_t>(id.size()) % 5 + 1), amplitudes));
}

[[nodiscard]] double fake(std::string_view name, std::int32_t x, std::int32_t y, std::int32_t z) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const char character : name) {
        hash = (hash ^ static_cast<std::uint64_t>(static_cast<unsigned char>(character))) * 1099511628211ULL;
    }
    hash = (hash ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(x))) * 1099511628211ULL;
    hash = (hash ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(y))) * 1099511628211ULL;
    hash = (hash ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(z))) * 1099511628211ULL;
    return std::sin(static_cast<double>(static_cast<std::int64_t>(hash)) * 1.0E-17);
}

class FakeEnvironment final : public surface::SurfaceRuleEnvironment {
public:
    [[nodiscard]] std::int32_t surfaceDepthNoise(std::int32_t x, std::int32_t z) const override {
        return ((x * 31 + z * 17) & 7) - 2;
    }

    [[nodiscard]] double surfaceSecondaryNoise(std::int32_t x, std::int32_t z) const override {
        return fake("secondary", x, 0, z);
    }

    [[nodiscard]] double sample2d(std::string_view noiseId, std::int32_t x, std::int32_t z) const override {
        return fake(noiseId, x, 0, z);
    }

    [[nodiscard]] double sample3d(std::string_view noiseId, std::int32_t x, std::int32_t y,
                                  std::int32_t z) const override {
        return fake(noiseId, x, y, z);
    }

    [[nodiscard]] float verticalGradientRandom(std::string_view name, std::int32_t x, std::int32_t y,
                                               std::int32_t z) const override {
        return static_cast<float>(std::abs(fake(name, x, y, z)));
    }

    [[nodiscard]] basalt::server::random::RandomSource positionalRandom(std::int32_t x, std::int32_t,
                                                                        std::int32_t z) const override {
        return basalt::server::random::RandomSource::xoroshiro(x * 31LL + z);
    }

    [[nodiscard]] std::int32_t estimatedSurfaceHeight(std::int32_t x, std::int32_t z) const override {
        return 64 + ((x * 7 + z * 13) & 31);
    }

    [[nodiscard]] const std::string& biomeAt(std::int32_t, std::int32_t, std::int32_t) const override {
        static const std::string plains = "minecraft:plains";
        return plains;
    }

    [[nodiscard]] const std::string& noiseBiomeAt(std::int32_t, std::int32_t, std::int32_t,
                                                  densityfunction::DensityCache*) const override {
        static const std::string plains = "minecraft:plains";
        return plains;
    }

    [[nodiscard]] bool isSteep(std::int32_t x, std::int32_t z) const override { return ((x ^ z) & 3) == 0; }

    [[nodiscard]] std::int32_t clayBand(std::int32_t, std::int32_t, std::int32_t) const override { return 1000; }
};

} // namespace

TEST_CASE("the cave and canyon carvers cut the same chunk as the frozen kotlin") {
    const NoiseLookup lookup = syntheticNoise;
    const OverworldNoiseRouter router =
        noiseRouterData::overworld(lookup, fn::blendedNoiseUnseeded(0.25, 0.125, 80.0, 160.0, 8.0));
    const FakeEnvironment environment;
    Chunk chunk{3, -5};

    // Solid stone up to y=100 so every carve has something to cut, plus a grass and dirt cap so the
    // top-material repair path runs too.
    const std::int32_t minY = chunk.verticalBounds().minY();
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = minY; y < 98; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            chunk.setBlockStateId(x, 98, z, registry::blocks::named("dirt")->stateId);
            chunk.setBlockStateId(x, 99, z, registry::blocks::named("grass_block")->stateId);
        }
    }

    OverworldAquifer aquifer{router, 987LL, 3, -5, minY, chunk.verticalBounds().height()};
    carver::carve(chunk, 987LL, aquifer, environment, surface::surfaceRuleData::overworld());

    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t carved = 0;
    for (std::int32_t y = minY; y < minY + chunk.verticalBounds().height(); ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t stateId = chunk.blockStateId(x, y, z);
                checksum =
                    (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(stateId))) * 1099511628211ULL;
                if (registry::blocks::isAir(stateId) && y < 98) {
                    ++carved;
                }
            }
        }
    }

    CHECK(carved == 238);
    CHECK(static_cast<std::int64_t>(checksum) == 864437565001245551LL);
}
