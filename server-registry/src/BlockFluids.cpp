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

/// The blocks whose fluid state is water whatever their properties say, because they carry no
/// `waterlogged` to say it with.
constexpr std::array<std::string_view, 4> kAlwaysInWater = {"seagrass", "tall_seagrass", "kelp", "kelp_plant"};

/// Every level of the block, not just the source: flowing water is water, and reading it as anything
/// else leaves it out of the section's fluid count and out of the flow engine.
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

bool hasFluid(std::int32_t stateId) {
    const std::int32_t lava = blocks::lava().stateId;
    return (stateId >= lava && stateId < lava + kLevels) || hasWaterFluid(stateId);
}

} // namespace basalt::server::registry::blockFluids
