#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/world/DripstoneGrowth.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/GrowthSupport.hpp>
#include <basalt/server/runtime/world/StalactiteDrips.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/runtime/world/WorldClock.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
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

/// parity: `Block.pushEntitiesUp` from mud, 14/16 tall, to a full block of clay.
void pushEntitiesUp(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    constexpr double kMudHeight = 14.0 / 16.0;
    const entity::Aabb gained{
        static_cast<double>(x), y + kMudHeight, static_cast<double>(z), x + 1.0, y + 1.0, z + 1.0};
    for (entity::ServerEntity* other : world.entitiesIn(x >> 4, z >> 4)) {
        if (entity::Aabb::of(other->x, other->y, other->z, other->width(), other->height()).intersects(gained)) {
            other->y = y + 1.0;
        }
    }
}

} // namespace

void dripstone(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
               random::RandomSource& random, std::vector<BlockChange>& changes, const bool evaporates,
               const LevelEvent& levelEvent, const WorldClock* clock) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return;
    }
    const std::string spike{block->path()};
    if (spike == "pointed_dripstone") {
        // parity: the float for the drip is drawn on every turn, before the growth's own.
        const float drip = random.nextFloat();
        if (const std::optional<stalactiteDrips::Transfer> transfer =
                stalactiteDrips::transfer(world, x, y, z, stateId, drip, evaporates);
            transfer.has_value()) {
            if (transfer->clay.has_value()) {
                const auto [cx, cy, cz] = *transfer->clay;
                growthSupport::set(world, cx, cy, cz, registry::blocks::named("clay")->stateId, changes);
                pushEntitiesUp(world, cx, cy, cz);
                if (clock != nullptr) {
                    // parity: `PointedDripstoneBlock.maybeTransferFluid` ends the clay with `BLOCK_CHANGE`.
                    vibrations::emit(world, "block_change", cx + 0.5, cy + 0.5, cz + 0.5, clock->totalTicks());
                }
            } else if (transfer->cauldron.has_value()) {
                const auto [cx, cy, cz] = *transfer->cauldron;
                const wire::common::Identifier* cauldron =
                    registry::blockStateDefinitions::blockOf(world.blockState(cx, cy, cz).value_or(0));
                if (cauldron != nullptr) {
                    world.scheduleBlockTick(cx, cy, cz, *cauldron, transfer->delay);
                }
            }
            if (levelEvent) {
                levelEvent(transfer->tip[0], transfer->tip[1], transfer->tip[2], stalactiteDrips::kDripEvent);
            }
        }
    }
    if (random.nextFloat() >= kChance) {
        return;
    }
    const auto grownOn = std::ranges::find(kGrowsOn, spike, &std::pair<std::string_view, std::string_view>::first);
    if (grownOn == kGrowsOn.end()) {
        return;
    }
    if (propertyOf(stateId, "vertical_direction") != "down") {
        return;
    }
    // parity: `isStalactiteStartPos`.
    if (pathAt(world, x, y + 1, z) == spike) {
        return;
    }
    // parity: `canGrow`.
    if (pathAt(world, x, y + 1, z) != grownOn->second) {
        return;
    }
    if (spike == "pointed_dripstone") {
        const std::int32_t twoUp = world.blockState(x, y + 2, z).value_or(0);
        if (fluidFlow::kindOf(twoUp) != FluidKind::Water || !isSourceFluid(twoUp)) {
            return;
        }
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
