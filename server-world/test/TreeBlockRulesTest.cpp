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
    CHECK(logHits == 132);
    CHECK(logChecksum == -6594563485870633379LL);

    const auto [validChecksum, validHits] =
        fold([](std::int32_t stateId) { return treeBlockRules::isValidTreeState(stateId) ? 1LL : 0LL; });
    CHECK(validHits == 711);
    CHECK(validChecksum == -862025256425519872LL);
}

TEST_CASE("the deferred tree rules match the frozen kotlin across every state") {
    // Each generated block asks a different question, and the hit counts differ accordingly: leaves
    // accept fewer targets than logs because a persistent leaf is never overwritten, and stone
    // accepts none at all because it is not a tree block.
    //
    // The two `resolve` sums of the blocks that waterlog were re-frozen when `bubble_column` joined
    // `blockFluids::kAlwaysInWater`. The jar is the authority over the retired Kotlin here:
    // `RootPlacer.getPotentiallyWaterloggedState` asks `s.is(FluidTags.WATER)` and
    // `FoliagePlacer` asks `isSourceOfType(Fluids.WATER)`, and `BubbleColumnBlock.getFluidState`
    // answers `Fluids.WATER.getSource(false)`, so a column satisfies both. Only these two moved,
    // which is the check that the entry did what it says: the three rows without `waterlogged` are
    // untouched.
    const std::vector<DeferredCase> cases = {
        {"oak_log", -862025256425519872LL, 711, 6380730288614645155LL, 32366},
        {"oak_leaves", 5205421896913956162LL, 557, 7507921152890863202LL, 32366},
        {"mangrove_roots", -154421432389656447LL, 766, 2765597207296845505LL, 32366},
        {"muddy_mangrove_roots", 7791462882714030475LL, 4, -1524759485161991939LL, 32366},
        {"stone", 3248206000953826909LL, 0, -4021252027964921389LL, 32366},
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

TEST_CASE("the log tag matches the frozen kotlin") {
    std::string rendered;
    for (const std::string& path : treeBlockRules::logBlockPaths()) {
        if (!rendered.empty()) {
            rendered.push_back(',');
        }
        rendered.append(path);
    }
    CHECK(rendered ==
          "acacia_log,acacia_wood,birch_log,birch_wood,cherry_log,cherry_wood,crimson_hyphae,crimson_stem,dark_oak_log,"
          "dark_oak_wood,jungle_log,jungle_wood,mangrove_log,mangrove_wood,oak_log,oak_wood,pale_oak_log,pale_oak_wood,"
          "spruce_log,spruce_wood,stripped_acacia_log,stripped_acacia_wood,stripped_birch_log,stripped_birch_wood,"
          "stripped_cherry_log,stripped_cherry_wood,stripped_crimson_hyphae,stripped_crimson_stem,stripped_dark_oak_"
          "log,stripped_dark_oak_wood,stripped_jungle_log,stripped_jungle_wood,stripped_mangrove_log,stripped_mangrove_"
          "wood,stripped_oak_log,stripped_oak_wood,stripped_pale_oak_log,stripped_pale_oak_wood,stripped_spruce_log,"
          "stripped_spruce_wood,stripped_warped_hyphae,stripped_warped_stem,warped_hyphae,warped_stem");
}
