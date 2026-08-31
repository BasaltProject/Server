#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/OverworldOreDecoration.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <tuple>

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

    [[nodiscard]] std::int32_t estimatedSurfaceHeight(std::int32_t, std::int32_t) const override { return 100; }

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

[[nodiscard]] Chunk stockedChunk() {
    Chunk chunk{-4, 6};
    const std::int32_t minY = chunk.verticalBounds().minY();
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = minY; y < 100; ++y) {
                // Deepslate low, stone above, so both target lists in a paired ore are reached.
                chunk.setBlockStateId(x, y, z, y < 0 ? stateIdOf("deepslate") : registry::blocks::stone().stateId);
            }
            chunk.setBlockStateId(x, 100, z, stateIdOf("dirt"));
            chunk.setBlockStateId(x, 101, z, stateIdOf("grass_block"));
        }
    }
    chunk.freezeWorldgenHeights();
    return chunk;
}

[[nodiscard]] std::tuple<std::int64_t, std::size_t, std::string> report(Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::map<std::string, std::int32_t> counts;
    const ChunkHeight bounds = chunk.verticalBounds();
    for (std::int32_t y = bounds.minY(); y <= bounds.maxY(); ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t stateId = chunk.blockStateId(x, y, z);
                checksum =
                    (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(stateId))) * 1099511628211ULL;
                const std::optional<registry::BlockState> block = registry::blocks::byStateId(stateId);
                if (!block.has_value()) {
                    continue;
                }
                const std::string path = block->block.path();
                if (path.ends_with("_ore") || path == "clay" || path == "gravel" || path == "dirt") {
                    ++counts[path];
                }
            }
        }
    }
    std::string rendered;
    for (const auto& [path, count] : counts) {
        if (!rendered.empty()) {
            rendered.push_back(',');
        }
        rendered.append(path).push_back(':');
        rendered.append(std::to_string(count));
    }
    return {static_cast<std::int64_t>(checksum), chunk.takeGeneratedBlockWrites().size(), rendered};
}

} // namespace

TEST_CASE("the underground ores step fills the same chunk as the frozen kotlin") {
    SUBCASE("plains") {
        Chunk chunk = stockedChunk();
        const FakeEnvironment environment{"minecraft:plains"};
        overworldOreDecoration::decorate(chunk, 77777LL, environment);
        const auto [checksum, deferred, counts] = report(chunk);
        CHECK(counts == "coal_ore:274,copper_ore:87,deepslate_copper_ore:25,deepslate_diamond_ore:22,deepslate_gold_"
                        "ore:17,deepslate_iron_ore:20,deepslate_lapis_ore:16,deepslate_redstone_ore:26,dirt:544,gold_"
                        "ore:14,gravel:721,iron_ore:58,lapis_ore:4,redstone_ore:15");
        CHECK(deferred == 0);
        CHECK(checksum == -9129793627105575744LL);
    }

    SUBCASE("dripstone_caves") {
        Chunk chunk = stockedChunk();
        const FakeEnvironment environment{"minecraft:dripstone_caves"};
        overworldOreDecoration::decorate(chunk, 77777LL, environment);
        const auto [checksum, deferred, counts] = report(chunk);
        CHECK(counts == "coal_ore:274,copper_ore:479,deepslate_diamond_ore:22,deepslate_gold_ore:17,deepslate_iron_ore:"
                        "20,deepslate_lapis_ore:16,deepslate_redstone_ore:26,dirt:544,gold_ore:14,gravel:721,iron_ore:"
                        "58,lapis_ore:4,redstone_ore:15");
        CHECK(deferred == 0);
        CHECK(checksum == -7778457022043298158LL);
    }

    SUBCASE("badlands") {
        Chunk chunk = stockedChunk();
        const FakeEnvironment environment{"minecraft:badlands"};
        overworldOreDecoration::decorate(chunk, 77777LL, environment);
        const auto [checksum, deferred, counts] = report(chunk);
        CHECK(counts == "coal_ore:274,copper_ore:87,deepslate_copper_ore:25,deepslate_diamond_ore:22,deepslate_gold_"
                        "ore:17,deepslate_iron_ore:20,deepslate_lapis_ore:16,deepslate_redstone_ore:26,dirt:544,gold_"
                        "ore:107,gravel:721,iron_ore:58,lapis_ore:4,redstone_ore:15");
        CHECK(deferred == 0);
        CHECK(checksum == -7706078015853384128LL);
    }

    SUBCASE("infested") {
        Chunk chunk = stockedChunk();
        const FakeEnvironment environment{"minecraft:grove"};
        overworldOreDecoration::decorateInfested(chunk, 77777LL, environment);
        const auto [checksum, deferred, counts] = report(chunk);
        CHECK(counts == "dirt:256");
        CHECK(deferred == 0);
        CHECK(checksum == 4969405327263453786LL);
    }

    SUBCASE("regional") {
        // With a region behind it every blob keeps the part that falls outside the chunk, which is
        // the deferred pipeline and about a third of a large vein.
        Chunk chunk = stockedChunk();
        const FakeEnvironment environment{"minecraft:plains"};
        overworldOreDecoration::decorate(
            chunk, 77777LL, environment, [](std::int32_t, std::int32_t) -> std::optional<std::int32_t> { return 100; },
            [](std::int32_t, std::int32_t y, std::int32_t) -> std::optional<std::int32_t> {
                if (y < 0) {
                    return stateIdOf("deepslate");
                }
                return y < 100 ? registry::blocks::stone().stateId : registry::blocks::air().stateId;
            });
        const auto [checksum, deferred, counts] = report(chunk);
        CHECK(counts == "coal_ore:286,copper_ore:83,deepslate_copper_ore:25,deepslate_diamond_ore:22,deepslate_gold_"
                        "ore:17,deepslate_iron_ore:20,deepslate_lapis_ore:16,deepslate_redstone_ore:26,dirt:544,gold_"
                        "ore:14,gravel:721,iron_ore:58,lapis_ore:4,redstone_ore:15");
        CHECK(deferred == 2251);
        CHECK(checksum == -1162384294097065528LL);
    }
}
