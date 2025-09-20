#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/BushPatchDecoration.hpp>

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

/// Supporting and non-supporting ground side by side, so both halves of the support test run.
[[nodiscard]] Chunk groundChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 66; y < 70; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            std::int32_t top = 0;
            switch ((x + z) % 3) {
            case 0:
                top = registry::blocks::stone().stateId;
                break;
            case 1:
                top = stateIdOf("podzol");
                break;
            default:
                top = stateIdOf("grass_block");
                break;
            }
            chunk.setBlockStateId(x, 70, z, top);
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
                if (id == stateIdOf("bush")) {
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
        Chunk chunk = groundChunk(index * 3 - 60, index * -5 + 20);
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

TEST_CASE("the bush patch places what the frozen kotlin places") {
    SUBCASE("plains") {
        const auto [bushes, deferred, checksum] =
            run("minecraft:plains", [](Chunk& chunk, const FakeEnvironment& environment) {
                bushPatchDecoration::decorate(chunk, 353535LL, environment);
            });
        CHECK(bushes == 86);
        CHECK(deferred == 0);
        CHECK(checksum == 9097187992672340721LL);
    }

    SUBCASE("forest") {
        const auto [bushes, deferred, checksum] =
            run("minecraft:forest", [](Chunk& chunk, const FakeEnvironment& environment) {
                bushPatchDecoration::decorate(chunk, 353535LL, environment);
            });
        CHECK(bushes == 86);
        CHECK(deferred == 0);
        CHECK(checksum == 9097187992672340721LL);
    }

    SUBCASE("wrong") {
        // desert is not in the nine-biome list, so this is the real rejection case.
        const auto [bushes, deferred, checksum] =
            run("minecraft:desert", [](Chunk& chunk, const FakeEnvironment& environment) {
                bushPatchDecoration::decorate(chunk, 353535LL, environment);
            });
        CHECK(bushes == 0);
        CHECK(deferred == 0);
        CHECK(checksum == 108324999509819949LL);
    }

    SUBCASE("regional") {
        const auto [bushes, deferred, checksum] =
            run("minecraft:plains", [](Chunk& chunk, const FakeEnvironment& environment) {
                bushPatchDecoration::decorate(
                    chunk, 353535LL, environment,
                    [](std::int32_t, std::int32_t y, std::int32_t) -> std::optional<std::int32_t> {
                        if (y >= 66 && y <= 69) {
                            return registry::blocks::stone().stateId;
                        }
                        return y == 70 ? stateIdOf("grass_block") : registry::blocks::air().stateId;
                    });
            });
        CHECK(bushes == 86);
        CHECK(deferred == 41);
        CHECK(checksum == 9097187992672340721LL);
    }
}

TEST_CASE("one bush patch at an origin matches the frozen kotlin") {
    Chunk chunk = groundChunk(0, 0);
    basalt::server::random::RandomSource random =
        basalt::server::random::RandomSource::worldgen(basalt::server::random::RandomSource::legacy(24680LL));
    bushPatchDecoration::placeAt(chunk, random, DecorationPos{8, 71, 8});
    CHECK(fold(chunk).second == 2);
    CHECK(fold(chunk).first == 407732382784741739LL);
    CHECK(random.nextInt() == -1866235707);
}
