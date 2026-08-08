#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/config/GameplayRules.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace basalt::server::runtime::world {
namespace {

constexpr std::int32_t kEmpty = -1;
constexpr std::int32_t kMaxAmount = 8;
constexpr std::int32_t kFallingLevel = 8;

/// parity: `FlowingFluid.getSlopeDistance`'s recursion, capped by `getSlopeFindDistance`.
constexpr std::int32_t kWaterSlopeDistance = 4;
constexpr std::int32_t kFastLavaSlopeDistance = 4;
constexpr std::int32_t kLavaSlopeDistance = 2;

/// parity: the 1000 `getSlopeDistance` starts from, meaning "no drop within reach".
constexpr std::int32_t kSlopeMiss = 1000;
constexpr float kLavaReplaceHeight = 0.44444445F;

constexpr std::array<std::array<std::int32_t, 3>, 6> kOffsets{{
    {0, -1, 0},
    {0, 1, 0},
    {-1, 0, 0},
    {1, 0, 0},
    {0, 0, -1},
    {0, 0, 1},
}};
constexpr std::size_t kUp = 1;
constexpr std::size_t kFirstHorizontal = 2;
constexpr std::size_t kLastHorizontal = 5;

constexpr std::array<registry::BlockFace, 6> kFaces{registry::BlockFace::Down,  registry::BlockFace::Up,
                                                    registry::BlockFace::West,  registry::BlockFace::East,
                                                    registry::BlockFace::North, registry::BlockFace::South};

[[nodiscard]] std::int32_t namedState(const std::string& name) {
    const registry::BlockState* block = registry::blocks::named(name);
    return block == nullptr ? registry::blocks::air().stateId : block->stateId;
}

struct SolidStates {
    std::int32_t obsidian = namedState("obsidian");
    std::int32_t cobblestone = namedState("cobblestone");
    std::int32_t basalt = namedState("basalt");
    std::int32_t soulSoil = namedState("soul_soil");
    std::int32_t blueIce = namedState("blue_ice");
    std::int32_t stone = namedState("stone");
    std::int32_t bedrock = namedState("bedrock");
};

[[nodiscard]] const SolidStates& solidStates() {
    static const SolidStates instance;
    return instance;
}

/// parity: `FlowingFluid.canHoldAnyFluid`'s own exclusion list.
[[nodiscard]] const std::unordered_set<std::int32_t>& neverFlooded() {
    static const std::unordered_set<std::int32_t> table = [] {
        std::unordered_set<std::int32_t> built;
        for (const char* name : {"ladder", "sugar_cane", "bubble_column", "nether_portal", "end_portal", "end_gateway",
                                 "structure_void"}) {
            if (const registry::BlockState* block = registry::blocks::named(name); block != nullptr) {
                built.insert(block->stateId);
            }
        }
        return built;
    }();
    return table;
}

[[nodiscard]] std::int32_t levelOf(std::int32_t stateId, FluidKind kind) {
    const std::int32_t base = baseStateId(kind);
    return stateId >= base && stateId <= base + 15 ? stateId - base : 0;
}

[[nodiscard]] bool isLiquidBlock(std::int32_t stateId) { return fluidFlow::liquidBlockKindOf(stateId).has_value(); }

/// parity: `FlowingFluid.getLegacyLevel` read backwards: block level to fluid amount.
[[nodiscard]] std::int32_t amountOf(std::int32_t stateId) {
    const std::optional<FluidKind> kind = fluidFlow::kindOf(stateId);
    if (!kind.has_value()) {
        return 0;
    }
    const std::int32_t level = levelOf(stateId, *kind);
    if (level == 0) {
        return kMaxAmount;
    }
    return level < kFallingLevel ? kMaxAmount - level : (2 * kMaxAmount) - level;
}

[[nodiscard]] bool isFalling(std::int32_t stateId) {
    const std::optional<FluidKind> kind = fluidFlow::kindOf(stateId);
    return kind.has_value() && levelOf(stateId, *kind) >= kFallingLevel;
}

[[nodiscard]] bool isSource(std::int32_t stateId) {
    const std::optional<FluidKind> kind = fluidFlow::kindOf(stateId);
    return kind.has_value() && levelOf(stateId, *kind) == 0;
}

/// parity: `FlowingFluid.getLegacyLevel`.
[[nodiscard]] std::int32_t stateFor(FluidKind kind, std::int32_t amount, bool falling) {
    if (amount >= kMaxAmount && !falling) {
        return baseStateId(kind);
    }
    const std::int32_t level = kMaxAmount - std::min(amount, kMaxAmount) + (falling ? kFallingLevel : 0);
    return baseStateId(kind) + level;
}

/// parity: `BlockState.blocksMotion`.
[[nodiscard]] bool blocksMotion(std::int32_t stateId) {
    return !registry::blocks::isAir(stateId) && !isLiquidBlock(stateId) && registry::blockShapes::blocksMotion(stateId);
}

/// parity: `LiquidBlockContainer`.
[[nodiscard]] bool isLiquidContainer(std::int32_t stateId, const wire::common::Identifier& block) {
    const std::string_view path = block.path();
    return path == "kelp" || path == "kelp_plant" || path == "seagrass" || path == "tall_seagrass" ||
           registry::blockStateDefinitions::propertiesOf(stateId).contains("waterlogged");
}

/// parity: 26.3 `FlowingFluid.canHoldFluid` for a flowing liquid.
[[nodiscard]] bool canHoldAnyFluid(std::int32_t stateId, const realBlockTags::TagMembers* tags) {
    if (tags == nullptr) {
        return !blocksMotion(stateId) && !neverFlooded().contains(stateId);
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && !isLiquidContainer(stateId, *block) &&
           realBlockTags::contains(*tags, "minecraft:washed_away_by_fluids", *block);
}

[[nodiscard]] std::int64_t packed(std::int32_t x, std::int32_t y, std::int32_t z) {
    return ((static_cast<std::int64_t>(x) & 0x3FFFFFF) << 38) | ((static_cast<std::int64_t>(y) & 0xFFF) << 26) |
           (static_cast<std::int64_t>(z) & 0x3FFFFFF);
}

class Flow {
public:
    Flow(const fluidFlow::Context& context, std::vector<FluidChange>& changes) : context_(context), changes_(changes) {}

    void tick(std::int32_t x, std::int32_t y, std::int32_t z) {
        std::int32_t state = at(x, y, z);
        const std::optional<FluidKind> kind = fluidFlow::kindOf(state);
        if (!kind.has_value() || solidify(x, y, z, state, *kind)) {
            return;
        }
        if (!isSource(state)) {
            const std::int32_t settled = newLiquid(x, y, z, *kind);
            if (settled == kEmpty) {
                set(x, y, z, registry::blocks::air().stateId);
                return;
            }
            if (settled != state) {
                const std::int32_t delay = spreadDelay(*kind, state, settled, x, y, z);
                state = settled;
                set(x, y, z, settled);
                context_.world.scheduleFluidTick(x, y, z, fluidId(*kind, isSource(settled)), delay);
            }
        }
        spread(x, y, z, state, *kind);
    }

    /// parity: `LiquidBlock.neighborChanged` on all six sides of what just changed.
    void arrived(std::int32_t x, std::int32_t y, std::int32_t z) { wake(x, y, z); }

    void blockChanged(std::int32_t x, std::int32_t y, std::int32_t z) {
        wake(x, y, z);
        for (const std::array<std::int32_t, 3>& offset : kOffsets) {
            wake(x + offset[0], y + offset[1], z + offset[2]);
        }
    }

private:
    [[nodiscard]] std::int32_t at(std::int32_t x, std::int32_t y, std::int32_t z) {
        const std::int64_t key = packed(x, y, z);
        const auto found = states_.find(key);
        if (found != states_.end()) {
            return found->second;
        }
        const std::int32_t state = context_.world.blockState(x, y, z).value_or(solidStates().bedrock);
        states_.emplace(key, state);
        return state;
    }

    /// ocean into a tick that reschedules itself for ever. parity: `LevelChunk` returns early on an
    void set(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
        const std::int32_t previous = at(x, y, z);
        if (previous == stateId || !context_.world.setBlockState(x, y, z, stateId)) {
            return;
        }
        states_[packed(x, y, z)] = stateId;
        // parity: `FlowingFluid.spreadTo` calls `beforeDestroyingBlock` before the block goes.
        changes_.push_back(FluidChange{.x = x, .y = y, .z = z, .stateId = stateId, .washedAway = previous});
        blockChanged(x, y, z);
    }

    void wake(std::int32_t x, std::int32_t y, std::int32_t z) {
        const std::int32_t state = at(x, y, z);
        const std::optional<FluidKind> kind = fluidFlow::kindOf(state);
        if (!kind.has_value() || solidify(x, y, z, state, *kind)) {
            return;
        }
        context_.world.scheduleFluidTick(x, y, z, fluidId(*kind, isSource(state)), tickDelay(*kind));
    }

    /// parity: `WaterFluid.getTickDelay`.
    [[nodiscard]] std::int32_t tickDelay(FluidKind kind) const {
        if (kind == FluidKind::Water) {
            return fluidFlow::kWaterTickDelay;
        }
        const config::GameplayRules::Blocks& blocks = context_.world.gameplay().blocks;
        return context_.ultrawarm ? blocks.lavaSpeedNether : blocks.lavaSpeedNotNether;
    }

    [[nodiscard]] std::int32_t slopeFindDistance(FluidKind kind) const {
        if (kind == FluidKind::Water) {
            return kWaterSlopeDistance;
        }
        return context_.ultrawarm ? kFastLavaSlopeDistance : kLavaSlopeDistance;
    }

    /// parity: `getDropOff`: water loses one level per block, overworld lava two.
    [[nodiscard]] std::int32_t dropOff(FluidKind kind) const {
        return kind == FluidKind::Water || context_.ultrawarm ? 1 : 2;
    }

    /// parity: `LavaFluid.getSpreadDelay`.
    [[nodiscard]] std::int32_t spreadDelay(FluidKind kind, std::int32_t was, std::int32_t now, std::int32_t x,
                                           std::int32_t y, std::int32_t z) {
        const std::int32_t base = tickDelay(kind);
        if (kind != FluidKind::Lava || was == kEmpty || now == kEmpty || !isLiquidBlock(was) || !isLiquidBlock(now) ||
            isFalling(was) || isFalling(now)) {
            return base;
        }
        // parity: `FluidState.getHeight` answers a full block whenever the square above holds the.
        if (fluidFlow::kindOf(at(x, y + 1, z)) == kind || amountOf(now) <= amountOf(was)) {
            return base;
        }
        return context_.random.nextInt(4) != 0 ? base * 4 : base;
    }

    /// parity: `LiquidBlock.shouldSpreadLiquid`.
    bool solidify(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t state, FluidKind kind) {
        if (kind != FluidKind::Lava) {
            return false;
        }
        const bool overSoulSoil = at(x, y - 1, z) == solidStates().soulSoil;
        for (const std::size_t index : {kUp, std::size_t{2}, std::size_t{3}, std::size_t{4}, std::size_t{5}}) {
            const std::array<std::int32_t, 3>& offset = kOffsets[index];
            const std::int32_t neighbour = at(x + offset[0], y + offset[1], z + offset[2]);
            if (fluidFlow::kindOf(neighbour) == FluidKind::Water) {
                set(x, y, z, isSource(state) ? solidStates().obsidian : solidStates().cobblestone);
                return true;
            }
            if (overSoulSoil && neighbour == solidStates().blueIce) {
                set(x, y, z, solidStates().basalt);
                return true;
            }
        }
        return false;
    }

    /// parity: `FlowingFluid.getNewLiquid`.
    [[nodiscard]] std::int32_t newLiquid(std::int32_t x, std::int32_t y, std::int32_t z, FluidKind kind) {
        std::int32_t highest = 0;
        std::int32_t sources = 0;
        const std::int32_t here = at(x, y, z);
        for (std::size_t index = kFirstHorizontal; index <= kLastHorizontal; ++index) {
            const std::array<std::int32_t, 3>& offset = kOffsets[index];
            const std::int32_t neighbour = at(x + offset[0], y + offset[1], z + offset[2]);
            if (fluidFlow::kindOf(neighbour) != kind || !canPassThroughWall(here, neighbour, kFaces[index])) {
                continue;
            }
            if (isSource(neighbour)) {
                ++sources;
            }
            highest = std::max(highest, amountOf(neighbour));
        }

        if (convertsToSource(kind, sources)) {
            const std::int32_t below = at(x, y - 1, z);
            // parity: `getNewLiquid` asks `isSolid`.
            if (registry::blockShapes::isSolidState(below) || (fluidFlow::kindOf(below) == kind && isSource(below))) {
                return stateFor(kind, kMaxAmount, false);
            }
        }

        const std::int32_t above = at(x, y + 1, z);
        if (fluidFlow::kindOf(above) == kind && canPassThroughWall(here, above, registry::BlockFace::Up)) {
            return stateFor(kind, kMaxAmount, true);
        }

        const std::int32_t amount = highest - dropOff(kind);
        return amount <= 0 ? kEmpty : stateFor(kind, amount, false);
    }

    [[nodiscard]] bool convertsToSource(FluidKind kind, std::int32_t sources) const {
        const config::GameplayRules::Blocks& blocks = context_.world.gameplay().blocks;
        if (kind == FluidKind::Lava && blocks.lavaInfiniteRequiredSources > 0) {
            return sources >= blocks.lavaInfiniteRequiredSources;
        }
        const std::int32_t required = kind == FluidKind::Water ? blocks.waterInfiniteRequiredSources : 2;
        return sources >= required && canConvertToSource(kind);
    }

    /// parity: the `water_source_conversion`/`lava_source_conversion` rules infinite water rides on.
    [[nodiscard]] bool canConvertToSource(FluidKind kind) const {
        return context_.gameRules.get(kind == FluidKind::Water ? gameRules::kWaterSourceConversion
                                                               : gameRules::kLavaSourceConversion);
    }

    /// parity: `FlowingFluid.spread`.
    void spread(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t state, FluidKind kind) {
        const std::int32_t below = at(x, y - 1, z);
        if (canMaybePassThrough(kind, state, below, registry::BlockFace::Down)) {
            const std::int32_t newBelow = newLiquid(x, y - 1, z, kind);
            if (newBelow != kEmpty && canBeReplacedWith(below, kind, true)) {
                spreadTo(x, y, z, x, y - 1, z, kind, true, newBelow);
                if (sourceNeighbourCount(x, y, z, kind) >= 3) {
                    spreadToSides(x, y, z, state, kind);
                }
                return;
            }
        }
        if (isSource(state) || !isHole(x, y, z, kind)) {
            spreadToSides(x, y, z, state, kind);
        }
    }

    void spreadToSides(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t state, FluidKind kind) {
        const std::int32_t amount = isFalling(state) ? kMaxAmount - 1 : amountOf(state) - dropOff(kind);
        if (amount <= 0) {
            return;
        }
        std::int32_t lowest = std::numeric_limits<std::int32_t>::max();
        std::array<std::int32_t, kOffsets.size()> distances{};
        std::array<std::int32_t, kOffsets.size()> targets{};
        distances.fill(std::numeric_limits<std::int32_t>::max());
        targets.fill(kEmpty);
        for (std::size_t index = kFirstHorizontal; index <= kLastHorizontal; ++index) {
            const std::array<std::int32_t, 3>& offset = kOffsets[index];
            const std::int32_t nextX = x + offset[0];
            const std::int32_t nextZ = z + offset[2];
            const std::int32_t neighbour = at(nextX, y, nextZ);
            if (!canMaybePassThrough(kind, state, neighbour, kFaces[index])) {
                continue;
            }
            const std::int32_t target = newLiquid(nextX, y, nextZ, kind);
            if (target == kEmpty) {
                continue;
            }
            const std::int32_t distance =
                isHole(nextX, y, nextZ, kind) ? 0 : slopeDistance(nextX, y, nextZ, 1, opposite(index), kind);
            distances[index] = distance;
            // parity: `getSpread` only records a direction the fluid can really be put in, but it.
            if (canBeReplacedWith(neighbour, kind, false)) {
                targets[index] = target;
            }
            lowest = std::min(lowest, distance);
        }

        for (std::size_t index = kFirstHorizontal; index <= kLastHorizontal; ++index) {
            if (targets[index] == kEmpty || distances[index] > lowest) {
                continue;
            }
            const std::array<std::int32_t, 3>& offset = kOffsets[index];
            spreadTo(x, y, z, x + offset[0], y, z + offset[2], kind, false, targets[index]);
        }
    }

    /// parity: `FlowingFluid.getSlopeDistance`.
    [[nodiscard]] std::int32_t slopeDistance(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t pass,
                                             std::size_t from, FluidKind kind) {
        std::int32_t lowest = kSlopeMiss;
        for (std::size_t index = kFirstHorizontal; index <= kLastHorizontal; ++index) {
            if (index == from) {
                continue;
            }
            const std::array<std::int32_t, 3>& offset = kOffsets[index];
            const std::int32_t nextX = x + offset[0];
            const std::int32_t nextZ = z + offset[2];
            const std::int32_t neighbour = at(nextX, y, nextZ);
            if (!canMaybePassThrough(kind, at(x, y, z), neighbour, kFaces[index])) {
                continue;
            }
            if (isHole(nextX, y, nextZ, kind)) {
                return pass;
            }
            if (pass < slopeFindDistance(kind)) {
                lowest = std::min(lowest, slopeDistance(nextX, y, nextZ, pass + 1, opposite(index), kind));
            }
        }
        return lowest;
    }

    [[nodiscard]] static std::size_t opposite(std::size_t index) { return index % 2 == 0 ? index + 1 : index - 1; }

    /// parity: `FlowingFluid.isWaterHole`.
    [[nodiscard]] bool isHole(std::int32_t x, std::int32_t y, std::int32_t z, FluidKind kind) {
        const std::int32_t below = at(x, y - 1, z);
        if (!canPassThroughWall(at(x, y, z), below, registry::BlockFace::Down)) {
            return false;
        }
        return fluidFlow::kindOf(below) == kind || canHoldAnyFluid(below, context_.blockTags);
    }

    [[nodiscard]] std::int32_t sourceNeighbourCount(std::int32_t x, std::int32_t y, std::int32_t z, FluidKind kind) {
        std::int32_t count = 0;
        for (std::size_t index = kFirstHorizontal; index <= kLastHorizontal; ++index) {
            const std::array<std::int32_t, 3>& offset = kOffsets[index];
            const std::int32_t neighbour = at(x + offset[0], y + offset[1], z + offset[2]);
            if (fluidFlow::kindOf(neighbour) == kind && isSource(neighbour)) {
                ++count;
            }
        }
        return count;
    }

    /// parity: `canMaybePassThrough`: a source of the same fluid is never overwritten.
    [[nodiscard]] bool canMaybePassThrough(FluidKind kind, std::int32_t source, std::int32_t target,
                                           registry::BlockFace face) const {
        if (fluidFlow::kindOf(target) == kind && isSource(target)) {
            return false;
        }
        return canHoldAnyFluid(target, context_.blockTags) && canPassThroughWall(source, target, face);
    }

    /// parity: `FlowingFluid.canPassThroughWall`.
    [[nodiscard]] static bool canPassThroughWall(std::int32_t source, std::int32_t target, registry::BlockFace face) {
        return !registry::blockShapes::mergedFaceOccludes(source, target, face);
    }

    /// parity: `WaterFluid`/`LavaFluid.canBeReplacedWith`.
    [[nodiscard]] static bool canBeReplacedWith(std::int32_t target, FluidKind incoming, bool downward) {
        const std::optional<FluidKind> kind = fluidFlow::kindOf(target);
        if (!kind.has_value()) {
            return true;
        }
        if (*kind == FluidKind::Water) {
            return downward && incoming != FluidKind::Water;
        }
        return static_cast<float>(amountOf(target)) / 9.0F >= kLavaReplaceHeight && incoming == FluidKind::Water;
    }

    /// parity: `FlowingFluid.spreadTo`.
    void spreadTo(std::int32_t fromX, std::int32_t fromY, std::int32_t fromZ, std::int32_t x, std::int32_t y,
                  std::int32_t z, FluidKind kind, bool downward, std::int32_t target) {
        if (context_.flowAllowed && !context_.flowAllowed(fromX, fromY, fromZ, x, y, z)) {
            return;
        }
        const std::int32_t existing = at(x, y, z);
        if (downward && kind == FluidKind::Lava && fluidFlow::kindOf(existing) == FluidKind::Water) {
            set(x, y, z, solidStates().stone);
            return;
        }
        set(x, y, z, target);
    }

    const fluidFlow::Context& context_;
    std::vector<FluidChange>& changes_;

    /// position twenty times. parity: `FlowingFluid.SpreadContext` caches for the same reason; here
    std::unordered_map<std::int64_t, std::int32_t> states_;
};

} // namespace

bool isSourceFluid(const std::int32_t stateId) { return isSource(stateId); }

const wire::common::Identifier& fluidId(const FluidKind kind, const bool source) {
    static const wire::common::Identifier water = wire::common::Identifier::vanilla("water");
    static const wire::common::Identifier lava = wire::common::Identifier::vanilla("lava");
    static const wire::common::Identifier flowingWater = wire::common::Identifier::vanilla("flowing_water");
    static const wire::common::Identifier flowingLava = wire::common::Identifier::vanilla("flowing_lava");
    // parity: `LiquidBlock` books with `state.getFluidState().getType()`.
    if (kind == FluidKind::Water) {
        return source ? water : flowingWater;
    }
    return source ? lava : flowingLava;
}

std::int32_t baseStateId(FluidKind kind) {
    static const std::int32_t water = namedState("water");
    static const std::int32_t lava = namedState("lava");
    return kind == FluidKind::Water ? water : lava;
}

namespace fluidFlow {

std::optional<FluidKind> kindOf(std::int32_t stateId) {
    const std::int32_t lava = baseStateId(FluidKind::Lava);
    if (stateId >= lava && stateId <= lava + 15) {
        return FluidKind::Lava;
    }
    return registry::blockFluids::hasWaterFluid(stateId) ? std::optional{FluidKind::Water} : std::nullopt;
}

std::optional<FluidKind> liquidBlockKindOf(std::int32_t stateId) {
    for (const FluidKind kind : {FluidKind::Water, FluidKind::Lava}) {
        const std::int32_t base = baseStateId(kind);
        if (stateId >= base && stateId <= base + 15) {
            return kind;
        }
    }
    return std::nullopt;
}

std::vector<FluidChange> tick(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z) {
    std::vector<FluidChange> changes;
    Flow{context, changes}.tick(x, y, z);
    return changes;
}

void onArrived(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z) {
    std::vector<FluidChange> changes;
    Flow{context, changes}.arrived(x, y, z);
}

std::vector<FluidChange> onBlockChanged(const Context& context, std::int32_t x, std::int32_t y, std::int32_t z) {
    std::vector<FluidChange> changes;
    Flow{context, changes}.blockChanged(x, y, z);
    return changes;
}

float heightOf(std::int32_t stateId, std::int32_t aboveStateId) {
    const std::optional<FluidKind> kind = kindOf(stateId);
    if (!kind.has_value()) {
        return 0.0F;
    }
    if (kindOf(aboveStateId) == kind) {
        return 1.0F;
    }
    return static_cast<float>(amountOf(stateId)) / 9.0F;
}

float ownHeightOf(std::int32_t stateId) {
    return kindOf(stateId).has_value() ? static_cast<float>(amountOf(stateId)) / 9.0F : 0.0F;
}

bool fallingFluid(std::int32_t stateId) { return isFalling(stateId); }

float heightAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    return heightOf(world.blockState(x, y, z).value_or(0), world.blockState(x, y + 1, z).value_or(0));
}

} // namespace fluidFlow

} // namespace basalt::server::runtime::world
