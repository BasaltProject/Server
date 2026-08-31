#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/CactusFeature.hpp>

#include <doctest.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t stateIdOf(std::string_view path) { return registry::blocks::named(path)->stateId; }

class FakeEnvironment final : public terrain::surface::SurfaceRuleEnvironment {
public:
    explicit FakeEnvironment(std::string biome) : biome_(std::move(biome)) {}

    [[nodiscard]] std::int32_t surfaceDepthNoise(std::int32_t, std::int32_t) const override { return 0; }

    [[nodiscard]] double surfaceSecondaryNoise(std::int32_t, std::int32_t) const override { return 0.0; }

    [[nodiscard]] double sample2d(std::string_view, std::int32_t, std::int32_t) const override { return 0.0; }

    [[nodiscard]] double sample3d(std::string_view, std::int32_t, std::int32_t, std::int32_t) const override {
        return 0.0;
    }

    [[nodiscard]] float verticalGradientRandom(std::string_view, std::int32_t, std::int32_t,
                                               std::int32_t) const override {
        return 0.5F;
    }

    [[nodiscard]] basalt::server::random::RandomSource positionalRandom(std::int32_t x, std::int32_t,
                                                                        std::int32_t z) const override {
        return basalt::server::random::RandomSource::xoroshiro(x * 31LL + z);
    }

    [[nodiscard]] std::int32_t estimatedSurfaceHeight(std::int32_t, std::int32_t) const override { return 70; }

    [[nodiscard]] const std::string& biomeAt(std::int32_t, std::int32_t, std::int32_t) const override { return biome_; }

    [[nodiscard]] const std::string& noiseBiomeAt(std::int32_t, std::int32_t, std::int32_t,
                                                  densityfunction::DensityCache*) const override {
        return biome_;
    }

    [[nodiscard]] bool isSteep(std::int32_t, std::int32_t) const override { return false; }

    [[nodiscard]] std::int32_t clayBand(std::int32_t, std::int32_t, std::int32_t) const override { return 0; }

private:
    std::string biome_;
};

/// Sand flats: cacti need sand below, air around and air above.
[[nodiscard]] Chunk sandChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = -64; y < 70; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            chunk.setBlockStateId(x, 70, z, stateIdOf("sand"));
        }
    }
    return chunk;
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t cacti = 0;
    for (std::int32_t y = 69; y <= 80; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                if (id == stateIdOf("cactus") || id == stateIdOf("cactus_flower")) {
                    ++cacti;
                }
            }
        }
    }
    return {static_cast<std::int64_t>(checksum), cacti};
}

[[nodiscard]] std::tuple<std::int32_t, std::size_t, std::int64_t>
run(std::string_view biome, const std::function<void(Chunk&, const FakeEnvironment&)>& body) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t cacti = 0;
    std::size_t deferred = 0;
    for (std::int32_t index = 0; index < 24; ++index) {
        Chunk chunk = sandChunk(index * 5 - 30, index * -3 + 12);
        const FakeEnvironment environment{std::string{biome}};
        body(chunk, environment);
        const auto [chunkChecksum, chunkCacti] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        cacti += chunkCacti;
        deferred += chunk.takeGeneratedBlockWrites().size();
    }
    return {cacti, deferred, static_cast<std::int64_t>(checksum)};
}

} // namespace

TEST_CASE("the cactus patches place what the frozen kotlin places") {
    SUBCASE("desert") {
        const auto [cacti, deferred, checksum] =
            run("minecraft:desert", [](Chunk& chunk, const FakeEnvironment& environment) {
                cactusPatchDecoration::decorate(chunk, 5150LL, environment);
            });
        CHECK(cacti == 8);
        CHECK(deferred == 0);
        CHECK(checksum == 6481009014018184843LL);
    }

    SUBCASE("badlands") {
        const auto [cacti, deferred, checksum] =
            run("minecraft:badlands", [](Chunk& chunk, const FakeEnvironment& environment) {
                cactusPatchDecoration::decorate(chunk, 5150LL, environment);
            });
        CHECK(cacti == 15);
        CHECK(deferred == 0);
        CHECK(checksum == 6321404518005265302LL);
    }

    SUBCASE("wrongBiome") {
        const auto [cacti, deferred, checksum] =
            run("minecraft:plains", [](Chunk& chunk, const FakeEnvironment& environment) {
                cactusPatchDecoration::decorate(chunk, 5150LL, environment);
            });
        CHECK(cacti == 0);
        CHECK(deferred == 0);
        CHECK(checksum == 652661689345430365LL);
    }

    SUBCASE("desertOnly") {
        const auto [cacti, deferred, checksum] =
            run("minecraft:desert", [](Chunk& chunk, const FakeEnvironment& environment) {
                cactusPatchDecoration::decorateFeature(chunk, 5150LL, environment, "patch_cactus_desert");
            });
        CHECK(cacti == 8);
        CHECK(deferred == 0);
        CHECK(checksum == 6481009014018184843LL);
    }

    SUBCASE("regional") {
        const auto [cacti, deferred, checksum] =
            run("minecraft:desert", [](Chunk& chunk, const FakeEnvironment& environment) {
                cactusPatchDecoration::decorate(
                    chunk, 5150LL, environment,
                    [](std::int32_t, std::int32_t y, std::int32_t) -> std::optional<std::int32_t> {
                        if (y < 70) {
                            return registry::blocks::stone().stateId;
                        }
                        return y == 70 ? stateIdOf("sand") : registry::blocks::air().stateId;
                    });
            });
        CHECK(cacti == 9);
        CHECK(deferred == 21);
        CHECK(checksum == -8253037260212862318LL);
    }
}

TEST_CASE("a cactus column matches the frozen kotlin height by height") {
    Chunk chunk = sandChunk(0, 0);
    basalt::server::random::RandomSource random =
        basalt::server::random::RandomSource::worldgen(basalt::server::random::RandomSource::legacy(31415LL));
    std::string heights;
    for (std::int32_t i = 0; i < 40; ++i) {
        const std::int32_t x = i % 16;
        const std::int32_t z = (i * 7) % 16;
        const std::int32_t before = fold(chunk).second;
        cactusFeature::place(chunk, random, DecorationPos{x, 71, z});
        heights.append(std::to_string(fold(chunk).second - before)).push_back(',');
    }
    CHECK(heights == "1,4,1,2,2,1,3,2,2,3,2,1,1,1,3,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,");
    CHECK(fold(chunk).first == -7898187539568528673LL);
    CHECK(random.nextInt() == -109705904);
}
