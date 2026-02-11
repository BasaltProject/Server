#include <basalt/server/random/PositionalRandomFactory.hpp>
#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockConnections.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Geysers.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <cstdint>
#include <map>

namespace basalt::server::runtime::world::geysers {
namespace {

constexpr std::string_view kStateProperty = "potent_sulfur_state";

/// parity: `PotentSulfurBlock.ALLOWED_WATER_BLOCKS_ABOVE`.
constexpr std::int32_t kAllowedWaterBlocksAbove = 4;

/// parity: `PotentSulfurBlockEntity.GEYSER_SALT`.
constexpr std::int64_t kGeyserSalt = -904011478LL;

/// The countdown ticker only looks every twenty ticks, so the wait is counted in those.
constexpr std::int32_t kCountdownStep = 20;

[[nodiscard]] std::string propertyOf(std::int32_t stateId, std::string_view name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(std::string{name});
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* id = registry::blockStateDefinitions::blockOf(stateId);
    return id == nullptr ? std::string{} : id->path();
}

[[nodiscard]] bool isWater(std::int32_t stateId) { return pathOf(stateId) == "water"; }

/// parity: `isGeyserPassableBlock`: air, water, or anything the plume can rise through.
[[nodiscard]] bool isPassable(std::int32_t stateId) {
    return stateId == registry::blocks::air().stateId || isWater(stateId) ||
           registry::blockShapes::isEmptyShape(stateId);
}

[[nodiscard]] bool isWaterSource(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return false;
    }
    if (isWater(*stateId)) {
        return propertyOf(*stateId, "level") == "0";
    }
    return propertyOf(*stateId, "waterlogged") == "true";
}

/// Flowing lava or water, which vanilla will not let drive a geyser.
[[nodiscard]] bool isFlowing(std::int32_t stateId) {
    const std::string level = propertyOf(stateId, "level");
    return !level.empty() && level != "0";
}

/// parity: `findNoxiousGasSourceBlock`: up through the water over the block, at most four of it, to
/// the open block on top. Nothing when something caps it, which is a geyser that does nothing.
[[nodiscard]] std::optional<std::int32_t> gasSourceY(WorldState& world, std::int32_t x, std::int32_t y,
                                                     std::int32_t z) {
    const std::int32_t maxY = y + kAllowedWaterBlocksAbove + 1;
    for (std::int32_t above = y + 1; above <= maxY; ++above) {
        const std::optional<std::int32_t> stateId = world.blockState(x, above, z);
        if (!stateId.has_value()) {
            return std::nullopt;
        }
        const bool water = isWaterSource(world, x, above, z);
        if (!water || (!isWater(*stateId) && !isPassable(*stateId))) {
            if (*stateId == registry::blocks::air().stateId || isPassable(*stateId)) {
                return above;
            }
            return std::nullopt;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::string wantedState(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                      const std::string& current) {
    if (!isWaterSource(world, x, y + 1, z)) {
        return "dry";
    }
    const std::optional<std::int32_t> below = world.blockState(x, y - 1, z);
    if (!below.has_value()) {
        return "dry";
    }
    // parity: `isSourceIfFluid`, which lets flowing lava out of it.
    const bool stillOrSolid = !isFlowing(*below);
    if (registry::blockConnections::eruptsContinuously(*below) && stillOrSolid) {
        return "continuous";
    }
    if (registry::blockConnections::eruptsPeriodically(*below) && stillOrSolid) {
        return current == "erupting" ? "erupting" : "dormant";
    }
    return "wet";
}

} // namespace

const wire::common::Identifier& block() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("potent_sulfur");
    return instance;
}

void scheduleTurn(WorldState& world, std::int64_t seed, std::int64_t gameTime, std::int32_t x, std::int32_t y,
                  std::int32_t z, std::string_view state) {
    const std::optional<std::int32_t> sourceY = gasSourceY(world, x, y, z);
    if (!sourceY.has_value()) {
        return;
    }
    const std::int32_t waterBlocks = *sourceY - y - 1;
    random::RandomSource root = random::RandomSource::xoroshiro(seed ^ kGeyserSalt);
    random::RandomSource source = root.forkPositional().at(x, y, z);
    std::int32_t steps = 0;
    if (state == "dormant") {
        steps = (10 * (waterBlocks - 1)) + source.nextIntBetweenInclusive(15, 30);
    } else {
        // parity: the erupting branch draws one it never reads, which the dormant one does not.
        static_cast<void>(source.nextInt());
        steps = waterBlocks - 1 + source.nextIntBetweenInclusive(1, 2);
    }
    if (steps <= 0) {
        return;
    }
    // The ticker only acts on a twenty-tick boundary, so the turn lands on one too.
    const auto toBoundary = static_cast<std::int32_t>(kCountdownStep - (gameTime % kCountdownStep));
    world.scheduleBlockTick(x, y, z, block(), toBoundary + ((steps - 1) * kCountdownStep));
}

std::optional<std::int32_t> validState(WorldState& world, std::int64_t seed, std::int64_t gameTime, std::int32_t x,
                                       std::int32_t y, std::int32_t z, std::int32_t stateId) {
    const std::string current = propertyOf(stateId, kStateProperty);
    if (current.empty()) {
        return std::nullopt;
    }
    const std::string wanted = wantedState(world, x, y, z, current);
    if (wanted == current) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> turned =
        registry::blockStateDefinitions::stateIdOf(block(), {{std::string{kStateProperty}, wanted}}, stateId);
    if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
        return std::nullopt;
    }
    // parity: `Level.setBlock` runs `onPlace` on every write, the state moving inside the same block
    // included, and `PotentSulfurBlock.onPlace` raises `BLOCK_ACTIVATE` for a geyser that is going.
    // Measured in `geiser`, where the jar's sensor hears the water land on it.
    if ((wanted == "erupting" || wanted == "continuous") &&
        world.hasSculkSensorNear(x, y, z, vibrations::kCalibratedRange)) {
        vibrations::emit(world, "block_activate", x + 0.5, y + 0.5, z + 0.5, gameTime);
    }
    // Entering a wait starts one; leaving it means the clock is somebody else's problem.
    if (wanted == "dormant" || wanted == "erupting") {
        scheduleTurn(world, seed, gameTime, x, y, z, wanted);
    }
    return *turned;
}

void tick(WorldState& world, std::int64_t seed, std::int64_t gameTime, std::int32_t x, std::int32_t y, std::int32_t z,
          std::vector<BlockChange>& changes) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || pathOf(*stateId) != "potent_sulfur") {
        return;
    }
    const std::string state = propertyOf(*stateId, kStateProperty);
    if (state != "dormant" && state != "erupting") {
        return;
    }
    if (!gasSourceY(world, x, y, z).has_value()) {
        return;
    }
    const std::string turnedTo = state == "dormant" ? "erupting" : "dormant";
    const std::optional<std::int32_t> turned =
        registry::blockStateDefinitions::stateIdOf(block(), {{std::string{kStateProperty}, turnedTo}}, *stateId);
    if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
        return;
    }
    changes.push_back(BlockChange{.x = x, .y = y, .z = z, .stateId = *turned});
    // parity: the ticker raises `BLOCK_DEACTIVATE` itself when the geyser goes back to sleep, and
    // the write that starts it raises `BLOCK_ACTIVATE` through `onPlace`, which `Level.setBlock`
    // runs on every write and not only when the block changes.
    if (world.hasSculkSensorNear(x, y, z, vibrations::kCalibratedRange)) {
        vibrations::emit(world, turnedTo == "dormant" ? "block_deactivate" : "block_activate", x + 0.5, y + 0.5,
                         z + 0.5, gameTime);
    }
    scheduleTurn(world, seed, gameTime, x, y, z, turnedTo);
}

} // namespace basalt::server::runtime::world::geysers
