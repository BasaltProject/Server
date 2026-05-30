#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>

namespace basalt::server::registry::blockFluids {
namespace {
// parity: blocks whose `getFluidState` is water without a `waterlogged` property.
constexpr std::array<std::string_view, 5> kAlwaysInWater = {"seagrass", "tall_seagrass", "kelp", "kelp_plant",
                                                            "bubble_column"};

constexpr std::int32_t kLevels = 16;
} // namespace

bool hasWaterFluid(std::int32_t stateId) {
    const std::int32_t water = blocks::water().stateId;
    if (stateId >= water && stateId < water + kLevels) {
        return true;
    }
    if (blockSupport::holdsWater(stateId)) {
        return true;
    }
    const wire::common::Identifier* block = blockStateDefinitions::blockOf(stateId);
    return block != nullptr && std::ranges::find(kAlwaysInWater, block->path()) != kAlwaysInWater.end();
}

namespace {

bool computeHasFluid(std::int32_t stateId) {
    const std::int32_t lava = blocks::lava().stateId;
    return (stateId >= lava && stateId < lava + kLevels) || hasWaterFluid(stateId);
}

} // namespace

bool hasFluid(std::int32_t stateId) {
    // Asked of nearly every block a chunk generation writes, and a state's answer never changes.
    static const std::vector<bool> answers = [] {
        std::vector<bool> built(static_cast<std::size_t>(blocks::stateCount()));
        for (std::size_t id = 0; id < built.size(); ++id) {
            built[id] = computeHasFluid(static_cast<std::int32_t>(id));
        }
        return built;
    }();
    return stateId >= 0 && static_cast<std::size_t>(stateId) < answers.size()
               ? answers[static_cast<std::size_t>(stateId)]
               : computeHasFluid(stateId);
}
} // namespace basalt::server::registry::blockFluids
