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

/// parity: `PotentSulfurBlockEntity.waitingCountdown` starts at minus one, which is what a geyser
/// that has never waited carries on disk.
constexpr std::int32_t kNoCountdown = -1;

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
        // parity: `validBlockState` calls `resetCountdown()` on the block entity when what was here
        // was **not** a geyser yet, so a geyser that has just been built starts its wait from
        // scratch instead of carrying whatever number it had from an earlier life.
        if (current != "erupting" && current != "dormant") {
            if (server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4); chunk != nullptr) {
                if (server::world::ChunkBlockEntity* entity = chunk->blockEntityAt(x & 15, y, z & 15);
                    entity != nullptr) {
                    entity->data().putInt("countdown", kNoCountdown);
                }
            }
        }
        return current == "erupting" ? "erupting" : "dormant";
    }
    return "wet";
}

/// parity: `PotentSulfurBlock.onPlace` for a geyser that is going: a block event with two zeroes
/// and the sound of it starting. The event is what tells a client **when** it started, because
/// `triggerEvent` writes `eruptionTick` and the client's own plume ticker counts from there; without
/// it the plume comes out of step with the sound. Neither reaches the world: the field is not saved
/// and nothing on this side reads it.
[[nodiscard]] std::vector<BlockEffect> startingEffects(const std::string& state) {
    if (state != "erupting" && state != "continuous") {
        return {};
    }
    return {blockEffect::Event{},
            blockEffect::Sound{.name = state == "continuous" ? "block.potent_sulfur.geyser_continuous_eruption"
                                                             : "block.potent_sulfur.geyser_eruption"}};
}

} // namespace

const wire::common::Identifier& block() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("potent_sulfur");
    return instance;
}

std::optional<std::int32_t> validState(WorldState& world, std::int64_t gameTime, std::int32_t x, std::int32_t y,
                                       std::int32_t z, std::int32_t stateId) {
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
    return *turned;
}

std::optional<BlockChange> countdownTick(WorldState& world, std::int64_t seed, std::int64_t gameTime, std::int32_t x,
                                         std::int32_t y, std::int32_t z) {
    // parity: the ticker runs every tick and does nothing on nineteen out of every twenty, so the
    // whole wait is counted in twenties.
    if (gameTime % kCountdownStep != 0) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || pathOf(*stateId) != "potent_sulfur") {
        return std::nullopt;
    }
    const std::string state = propertyOf(*stateId, kStateProperty);
    // parity: `PotentSulfurBlock.getTicker` hands the countdown only to a geyser that is waiting or
    // going off. A dry one has no ticker at all, a wet one only breathes, and a continuous one never
    // stops.
    if (state != "dormant" && state != "erupting") {
        return std::nullopt;
    }
    const std::optional<std::int32_t> sourceY = gasSourceY(world, x, y, z);
    if (!sourceY.has_value()) {
        return std::nullopt;
    }
    server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    server::world::ChunkBlockEntity* entity = chunk == nullptr ? nullptr : chunk->blockEntityAt(x & 15, y, z & 15);
    if (entity == nullptr) {
        return std::nullopt;
    }
    std::int32_t countdown = entity->data().getInt("countdown", kNoCountdown);
    if (countdown <= 0) {
        const std::int32_t waterBlocks = *sourceY - y - 1;
        // parity: `geyserPositional`, the world seed salted and forked at this very square, so two
        // geysers side by side keep their own rhythm and the same world always keeps the same one.
        random::RandomSource root = random::RandomSource::xoroshiro(seed ^ kGeyserSalt);
        random::RandomSource source = root.forkPositional().at(x, y, z);
        if (state == "dormant") {
            countdown = (10 * (waterBlocks - 1)) + source.nextIntBetweenInclusive(15, 30);
        } else {
            // parity: the erupting branch draws one it never reads, which the dormant one does not.
            static_cast<void>(source.nextInt());
            countdown = waterBlocks - 1 + source.nextIntBetweenInclusive(1, 2);
        }
    }
    if (countdown > 0) {
        --countdown;
    }
    // parity: `waitingCountdown--` is written without `setChanged`, so the chunk stays as clean as
    // it was: what reaches the disk is whatever it held the last time something else wrote it.
    entity->data().putInt("countdown", countdown);
    if (countdown != 0) {
        return std::nullopt;
    }
    const std::string turnedTo = state == "dormant" ? "erupting" : "dormant";
    const std::optional<std::int32_t> turned =
        registry::blockStateDefinitions::stateIdOf(block(), {{std::string{kStateProperty}, turnedTo}}, *stateId);
    if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
        return std::nullopt;
    }
    // parity: the ticker raises `BLOCK_DEACTIVATE` itself when the geyser goes back to sleep, and
    // the write that starts it raises `BLOCK_ACTIVATE` through `onPlace`, which `Level.setBlock`
    // runs on every write and not only when the block changes.
    if (world.hasSculkSensorNear(x, y, z, vibrations::kCalibratedRange)) {
        vibrations::emit(world, turnedTo == "dormant" ? "block_deactivate" : "block_activate", x + 0.5, y + 0.5,
                         z + 0.5, gameTime);
    }
    return BlockChange{.x = x, .y = y, .z = z, .stateId = *turned, .effects = startingEffects(turnedTo)};
}

} // namespace basalt::server::runtime::world::geysers
