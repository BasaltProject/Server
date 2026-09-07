#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>

#include <doctest.h>

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace basalt::server::registry;
using namespace basalt::server::registry::blockStateFamilies;

namespace {

constexpr std::int64_t kSentinel = std::numeric_limits<std::int64_t>::min();

[[nodiscard]] std::int64_t value(std::optional<std::int32_t> result) {
    return result.has_value() ? static_cast<std::int64_t>(*result) : kSentinel;
}

struct Fold {
    std::string_view name;
    std::int64_t (*of)(std::int32_t);
    std::int64_t checksum;
    std::int32_t hits;
};

} // namespace

TEST_CASE("every state family matches the frozen kotlin across all block states") {
    // Over every state rather than a sample: these are offsets from a family default, so an
    // off-by-one shows up in one state out of thirty thousand and nowhere else.
    const std::vector<Fold> folds = {
        {"isLeaves", [](std::int32_t stateId) { return isLeavesState(stateId) ? std::int64_t{1} : kSentinel; },
         -7962625503865714927LL, 308},
        {"leafDistance", [](std::int32_t stateId) { return value(leafDistance(stateId)); }, 137942025502718621LL, 308},
        {"withLeafDistance5",
         [](std::int32_t stateId) { return isLeavesState(stateId) ? value(withLeafDistance(stateId, 5)) : kSentinel; },
         4144082399832047497LL, 308},
        {"isDoublePlant",
         [](std::int32_t stateId) { return isDoublePlantState(stateId) ? std::int64_t{1} : kSentinel; },
         2514696154355804319LL, 14},
        {"upperDoublePlant", [](std::int32_t stateId) { return value(upperDoublePlantStateId(stateId)); },
         7896168875409523436LL, 7},
        {"snowyTrue", [](std::int32_t stateId) { return value(withSnowy(stateId, true)); }, 1352968563699925045LL, 6},
        {"snowyFalse", [](std::int32_t stateId) { return value(withSnowy(stateId, false)); }, -256653474340264377LL, 6},
        {"isKelp", [](std::int32_t stateId) { return isKelpState(stateId) ? std::int64_t{1} : kSentinel; },
         -3121521888998229137LL, 26},
        {"vineHead7", [](std::int32_t stateId) { return value(growingVineHeadStateId(stateId, 7)); },
         1994805262625992315LL, 52},
        {"isSeaPickle", [](std::int32_t stateId) { return isSeaPickleState(stateId) ? std::int64_t{1} : kSentinel; },
         6467262591771467829LL, 8},
        {"isCoralPlant", [](std::int32_t stateId) { return isCoralPlantState(stateId) ? std::int64_t{1} : kSentinel; },
         5320997510605085057LL, 60},
        {"coralWallFan", [](std::int32_t stateId) { return value(coralWallFanStateId(stateId, "west", true)); },
         -5039611045297653243LL, 40},
        {"isHugeMushroom",
         [](std::int32_t stateId) { return isHugeMushroomState(stateId) ? std::int64_t{1} : kSentinel; },
         -240261360292004067LL, 192},
        {"hugeMushroom",
         [](std::int32_t stateId) {
             return value(hugeMushroomStateId(stateId, true, false, true, false, true, false));
         },
         -6950332968480692195LL, 192},
        {"isWildflowers",
         [](std::int32_t stateId) { return isWildflowersState(stateId) ? std::int64_t{1} : kSentinel; },
         -3016051765538710771LL, 16},
        {"isPinkPetals", [](std::int32_t stateId) { return isPinkPetalsState(stateId) ? std::int64_t{1} : kSentinel; },
         -224122607065118963LL, 16},
        {"isGlowLichen", [](std::int32_t stateId) { return isGlowLichenState(stateId) ? std::int64_t{1} : kSentinel; },
         2406304357268438749LL, 128},
        {"isVine", [](std::int32_t stateId) { return isVineState(stateId) ? std::int64_t{1} : kSentinel; },
         -559004133656793475LL, 32},
        {"isCocoa", [](std::int32_t stateId) { return isCocoaState(stateId) ? std::int64_t{1} : kSentinel; },
         -2705314438052268511LL, 12},
        {"isPaleHangingMoss",
         [](std::int32_t stateId) { return isPaleHangingMossState(stateId) ? std::int64_t{1} : kSentinel; },
         4924645298137141691LL, 2},
        {"isCreakingHeart",
         [](std::int32_t stateId) { return isCreakingHeartState(stateId) ? std::int64_t{1} : kSentinel; },
         -7597430723210036869LL, 18},
        {"isMangrovePropagule",
         [](std::int32_t stateId) { return isMangrovePropaguleState(stateId) ? std::int64_t{1} : kSentinel; },
         -7669028796148813867LL, 40},
        {"isLeafLitter", [](std::int32_t stateId) { return isLeafLitterState(stateId) ? std::int64_t{1} : kSentinel; },
         5270943634463354637LL, 16},
        {"isBeeNest", [](std::int32_t stateId) { return isBeeNestState(stateId) ? std::int64_t{1} : kSentinel; },
         7271691261327459893LL, 24},
    };

    for (const Fold& fold : folds) {
        CAPTURE(fold.name);
        std::uint64_t checksum = 14695981039346656037ULL;
        std::int32_t hits = 0;
        for (std::int32_t stateId = 0; stateId < blocks::stateCount(); ++stateId) {
            const std::int64_t result = fold.of(stateId);
            checksum = (checksum ^ static_cast<std::uint64_t>(result)) * 1099511628211ULL;
            if (result != kSentinel) {
                ++hits;
            }
        }
        CHECK(hits == fold.hits);
        CHECK(static_cast<std::int64_t>(checksum) == fold.checksum);
    }
}

TEST_CASE("the packed connection states match the frozen kotlin") {
    std::string rendered;
    for (std::int32_t mask = 0; mask < 64; ++mask) {
        rendered.append(std::to_string(chorusPlantStateId((mask & 32) != 0, (mask & 16) != 0, (mask & 8) != 0,
                                                          (mask & 4) != 0, (mask & 2) != 0, (mask & 1) != 0)));
        rendered.push_back(',');
    }
    for (std::int32_t age = 0; age <= 5; ++age) {
        rendered.append(std::to_string(chorusFlowerStateId(age))).push_back(',');
    }
    for (std::int32_t mask = 0; mask < 16; ++mask) {
        rendered.append(
            std::to_string(ironBarsStateId((mask & 8) != 0, (mask & 4) != 0, (mask & 2) != 0, (mask & 1) != 0)));
        rendered.push_back(',');
    }
    CHECK(rendered ==
          "14705,14704,14703,14702,14701,14700,14699,14698,14697,14696,14695,14694,14693,14692,14691,14690,14689,14688,"
          "14687,14686,14685,14684,14683,14682,14681,14680,14679,14678,14677,14676,14675,14674,14673,14672,14671,14670,"
          "14669,14668,14667,14666,14665,14664,14663,14662,14661,14660,14659,14658,14657,14656,14655,14654,14653,14652,"
          "14651,14650,14649,14648,14647,14646,14645,14644,14643,14642,14706,14707,14708,14709,14710,14711,7989,7988,"
          "7985,7984,7973,7972,7969,7968,7981,7980,7977,7976,7965,7964,7961,7960,");
}
