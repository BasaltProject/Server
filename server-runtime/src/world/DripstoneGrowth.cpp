#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/DripstoneGrowth.hpp>
#include <basalt/server/runtime/world/GrowthSupport.hpp>

#include <algorithm>
#include <array>
#include <string>
#include <utility>

namespace basalt::server::runtime::world::dripstoneGrowth {
namespace {

/// parity: each `SpeleothemBlock`'s `blockToGrowOn` and `getMaxGrowthLength`.
constexpr std::array<std::pair<std::string_view, std::string_view>, 2> kGrowsOn{
    {{"pointed_dripstone", "dripstone_block"}, {"sulfur_spike", "sulfur"}}};

constexpr std::array<std::pair<std::string_view, std::int32_t>, 2> kReachOf{
    {{"pointed_dripstone", 7}, {"sulfur_spike", 2}}};

[[nodiscard]] std::string pathAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return {};
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    return block == nullptr ? std::string{} : block->path();
}

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& key) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(key);
    return found == properties.end() ? std::string{} : found->second;
}

void placeTip(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, const std::string& direction,
              const std::string& spike, std::vector<BlockChange>& changes) {
    const std::optional<std::int32_t> tip = registry::blockStateDefinitions::stateIdOf(
        wire::common::Identifier::vanilla(spike),
        {{"vertical_direction", direction}, {"thickness", "tip"}, {"waterlogged", "false"}}, std::nullopt);
    if (!tip.has_value()) {
        return;
    }
    growthSupport::set(world, x, y, z, *tip, changes);
}

} // namespace

void dripstone(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
               random::RandomSource& random, std::vector<BlockChange>& changes) {
    if (random.nextFloat() >= kChance) {
        return;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return;
    }
    const std::string spike{block->path()};
    const auto grownOn = std::ranges::find(kGrowsOn, spike, &std::pair<std::string_view, std::string_view>::first);
    if (grownOn == kGrowsOn.end()) {
        return;
    }
    if (propertyOf(stateId, "vertical_direction") != "down") {
        return;
    }
    // parity: `isStalactiteStartPos`, the topmost block of the stalactite. The check below refuses
    // the same blocks, since a spike above is not the block a spike hangs from; this says which of
    // the two rules is which.
    if (pathAt(world, x, y + 1, z) == spike) {
        return;
    }
    // parity: `canGrow`, which asks for the block it hangs from.
    if (pathAt(world, x, y + 1, z) != grownOn->second) {
        return;
    }

    const auto reachOf = std::ranges::find(kReachOf, spike, &std::pair<std::string_view, std::int32_t>::first);
    const std::int32_t reach = reachOf == kReachOf.end() ? kDefaultReach : reachOf->second;

    std::int32_t tipY = y;
    bool found = false;
    for (std::int32_t step = 0; step < reach; ++step) {
        const std::optional<std::int32_t> here = world.blockState(x, tipY, z);
        if (!here.has_value()) {
            return;
        }
        if (propertyOf(*here, "thickness") == "tip") {
            found = true;
            break;
        }
        if (pathAt(world, x, tipY - 1, z) != spike) {
            return;
        }
        --tipY;
    }
    if (!found) {
        return;
    }

    const std::optional<std::int32_t> tip = world.blockState(x, tipY, z);
    if (!tip.has_value()) {
        return;
    }
    // parity: `isFreeHangingStalactite`, which will not grow from a waterlogged tip.
    if (propertyOf(*tip, "waterlogged") == "true") {
        return;
    }
    const std::optional<std::int32_t> below = world.blockState(x, tipY - 1, z);
    if (!below.has_value() || *below != registry::blocks::air().stateId) {
        return;
    }

    if (random.nextBoolean()) {
        placeTip(world, x, tipY - 1, z, "down", spike, changes);
        return;
    }

    // parity: `growStalagmiteBelow`, ten blocks down for a floor to grow up from.
    std::int32_t floorY = tipY - 1;
    for (std::int32_t step = 0; step < kDrop; ++step) {
        const std::optional<std::int32_t> here = world.blockState(x, floorY, z);
        // Water stops the drip. Falling through to the check below would stop it too, since water
        // is not something a stalagmite stands on, but a flooded cave is a different reason.
        if (!here.has_value() || registry::blockSupport::holdsWater(*here)) {
            return;
        }
        if (*here != registry::blocks::air().stateId) {
            if (registry::blockShapes::isFaceSturdy(*here, registry::BlockFace::Up)) {
                placeTip(world, x, floorY + 1, z, "up", spike, changes);
            }
            return;
        }
        --floorY;
    }
}

} // namespace basalt::server::runtime::world::dripstoneGrowth
