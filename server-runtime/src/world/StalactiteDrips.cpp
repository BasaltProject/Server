#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/StalactiteDrips.hpp>

#include <map>
#include <string>

namespace basalt::server::runtime::world::stalactiteDrips {
namespace {

namespace reg = server::registry;

/// parity: `findBlockVertical`'s search, eleven steps less the one it starts on.
constexpr std::int32_t kMaxSteps = 11;
constexpr float kWaterChance = 0.17578125F;
constexpr float kLavaChance = 0.05859375F;
constexpr std::int32_t kDripDelay = 50;

enum class Fluid : std::uint8_t { None, Water, Lava };

[[nodiscard]] std::string_view pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = reg::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string_view{} : block->path();
}

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& key) {
    const std::map<std::string, std::string> properties = reg::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(key);
    return found == properties.end() ? std::string{} : found->second;
}

/// parity: the `speleothems` block tag.
[[nodiscard]] bool isSpeleothem(std::string_view path) { return path == "pointed_dripstone" || path == "sulfur_spike"; }

[[nodiscard]] bool isStalactite(std::int32_t stateId) {
    return isSpeleothem(pathOf(stateId)) && propertyOf(stateId, "vertical_direction") == "down";
}

/// parity: `SpeleothemBlock.isTip` without the merged tip.
[[nodiscard]] bool isTip(std::int32_t stateId) {
    return isSpeleothem(pathOf(stateId)) && propertyOf(stateId, "thickness") == "tip";
}

[[nodiscard]] bool continues(std::int32_t stateId, std::int32_t from) {
    return pathOf(stateId) == pathOf(from) && isStalactite(stateId);
}

[[nodiscard]] std::int32_t stateAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    return world.blockState(x, y, z).value_or(reg::blocks::air().stateId);
}

/// parity: `getFluidState().getType()` compared with the source fluids.
[[nodiscard]] Fluid fluidAt(std::int32_t stateId, bool evaporates) {
    if (pathOf(stateId) == "mud") {
        return evaporates ? Fluid::None : Fluid::Water;
    }
    const std::optional<FluidKind> kind = fluidFlow::kindOf(stateId);
    if (!kind.has_value() || !isSourceFluid(stateId)) {
        return Fluid::None;
    }
    return *kind == FluidKind::Water ? Fluid::Water : Fluid::Lava;
}

/// parity: `getFluidAboveStalactite` over `findRootBlock`.
struct Above {
    Position at{};
    Fluid fluid = Fluid::None;
    bool mud = false;
};

[[nodiscard]] std::optional<Above> fluidAbove(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                              std::int32_t stateId, bool evaporates) {
    if (!isStalactite(stateId)) {
        return std::nullopt;
    }
    for (std::int32_t step = 1; step < kMaxSteps; ++step) {
        const std::int32_t here = stateAt(world, x, y + step, z);
        if (pathOf(here) != pathOf(stateId)) {
            const std::int32_t above = stateAt(world, x, y + step + 1, z);
            return Above{
                .at = {x, y + step + 1, z}, .fluid = fluidAt(above, evaporates), .mud = pathOf(above) == "mud"};
        }
        if (!continues(here, stateId)) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

/// parity: `findTip`, down the stalactite to its tip.
[[nodiscard]] std::optional<Position> tipOf(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                            std::int32_t stateId) {
    if (isTip(stateId)) {
        return Position{x, y, z};
    }
    for (std::int32_t step = 1; step < kMaxSteps; ++step) {
        const std::int32_t here = stateAt(world, x, y - step, z);
        if (isTip(here)) {
            return Position{x, y - step, z};
        }
        if (!continues(here, stateId)) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

/// parity: `canDripThrough`.
[[nodiscard]] bool dripsThrough(std::int32_t stateId) {
    if (stateId == reg::blocks::air().stateId || pathOf(stateId) == "cave_air" || pathOf(stateId) == "void_air") {
        return true;
    }
    if (reg::blockShapes::isSolidRender(stateId) || reg::blockFluids::hasFluid(stateId)) {
        return false;
    }
    constexpr std::int32_t kLow = 6 * reg::blockShapes::kUnits / 16;
    constexpr std::int32_t kHigh = 10 * reg::blockShapes::kUnits / 16;
    const std::span<const std::int32_t> boxes = reg::blockShapes::collisionBoxes(stateId);
    for (std::size_t at = 0; at + reg::blockShapes::kCorners <= boxes.size(); at += reg::blockShapes::kCorners) {
        const bool overlapsX = boxes[at] < kHigh && boxes[at + 3] > kLow;
        const bool overlapsY = boxes[at + 1] < reg::blockShapes::kUnits && boxes[at + 4] > 0;
        const bool overlapsZ = boxes[at + 2] < kHigh && boxes[at + 5] > kLow;
        if (overlapsX && overlapsY && overlapsZ) {
            return false;
        }
    }
    return true;
}

/// parity: each cauldron's `canReceiveStalactiteDrip`.
[[nodiscard]] bool receives(std::int32_t stateId, Fluid fluid) {
    const std::string_view path = pathOf(stateId);
    return (path == "cauldron" && fluid != Fluid::None) || (path == "water_cauldron" && fluid == Fluid::Water);
}

[[nodiscard]] bool isCauldron(std::int32_t stateId) {
    const std::string_view path = pathOf(stateId);
    return path == "cauldron" || path == "water_cauldron" || path == "lava_cauldron" || path == "powder_snow_cauldron";
}

} // namespace

std::optional<Transfer> transfer(const WorldState& world, const std::int32_t x, const std::int32_t y,
                                 const std::int32_t z, const std::int32_t stateId, const float randomValue,
                                 const bool evaporates) {
    // parity: `isStalactiteStartPos`, the top piece of a stalactite.
    if (randomValue > kWaterChance || pathOf(stateAt(world, x, y + 1, z)) == pathOf(stateId)) {
        return std::nullopt;
    }
    const std::optional<Above> above = fluidAbove(world, x, y, z, stateId, evaporates);
    if (!above.has_value() || above->fluid == Fluid::None) {
        return std::nullopt;
    }
    const float chance = above->fluid == Fluid::Water ? kWaterChance : kLavaChance;
    if (randomValue >= chance) {
        return std::nullopt;
    }
    const std::optional<Position> tip = tipOf(world, x, y, z, stateId);
    if (!tip.has_value()) {
        return std::nullopt;
    }
    if (above->mud && above->fluid == Fluid::Water) {
        return Transfer{.tip = *tip, .clay = above->at};
    }
    // parity: `findFillableCauldronBelowStalactiteTip`.
    for (std::int32_t step = 1; step < kMaxSteps; ++step) {
        const std::int32_t below = stateAt(world, (*tip)[0], (*tip)[1] - step, (*tip)[2]);
        if (receives(below, above->fluid)) {
            return Transfer{
                .tip = *tip, .cauldron = Position{(*tip)[0], (*tip)[1] - step, (*tip)[2]}, .delay = kDripDelay + step};
        }
        if (!dripsThrough(below)) {
            break;
        }
    }
    return std::nullopt;
}

std::optional<Fill> fillCauldron(const WorldState& world, const std::int32_t x, const std::int32_t y,
                                 const std::int32_t z, const bool evaporates) {
    const std::int32_t cauldron = stateAt(world, x, y, z);
    if (!isCauldron(cauldron)) {
        return std::nullopt;
    }
    // parity: `findStalactiteTipAboveCauldron`, a free-hanging tip over a clear drop.
    for (std::int32_t step = 1; step < kMaxSteps; ++step) {
        const std::int32_t above = stateAt(world, x, y + step, z);
        if (isStalactite(above) && isTip(above) && propertyOf(above, "waterlogged") != "true") {
            const std::optional<Above> source = fluidAbove(world, x, y + step, z, above, evaporates);
            const Fluid fluid = source.has_value() ? source->fluid : Fluid::None;
            if (fluid == Fluid::None || !receives(cauldron, fluid)) {
                return std::nullopt;
            }
            if (pathOf(cauldron) == "cauldron") {
                const std::string_view filled = fluid == Fluid::Water ? "water_cauldron" : "lava_cauldron";
                const reg::BlockState* block = reg::blocks::named(filled);
                return block == nullptr ? std::nullopt
                                        : std::optional{Fill{.stateId = block->stateId,
                                                             .levelEvent = fluid == Fluid::Water ? kWaterIntoCauldron
                                                                                                 : kLavaIntoCauldron}};
            }
            const std::int32_t level = std::stoi("0" + propertyOf(cauldron, "level"));
            if (level >= 3) {
                return std::nullopt;
            }
            const std::optional<std::int32_t> raised = reg::blockStateDefinitions::stateIdOf(
                wire::common::Identifier::vanilla("water_cauldron"), {{"level", std::to_string(level + 1)}}, cauldron);
            return raised.has_value() ? std::optional{Fill{.stateId = *raised, .levelEvent = kWaterIntoCauldron}}
                                      : std::nullopt;
        }
        if (!dripsThrough(above)) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

} // namespace basalt::server::runtime::world::stalactiteDrips
