#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/OverworldOreConfigs.hpp>
#include <basalt/server/world/terrain/decoration/OverworldOreDecoration.hpp>
#include <basalt/server/world/terrain/decoration/StoneBlockTags.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using basalt::server::world::testsupport::stateIdOf;
namespace registry = basalt::server::registry;

namespace {

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
        const testsupport::FixedBiomeEnvironment environment{"minecraft:plains"};
        overworldOreDecoration::decorate(chunk, 77777LL, environment);
        const auto [checksum, deferred, counts] = report(chunk);
        CHECK(counts == "coal_ore:274,copper_ore:87,deepslate_copper_ore:25,deepslate_diamond_ore:22,deepslate_gold_"
                        "ore:17,deepslate_iron_ore:20,deepslate_lapis_ore:16,deepslate_redstone_ore:26,dirt:544,gold_"
                        "ore:14,gravel:721,iron_ore:58,lapis_ore:4,redstone_ore:15");
        CHECK(deferred == 0);
        CHECK(checksum == -1044425183117465505LL);
    }

    SUBCASE("dripstone_caves") {
        Chunk chunk = stockedChunk();
        const testsupport::FixedBiomeEnvironment environment{"minecraft:dripstone_caves"};
        overworldOreDecoration::decorate(chunk, 77777LL, environment);
        const auto [checksum, deferred, counts] = report(chunk);
        CHECK(counts == "coal_ore:274,copper_ore:479,deepslate_diamond_ore:22,deepslate_gold_ore:17,deepslate_iron_ore:"
                        "20,deepslate_lapis_ore:16,deepslate_redstone_ore:26,dirt:544,gold_ore:14,gravel:721,iron_ore:"
                        "58,lapis_ore:4,redstone_ore:15");
        CHECK(deferred == 0);
        CHECK(checksum == 3340881560419265137LL);
    }

    SUBCASE("badlands") {
        Chunk chunk = stockedChunk();
        const testsupport::FixedBiomeEnvironment environment{"minecraft:badlands"};
        overworldOreDecoration::decorate(chunk, 77777LL, environment);
        const auto [checksum, deferred, counts] = report(chunk);
        CHECK(counts == "coal_ore:274,copper_ore:87,deepslate_copper_ore:25,deepslate_diamond_ore:22,deepslate_gold_"
                        "ore:17,deepslate_iron_ore:20,deepslate_lapis_ore:16,deepslate_redstone_ore:26,dirt:544,gold_"
                        "ore:107,gravel:721,iron_ore:58,lapis_ore:4,redstone_ore:15");
        CHECK(deferred == 0);
        CHECK(checksum == 2782206242456095130LL);
    }

    SUBCASE("infested") {
        Chunk chunk = stockedChunk();
        const testsupport::FixedBiomeEnvironment environment{"minecraft:grove"};
        overworldOreDecoration::decorateInfested(chunk, 77777LL, environment);
        const auto [checksum, deferred, counts] = report(chunk);
        CHECK(counts == "dirt:256");
        CHECK(deferred == 0);
        CHECK(checksum == -336568363818669604LL);
    }

    SUBCASE("regional") {
        // With a region behind it every blob keeps the part that falls outside the chunk, which is
        // the deferred pipeline and about a third of a large vein.
        Chunk chunk = stockedChunk();
        const testsupport::FixedBiomeEnvironment environment{"minecraft:plains"};
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
        CHECK(checksum == 2781508927182871967LL);
    }
}

TEST_CASE("the 26.3 paired ore targets take tuff by height and a plain blob takes it anywhere") {
    namespace tags = basalt::server::world::terrain::decoration::stoneBlockTags;
    using basalt::server::world::terrain::decoration::Target;
    using basalt::server::world::terrain::decoration::targetMatches;
    const Target stone{&tags::stoneOreReplaceables(), 1, std::pair{0, 2031}};
    const Target deepslate{&tags::deepslateOreReplaceables(), 2, std::pair{-2032, 8}};
    const Target blob{&tags::baseStoneOverworld(), 3, std::nullopt};

    CHECK(targetMatches(stone, "tuff", 0));
    CHECK(targetMatches(stone, "tuff", 2031));
    CHECK_FALSE(targetMatches(stone, "tuff", -1));
    CHECK(targetMatches(deepslate, "tuff", 8));
    CHECK(targetMatches(deepslate, "tuff", -64));
    CHECK_FALSE(targetMatches(deepslate, "tuff", 9));
    CHECK(targetMatches(blob, "tuff", 100));
    CHECK(targetMatches(blob, "tuff", -60));

    CHECK(targetMatches(stone, "granite", -60));
    CHECK_FALSE(targetMatches(stone, "deepslate", 0));
    CHECK(targetMatches(deepslate, "deepslate", 100));
}
