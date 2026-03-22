#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeBlockRules.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

using namespace basalt::server::registry;
using namespace basalt::server::world::terrain::decoration::tree;

namespace {

/// FNV-1a over every block state. These are tag membership questions, so a missing entry shows up in
/// one state out of thirty thousand and nowhere else: a sample would not find it.
template <typename Fn>
[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(Fn&& of) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t hits = 0;
    for (std::int32_t stateId = 0; stateId < blocks::stateCount(); ++stateId) {
        const std::int64_t value = of(stateId);
        checksum = (checksum ^ static_cast<std::uint64_t>(value)) * 1099511628211ULL;
        if (value != 0) {
            ++hits;
        }
    }
    return {static_cast<std::int64_t>(checksum), hits};
}

struct DeferredCase {
    std::string_view generated;
    std::int64_t canReplaceChecksum;
    std::int32_t canReplaceHits;
    std::int64_t resolveChecksum;
    std::int32_t resolveHits;
};

} // namespace

TEST_CASE("the tree block tags match the frozen kotlin across every state") {
    const auto [logChecksum, logHits] =
        fold([](std::int32_t stateId) { return treeBlockRules::isLogState(stateId) ? 1LL : 0LL; });
    CHECK(logHits == 144);
    CHECK(logChecksum == 3358852166341920889LL);

    const auto [validChecksum, validHits] =
        fold([](std::int32_t stateId) { return treeBlockRules::isValidTreeState(stateId) ? 1LL : 0LL; });
    CHECK(validHits == 803);
    CHECK(validChecksum == -6265263885651082876LL);
}

TEST_CASE("the deferred tree rules match the frozen kotlin across every state") {
    // Hit counts differ per block: a persistent leaf is never overwritten and stone is no tree block.
    // The waterlogging `resolve` sums follow the jar: `bubble_column` counts as water for both
    // `RootPlacer.getPotentiallyWaterloggedState` and `FoliagePlacer`.
    const std::vector<DeferredCase> cases = {
        {"oak_log", -6265263885651082876LL, 803, 4856878395355651211LL, 35723},
        {"oak_leaves", -8693261334539119208LL, 607, -7719366257610474507LL, 35723},
        {"mangrove_roots", -8863316575167149949LL, 858, 7831339801412569814LL, 35723},
        {"muddy_mangrove_roots", 4928811664120894323LL, 4, 1996769551517896363LL, 35723},
        {"stone", 6413603058071678615LL, 0, -4119893808682362438LL, 35723},
    };

    for (const DeferredCase& deferred : cases) {
        CAPTURE(deferred.generated);
        const std::int32_t generatedStateId = blocks::named(deferred.generated)->stateId;
        const auto [canChecksum, canHits] = fold([generatedStateId](std::int32_t actual) {
            return treeBlockRules::canReplaceDeferred(actual, generatedStateId) ? 1LL : 0LL;
        });
        CHECK(canHits == deferred.canReplaceHits);
        CHECK(canChecksum == deferred.canReplaceChecksum);

        const auto [resolveChecksum, resolveHits] = fold([generatedStateId](std::int32_t actual) {
            return static_cast<std::int64_t>(treeBlockRules::resolveDeferredState(actual, generatedStateId));
        });
        CHECK(resolveHits == deferred.resolveHits);
        CHECK(resolveChecksum == deferred.resolveChecksum);
    }
}

TEST_CASE("the log tag matches the 26.3 logs tag") {
    std::string rendered;
    for (const std::string& path : treeBlockRules::logBlockPaths()) {
        if (!rendered.empty()) {
            rendered.push_back(',');
        }
        rendered.append(path);
    }
    CHECK(rendered ==
          "acacia_log,acacia_wood,birch_log,birch_wood,cherry_log,cherry_wood,crimson_hyphae,crimson_stem,dark_"
          "oak_log,dark_oak_wood,jungle_log,jungle_wood,mangrove_log,mangrove_wood,oak_log,oak_wood,pale_oak_lo"
          "g,pale_oak_wood,poplar_log,poplar_wood,spruce_log,spruce_wood,stripped_acacia_log,stripped_acacia_wo"
          "od,stripped_birch_log,stripped_birch_wood,stripped_cherry_log,stripped_cherry_wood,stripped_crimson_"
          "hyphae,stripped_crimson_stem,stripped_dark_oak_log,stripped_dark_oak_wood,stripped_jungle_log,stripp"
          "ed_jungle_wood,stripped_mangrove_log,stripped_mangrove_wood,stripped_oak_log,stripped_oak_wood,strip"
          "ped_pale_oak_log,stripped_pale_oak_wood,stripped_poplar_log,stripped_poplar_wood,stripped_spruce_log"
          ",stripped_spruce_wood,stripped_warped_hyphae,stripped_warped_stem,warped_hyphae,warped_stem");
}
