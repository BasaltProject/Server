#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockExperience.hpp>

#include <array>
#include <string_view>

namespace basalt::server::runtime::world::blockExperience {
namespace {

using namespace std::string_view_literals;

struct Range {
    std::string_view block;
    std::int32_t min = 0;
    std::int32_t max = 0;
};

/// parity: the `xpRange` each block is registered with in `Blocks`.
constexpr std::array kRanges{
    Range{"coal_ore"sv, 0, 2},
    Range{"deepslate_coal_ore"sv, 0, 2},
    Range{"nether_gold_ore"sv, 0, 1},
    Range{"lapis_ore"sv, 2, 5},
    Range{"deepslate_lapis_ore"sv, 2, 5},
    Range{"diamond_ore"sv, 3, 7},
    Range{"deepslate_diamond_ore"sv, 3, 7},
    Range{"emerald_ore"sv, 3, 7},
    Range{"deepslate_emerald_ore"sv, 3, 7},
    Range{"nether_quartz_ore"sv, 2, 5},
    Range{"redstone_ore"sv, 1, 5},
    Range{"deepslate_redstone_ore"sv, 1, 5},
    Range{"sculk"sv, 1, 1},
    Range{"sculk_catalyst"sv, 5, 5},
    Range{"sculk_shrieker"sv, 5, 5},
    Range{"sculk_sensor"sv, 5, 5},
    Range{"calibrated_sculk_sensor"sv, 5, 5},
};

constexpr std::int32_t kSpawnerBase = 15;
constexpr std::int32_t kSpawnerSpread = 15;

} // namespace

std::int32_t onBreak(const wire::common::Identifier& block, bool silkTouch, random::RandomSource& random) {
    if (block.path() == "spawner") {
        return kSpawnerBase + random.nextInt(kSpawnerSpread) + random.nextInt(kSpawnerSpread);
    }
    for (const Range& range : kRanges) {
        if (range.block != block.path()) {
            continue;
        }
        // parity: `ConstantInt` draws nothing; `UniformInt` is `Mth.randomBetweenInclusive`.
        const std::int32_t sampled =
            range.min == range.max ? range.min : random.nextInt(range.max - range.min + 1) + range.min;
        return silkTouch ? 0 : sampled;
    }
    return 0;
}

} // namespace basalt::server::runtime::world::blockExperience
