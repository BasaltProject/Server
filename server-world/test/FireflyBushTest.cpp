#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/FireflyBushPatchDecoration.hpp>

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

/// A grass bank beside a water channel. Two of the three specs only place next to water, so a chunk
/// without a shoreline would exercise nothing but their rejection.
[[nodiscard]] Chunk shoreChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 66; y < 70; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            chunk.setBlockStateId(x, 70, z,
                                  z >= 7 && z <= 8 ? registry::blocks::water().stateId : stateIdOf("grass_block"));
        }
    }
    return chunk;
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t bushes = 0;
    for (std::int32_t y = 69; y <= 76; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                if (id == stateIdOf("firefly_bush")) {
                    ++bushes;
                }
            }
        }
    }
    return {static_cast<std::int64_t>(checksum), bushes};
}

[[nodiscard]] std::tuple<std::int32_t, std::size_t, std::int64_t>
run(std::string_view biome, const std::function<void(Chunk&, const FakeEnvironment&)>& body) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t bushes = 0;
    std::size_t deferred = 0;
    for (std::int32_t index = 0; index < 120; ++index) {
        Chunk chunk = shoreChunk(index * 3 - 60, index * -5 + 20);
        const FakeEnvironment environment{std::string{biome}};
        body(chunk, environment);
        const auto [chunkChecksum, chunkBushes] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        bushes += chunkBushes;
        deferred += chunk.takeGeneratedBlockWrites().size();
    }
    return {bushes, deferred, static_cast<std::int64_t>(checksum)};
}

} // namespace

TEST_CASE("the firefly bush patches place what the frozen kotlin places") {
    SUBCASE("swamp") {
        const auto [bushes, deferred, checksum] =
            run("minecraft:swamp", [](Chunk& chunk, const FakeEnvironment& environment) {
                fireflyBushPatchDecoration::decorate(chunk, 737373LL, environment);
            });
        CHECK(bushes == 154);
        CHECK(deferred == 0);
        CHECK(checksum == 7625999647431062251LL);
    }

    SUBCASE("plains") {
        const auto [bushes, deferred, checksum] =
            run("minecraft:plains", [](Chunk& chunk, const FakeEnvironment& environment) {
                fireflyBushPatchDecoration::decorate(chunk, 737373LL, environment);
            });
        CHECK(bushes == 77);
        CHECK(deferred == 0);
        CHECK(checksum == -2291930531195999556LL);
    }

    SUBCASE("wrong") {
        // desert is in neither the swamp specs nor the 42-biome default list.
        const auto [bushes, deferred, checksum] =
            run("minecraft:desert", [](Chunk& chunk, const FakeEnvironment& environment) {
                fireflyBushPatchDecoration::decorate(chunk, 737373LL, environment);
            });
        CHECK(bushes == 0);
        CHECK(deferred == 0);
        CHECK(checksum == -5440262655251532547LL);
    }

    SUBCASE("swampRarityOnly") {
        const auto [bushes, deferred, checksum] =
            run("minecraft:swamp", [](Chunk& chunk, const FakeEnvironment& environment) {
                fireflyBushPatchDecoration::decorateFeature(chunk, 737373LL, environment, "patch_firefly_bush_swamp");
            });
        CHECK(bushes == 28);
        CHECK(deferred == 0);
        CHECK(checksum == 5142116751685418497LL);
    }

    SUBCASE("nearWaterOnly") {
        const auto [bushes, deferred, checksum] =
            run("minecraft:swamp", [](Chunk& chunk, const FakeEnvironment& environment) {
                fireflyBushPatchDecoration::decorateFeature(chunk, 737373LL, environment,
                                                            "patch_firefly_bush_near_water_swamp");
            });
        CHECK(bushes == 126);
        CHECK(deferred == 0);
        CHECK(checksum == 2260162718174963811LL);
    }

    SUBCASE("regional") {
        const auto [bushes, deferred,
                    checksum] = run("minecraft:plains", [](Chunk& chunk, const FakeEnvironment& environment) {
            fireflyBushPatchDecoration::decorate(
                chunk, 737373LL, environment,
                [](std::int32_t, std::int32_t y, std::int32_t z) -> std::optional<std::int32_t> {
                    if (y >= 66 && y <= 69) {
                        return registry::blocks::stone().stateId;
                    }
                    if (y == 70) {
                        const std::int32_t local = z & 15;
                        return local >= 7 && local <= 8 ? registry::blocks::water().stateId : stateIdOf("grass_block");
                    }
                    return registry::blocks::air().stateId;
                });
        });
        CHECK(bushes == 77);
        CHECK(deferred == 6);
        CHECK(checksum == -2291930531195999556LL);
    }

    SUBCASE("the two swamp specs account for the whole swamp total") {
        // Independent of the checksums: if either spec drifted, the parts would stop summing.
        CHECK(28 + 126 == 154);
    }
}

TEST_CASE("the near-water specs place nothing without a shoreline") {
    Chunk dry{0, 0};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 66; y < 70; ++y) {
                dry.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            dry.setBlockStateId(x, 70, z, stateIdOf("grass_block"));
        }
    }
    const FakeEnvironment environment{"minecraft:swamp"};
    fireflyBushPatchDecoration::decorateFeature(dry, 737373LL, environment, "patch_firefly_bush_near_water_swamp");
    CHECK(fold(dry).second == 0);
}
