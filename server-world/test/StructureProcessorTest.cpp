#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/structure/StructureProcessor.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

using namespace basalt::server::world::structure;
using basalt::server::world::testsupport::stateIdOf;
namespace rng = basalt::server::random;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t floorMod(std::int32_t value, std::int32_t modulus) {
    const std::int32_t rest = value % modulus;
    return rest < 0 ? rest + modulus : rest;
}

} // namespace

TEST_CASE("the structure rule tests match the frozen kotlin") {
    const std::int32_t cobblestone = stateIdOf("cobblestone");
    const std::int32_t stoneBricks = stateIdOf("stone_bricks");
    const std::int32_t snow = stateIdOf("snow");
    // Snow carries a layer count, so `snow + 1` is the same block in a different state: that is what
    // separates matching a block from matching one exact state.
    const std::array<std::int32_t, 5> probes{snow, snow + 1, cobblestone, stoneBricks, registry::blocks::air().stateId};

    SUBCASE("blockMatch") {
        const StructureRuleTest test = StructureRuleTest::blockMatch(snow);
        rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(4242LL));
        std::string results;
        for (const std::int32_t probe : probes) {
            for (int attempt = 0; attempt < 3; ++attempt) {
                results += test.test(probe, random) ? "1" : "0";
            }
        }
        CHECK(results == "111111000000000");
        CHECK(random.nextInt() == 476620258);
    }

    SUBCASE("randomBlockMatch") {
        const StructureRuleTest test = StructureRuleTest::randomBlockMatch(snow, 0.5F);
        rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(4242LL));
        std::string results;
        for (const std::int32_t probe : probes) {
            for (int attempt = 0; attempt < 3; ++attempt) {
                results += test.test(probe, random) ? "1" : "0";
            }
        }
        CHECK(results == "101110000000000");
        CHECK(random.nextInt() == 509012045);
    }

    SUBCASE("blockStateMatch") {
        const StructureRuleTest test = StructureRuleTest::blockStateMatch(snow);
        rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(4242LL));
        std::string results;
        for (const std::int32_t probe : probes) {
            for (int attempt = 0; attempt < 3; ++attempt) {
                results += test.test(probe, random) ? "1" : "0";
            }
        }
        CHECK(results == "111000000000000");
        CHECK(random.nextInt() == 476620258);
    }

    SUBCASE("tagMatch") {
        const StructureRuleTest test = StructureRuleTest::tagMatch({cobblestone, snow});
        rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(4242LL));
        std::string results;
        for (const std::int32_t probe : probes) {
            for (int attempt = 0; attempt < 3; ++attempt) {
                results += test.test(probe, random) ? "1" : "0";
            }
        }
        CHECK(results == "111111111000000");
        CHECK(random.nextInt() == 476620258);
    }
}

TEST_CASE("the processor list processes what the frozen kotlin processes") {
    const std::int32_t cobblestone = stateIdOf("cobblestone");
    const std::int32_t mossy = stateIdOf("mossy_cobblestone");
    const std::int32_t stoneBricks = stateIdOf("stone_bricks");
    const std::int32_t crackedBricks = stateIdOf("cracked_stone_bricks");
    const std::int32_t bedrock = stateIdOf("bedrock");
    const std::int32_t water = registry::blocks::water().stateId;
    const std::int32_t air = registry::blocks::air().stateId;

    const StructureProcessorList list{
        {StructureProcessorRule{.inputTest = StructureRuleTest::randomBlockMatch(cobblestone, 0.5F),
                                .locationTest = std::nullopt,
                                .outputStateId = mossy},
         StructureProcessorRule{.inputTest = StructureRuleTest::blockMatch(stoneBricks),
                                .locationTest = StructureRuleTest::blockMatch(water),
                                .outputStateId = crackedBricks}},
        BlockRot{.integrity = 0.6F, .rottable = std::set<std::int32_t>{cobblestone}},
        std::set<std::int32_t>{bedrock}};

    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t dropped = 0;
    std::int32_t changed = 0;
    const std::array<std::int32_t, 3> inputs{cobblestone, stoneBricks, mossy};
    const std::array<std::int32_t, 3> worlds{air, water, bedrock};
    for (std::int32_t index = 0; index < 512; ++index) {
        const std::int32_t input = inputs[static_cast<std::size_t>(floorMod(index, 3))];
        const std::int32_t world = worlds[static_cast<std::size_t>(floorMod(index, 3))];
        const StructurePos position{floorMod(index, 29) - 14, floorMod(index, 17) + 40, floorMod(index, 23) - 11};
        const std::optional<std::int32_t> result = list.processAt(input, world, position);
        if (!result.has_value()) {
            ++dropped;
        } else if (*result != input) {
            ++changed;
        }
        checksum = (checksum ^ static_cast<std::uint64_t>(result.value_or(-1))) * 1099511628211ULL;
    }
    CHECK(dropped == 227);
    CHECK(changed == 267);
    CHECK(static_cast<std::int64_t>(checksum) == 7644178188119490970LL);
    CHECK_FALSE(list.empty());
    CHECK(StructureProcessorList{}.empty());

    const StructureProcessorList mossify = StructureProcessorList::mossify(0.7F);
    std::uint64_t mossChecksum = 14695981039346656037ULL;
    for (std::int32_t index = 0; index < 256; ++index) {
        const StructurePos position{floorMod(index, 31) - 15, 64, floorMod(index, 19) - 9};
        const std::optional<std::int32_t> result = mossify.processAt(cobblestone, air, position);
        mossChecksum = (mossChecksum ^ static_cast<std::uint64_t>(result.value_or(-1))) * 1099511628211ULL;
    }
    CHECK(static_cast<std::int64_t>(mossChecksum) == -6949142788833374561LL);
    CHECK_FALSE(mossify.empty());
}

TEST_CASE("the capped processor chooses what the frozen kotlin chooses") {
    const std::int32_t cobblestone = stateIdOf("cobblestone");
    const std::int32_t mossy = stateIdOf("mossy_cobblestone");
    const std::int32_t stoneBricks = stateIdOf("stone_bricks");
    const std::int32_t crackedBricks = stateIdOf("cracked_stone_bricks");
    const CappedRules capped{{StructureProcessorRule{.inputTest = StructureRuleTest::blockMatch(cobblestone),
                                                     .locationTest = std::nullopt,
                                                     .outputStateId = mossy},
                              StructureProcessorRule{.inputTest = StructureRuleTest::blockMatch(stoneBricks),
                                                     .locationTest = std::nullopt,
                                                     .outputStateId = crackedBricks},
                              // A rule whose output is its own input: it matches and still converts
                              // nothing, so it must not eat one of the slots.
                              StructureProcessorRule{.inputTest = StructureRuleTest::blockMatch(mossy),
                                                     .locationTest = std::nullopt,
                                                     .outputStateId = mossy}},
                             18};
    const std::array<std::int32_t, 4> cycle{cobblestone, stoneBricks, mossy, registry::blocks::air().stateId};
    std::vector<std::int32_t> candidates;
    candidates.reserve(40);
    for (std::int32_t index = 0; index < 40; ++index) {
        candidates.push_back(cycle[static_cast<std::size_t>(floorMod(index, 4))]);
    }
    const std::map<std::int32_t, StructureProcessorRule> chosen =
        capped.choose(candidates, 7777LL, StructurePos{12, 64, -30});
    std::string rendered;
    for (const auto& [index, rule] : chosen) {
        if (!rendered.empty()) {
            rendered += ",";
        }
        rendered += std::to_string(index) + "=" + std::to_string(rule.outputStateId);
    }
    CHECK(chosen.size() == 18);
    CHECK(rendered ==
          "0=4924,1=9490,4=4924,5=9490,8=4924,12=4924,13=9490,16=4924,17=9490,20=4924,21=9490,24=4924,25=9490,2"
          "8=4924,29=9490,33=9490,36=4924,37=9490");
    CHECK(CappedRules{{}, 0}.choose(candidates, 7777LL, StructurePos{0, 0, 0}).empty());
}
