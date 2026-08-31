#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/SugarCanePatchDecoration.hpp>

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

/// A shoreline: sand banks beside a water channel, which is the only place cane accepts.
[[nodiscard]] Chunk shoreChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 66; y < 70; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            chunk.setBlockStateId(x, 70, z, z >= 6 && z <= 9 ? registry::blocks::water().stateId : stateIdOf("sand"));
        }
    }
    return chunk;
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t cane = 0;
    for (std::int32_t y = 69; y <= 78; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                if (id == stateIdOf("sugar_cane")) {
                    ++cane;
                }
            }
        }
    }
    return {static_cast<std::int64_t>(checksum), cane};
}

[[nodiscard]] std::tuple<std::int32_t, std::size_t, std::int64_t>
run(std::string_view biome, const std::function<void(Chunk&, const FakeEnvironment&)>& body) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t cane = 0;
    std::size_t deferred = 0;
    for (std::int32_t index = 0; index < 120; ++index) {
        Chunk chunk = shoreChunk(index * 3 - 60, index * -5 + 20);
        const FakeEnvironment environment{std::string{biome}};
        body(chunk, environment);
        const auto [chunkChecksum, chunkCane] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        cane += chunkCane;
        deferred += chunk.takeGeneratedBlockWrites().size();
    }
    return {cane, deferred, static_cast<std::int64_t>(checksum)};
}

} // namespace

TEST_CASE("the sugar cane patches place what the frozen kotlin places") {
    SUBCASE("desert") {
        const auto [cane, deferred, checksum] =
            run("minecraft:desert", [](Chunk& chunk, const FakeEnvironment& environment) {
                sugarCanePatchDecoration::decorate(chunk, 13579LL, environment);
            });
        CHECK(cane == 560);
        CHECK(deferred == 0);
        CHECK(checksum == -2241159267966534677LL);
    }

    SUBCASE("badlands") {
        const auto [cane, deferred, checksum] =
            run("minecraft:badlands", [](Chunk& chunk, const FakeEnvironment& environment) {
                sugarCanePatchDecoration::decorate(chunk, 13579LL, environment);
            });
        CHECK(cane == 143);
        CHECK(deferred == 0);
        CHECK(checksum == -6793943546750051700LL);
    }

    SUBCASE("swamp") {
        const auto [cane, deferred, checksum] =
            run("minecraft:swamp", [](Chunk& chunk, const FakeEnvironment& environment) {
                sugarCanePatchDecoration::decorate(chunk, 13579LL, environment);
            });
        CHECK(cane == 197);
        CHECK(deferred == 0);
        CHECK(checksum == 6128057495779430292LL);
    }

    SUBCASE("plains") {
        const auto [cane, deferred, checksum] =
            run("minecraft:plains", [](Chunk& chunk, const FakeEnvironment& environment) {
                sugarCanePatchDecoration::decorate(chunk, 13579LL, environment);
            });
        CHECK(cane == 98);
        CHECK(deferred == 0);
        CHECK(checksum == 1021867050187984159LL);
    }

    SUBCASE("wrong") {
        // deep_dark is in none of the four lists, so this is the real rejection case.
        const auto [cane, deferred, checksum] =
            run("minecraft:deep_dark", [](Chunk& chunk, const FakeEnvironment& environment) {
                sugarCanePatchDecoration::decorate(chunk, 13579LL, environment);
            });
        CHECK(cane == 0);
        CHECK(deferred == 0);
        CHECK(checksum == 7994375482017995133LL);
    }

    SUBCASE("desertOnly") {
        const auto [cane, deferred, checksum] =
            run("minecraft:desert", [](Chunk& chunk, const FakeEnvironment& environment) {
                sugarCanePatchDecoration::decorateFeature(chunk, 13579LL, environment, "patch_sugar_cane_desert");
            });
        CHECK(cane == 560);
        CHECK(deferred == 0);
        CHECK(checksum == -2241159267966534677LL);
    }

    SUBCASE("regional") {
        const auto [cane, deferred, checksum] =
            run("minecraft:desert", [](Chunk& chunk, const FakeEnvironment& environment) {
                sugarCanePatchDecoration::decorate(
                    chunk, 13579LL, environment,
                    [](std::int32_t, std::int32_t y, std::int32_t z) -> std::optional<std::int32_t> {
                        if (y >= 66 && y <= 69) {
                            return registry::blocks::stone().stateId;
                        }
                        if (y == 70) {
                            const std::int32_t local = z & 15;
                            return local >= 6 && local <= 9 ? registry::blocks::water().stateId : stateIdOf("sand");
                        }
                        return registry::blocks::air().stateId;
                    });
            });
        CHECK(cane == 562);
        CHECK(deferred == 57);
        CHECK(checksum == -8940531553218222127LL);
    }
}
