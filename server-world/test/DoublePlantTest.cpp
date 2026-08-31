#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/DoublePlantPatchDecoration.hpp>

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

/// A mix of supporting and non-supporting ground, plus a low ceiling over one stripe: a double plant
/// needs two free blocks, so the ceiling is what exercises the upper-half rejection.
[[nodiscard]] Chunk groundChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 66; y < 70; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            chunk.setBlockStateId(x, 70, z, x % 5 == 0 ? registry::blocks::stone().stateId : stateIdOf("grass_block"));
            if (z % 6 == 0) {
                chunk.setBlockStateId(x, 72, z, registry::blocks::stone().stateId);
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::tuple<std::int64_t, std::int32_t, std::int32_t> fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t lower = 0;
    std::int32_t halves = 0;
    for (std::int32_t y = 69; y <= 78; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                if (registry::blockStateFamilies::isDoublePlantState(id)) {
                    ++halves;
                    if (id == stateIdOf("sunflower") || id == stateIdOf("large_fern")) {
                        ++lower;
                    }
                }
            }
        }
    }
    return {static_cast<std::int64_t>(checksum), lower, halves};
}

[[nodiscard]] std::tuple<std::int32_t, std::int32_t, std::size_t, std::int64_t>
run(std::string_view biome, const std::function<void(Chunk&, const FakeEnvironment&)>& body) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t lower = 0;
    std::int32_t halves = 0;
    std::size_t deferred = 0;
    for (std::int32_t index = 0; index < 150; ++index) {
        Chunk chunk = groundChunk(index * 3 - 70, index * -5 + 25);
        const FakeEnvironment environment{std::string{biome}};
        body(chunk, environment);
        const auto [chunkChecksum, chunkLower, chunkHalves] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        lower += chunkLower;
        halves += chunkHalves;
        deferred += chunk.takeGeneratedBlockWrites().size();
    }
    return {lower, halves, deferred, static_cast<std::int64_t>(checksum)};
}

} // namespace

TEST_CASE("the double plant patches place what the frozen kotlin places") {
    SUBCASE("sunflower") {
        const auto [lower, halves, deferred, checksum] =
            run("minecraft:sunflower_plains", [](Chunk& chunk, const FakeEnvironment& environment) {
                doublePlantPatchDecoration::decorate(chunk, 8080LL, environment);
            });
        CHECK(lower == 515);
        // Both halves always land together, so this pins the upper write as well as the lower.
        CHECK(halves == 1030);
        CHECK(deferred == 0);
        CHECK(checksum == -6352056353297289416LL);
    }

    SUBCASE("largeFernTaiga") {
        const auto [lower, halves, deferred, checksum] =
            run("minecraft:taiga", [](Chunk& chunk, const FakeEnvironment& environment) {
                doublePlantPatchDecoration::decorate(chunk, 8080LL, environment);
            });
        CHECK(lower == 222);
        // Both halves always land together, so this pins the upper write as well as the lower.
        CHECK(halves == 444);
        CHECK(deferred == 0);
        CHECK(checksum == -2267538228092261259LL);
    }

    SUBCASE("largeFernSnowy") {
        const auto [lower, halves, deferred, checksum] =
            run("minecraft:snowy_taiga", [](Chunk& chunk, const FakeEnvironment& environment) {
                doublePlantPatchDecoration::decorate(chunk, 8080LL, environment);
            });
        CHECK(lower == 222);
        // Both halves always land together, so this pins the upper write as well as the lower.
        CHECK(halves == 444);
        CHECK(deferred == 0);
        CHECK(checksum == -2267538228092261259LL);
    }

    SUBCASE("wrong") {
        // desert is in neither spec's list, so this is the real rejection case.
        const auto [lower, halves, deferred, checksum] =
            run("minecraft:desert", [](Chunk& chunk, const FakeEnvironment& environment) {
                doublePlantPatchDecoration::decorate(chunk, 8080LL, environment);
            });
        CHECK(lower == 0);
        // Both halves always land together, so this pins the upper write as well as the lower.
        CHECK(halves == 0);
        CHECK(deferred == 0);
        CHECK(checksum == 8403995520002277259LL);
    }

    SUBCASE("sunflowerOnly") {
        const auto [lower, halves, deferred, checksum] =
            run("minecraft:sunflower_plains", [](Chunk& chunk, const FakeEnvironment& environment) {
                doublePlantPatchDecoration::decorateFeature(chunk, 8080LL, environment, "patch_sunflower");
            });
        CHECK(lower == 515);
        // Both halves always land together, so this pins the upper write as well as the lower.
        CHECK(halves == 1030);
        CHECK(deferred == 0);
        CHECK(checksum == -6352056353297289416LL);
    }

    SUBCASE("regional") {
        const auto [lower, halves, deferred, checksum] =
            run("minecraft:sunflower_plains", [](Chunk& chunk, const FakeEnvironment& environment) {
                doublePlantPatchDecoration::decorate(
                    chunk, 8080LL, environment,
                    [](std::int32_t, std::int32_t y, std::int32_t) -> std::optional<std::int32_t> {
                        if (y >= 66 && y <= 69) {
                            return registry::blocks::stone().stateId;
                        }
                        return y == 70 ? stateIdOf("grass_block") : registry::blocks::air().stateId;
                    });
            });
        CHECK(lower == 515);
        // Both halves always land together, so this pins the upper write as well as the lower.
        CHECK(halves == 1030);
        CHECK(deferred == 544);
        CHECK(checksum == -6352056353297289416LL);
    }
}

TEST_CASE("one double plant patch at an origin matches the frozen kotlin") {
    Chunk chunk = groundChunk(0, 0);
    basalt::server::random::RandomSource random =
        basalt::server::random::RandomSource::worldgen(basalt::server::random::RandomSource::legacy(5566LL));
    CHECK(doublePlantPatchDecoration::placeAt(chunk, random, DecorationPos{8, 71, 8}, "patch_large_fern") == true);
    CHECK(doublePlantPatchDecoration::placeAt(chunk, random, DecorationPos{8, 71, 8}, "patch_nothing") == false);
    const auto [checksum, lower, halves] = fold(chunk);
    CHECK(lower == 12);
    CHECK(halves == 24);
    CHECK(checksum == 3422881316589854433LL);
    CHECK(random.nextInt() == -930694581);
}

TEST_CASE("the placed feature names match the frozen kotlin") {
    std::string rendered;
    for (const std::string& id : doublePlantPatchDecoration::placedIds()) {
        if (!rendered.empty()) {
            rendered.push_back(',');
        }
        rendered.append(id);
    }
    CHECK(rendered == "patch_large_fern,patch_sunflower");
}
