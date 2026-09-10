#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Dripleaves.hpp>
#include <basalt/server/runtime/world/ProjectileHits.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <utility>

namespace basalt::server::runtime::world::projectileHits {
namespace {

constexpr double kHalf = 0.5;
constexpr double kMaxSignal = 15.0;

[[nodiscard]] const wire::common::Identifier& targetBlock() {
    static const wire::common::Identifier id = wire::common::Identifier::vanilla("target");
    return id;
}

/// parity: `Mth.frac`, which is the part of a coordinate inside its own block.
[[nodiscard]] double fraction(double value) { return value - std::floor(value); }

[[nodiscard]] std::optional<BlockChange> hitTarget(WorldState& world, const entity::BlockHit& hit, std::int32_t stateId,
                                                   const wire::common::Identifier& block, bool byArrow) {
    // parity: a target already counting down keeps the signal it has; only the first hit sets it.
    if (world.hasScheduledBlockTick(hit.blockX, hit.blockY, hit.blockZ, targetBlock())) {
        return std::nullopt;
    }
    const std::int32_t signal = signalFor(hit);
    const std::map<std::string, std::string> properties{{"power", std::to_string(signal)}};
    const std::optional<std::int32_t> turned = registry::blockStateDefinitions::stateIdOf(block, properties, stateId);
    if (!turned.has_value()) {
        return std::nullopt;
    }
    if (!world.setBlockState(hit.blockX, hit.blockY, hit.blockZ, *turned)) {
        return std::nullopt;
    }
    world.scheduleBlockTick(hit.blockX, hit.blockY, hit.blockZ, targetBlock(),
                            byArrow ? kArrowHoldTicks : kThrownHoldTicks);
    return BlockChange{hit.blockX, hit.blockY, hit.blockZ, *turned, {}};
}

[[nodiscard]] std::optional<BlockChange> shatter(WorldState& world, const entity::BlockHit& hit) {
    const std::int32_t air = registry::blocks::air().stateId;
    if (!world.setBlockState(hit.blockX, hit.blockY, hit.blockZ, air)) {
        return std::nullopt;
    }
    return BlockChange{hit.blockX, hit.blockY, hit.blockZ, air, {}};
}

/// parity: `TntBlock.onProjectileHit` into `TntBlock.prime`: the block goes and the entity that
/// takes its place is what counts down.
[[nodiscard]] std::optional<BlockChange> primeTnt(WorldState& world, const entity::BlockHit& hit, bool tntExplodes) {
    // parity: `TntBlock.prime` answers false with the `tnt_explodes` rule off, and `onProjectileHit`
    // only removes the block when it answered true.
    if (!tntExplodes) {
        return std::nullopt;
    }
    const std::int32_t air = registry::blocks::air().stateId;
    if (!world.setBlockState(hit.blockX, hit.blockY, hit.blockZ, air)) {
        return std::nullopt;
    }
    world.primeTnt(hit.blockX, hit.blockY, hit.blockZ, "prime_fuse");
    return BlockChange{hit.blockX, hit.blockY, hit.blockZ, air, {}};
}

/// parity: `AbstractCandleBlock.onProjectileHit` and `CampfireBlock.onProjectileHit`: both want a
/// projectile that is **alight** and a block that is not lit already. A candle also refuses while it
/// is waterlogged, and so does a campfire.
[[nodiscard]] std::optional<BlockChange> light(WorldState& world, const entity::BlockHit& hit, std::int32_t stateId,
                                               const wire::common::Identifier& block) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto value = [&properties](const std::string& key) {
        const auto found = properties.find(key);
        return found == properties.end() ? std::string_view{} : std::string_view{found->second};
    };
    if (value("lit") == "true" || value("waterlogged") == "true") {
        return std::nullopt;
    }
    const std::optional<std::int32_t> turned =
        registry::blockStateDefinitions::stateIdOf(block, {{"lit", "true"}}, stateId);
    if (!turned.has_value() || !world.setBlockState(hit.blockX, hit.blockY, hit.blockZ, *turned)) {
        return std::nullopt;
    }
    return BlockChange{hit.blockX, hit.blockY, hit.blockZ, *turned, {}};
}

/// The face that name stands for, or nothing for a name that is not one.
[[nodiscard]] std::optional<registry::BlockFace> faceNamed(std::string_view name) {
    static constexpr std::array<std::pair<std::string_view, registry::BlockFace>, 6> kNames{
        {{"down", registry::BlockFace::Down},
         {"up", registry::BlockFace::Up},
         {"north", registry::BlockFace::North},
         {"south", registry::BlockFace::South},
         {"west", registry::BlockFace::West},
         {"east", registry::BlockFace::East}}};
    for (const auto& [named, face] : kNames) {
        if (named == name) {
            return face;
        }
    }
    return std::nullopt;
}

/// parity: `isProperHit`'s own `0.8124F`: the top fifth of the bell does not ring it.
constexpr double kBellTopEdge = static_cast<double>(0.8124F);

/// parity: `BellBlock.isProperHit`. A bell only rings when it is struck on the right side, and which
/// side that is depends on how it hangs: a floor bell along the axis it faces, a wall bell across it,
/// a ceiling bell from anywhere. Never from above or below, and never on the top fifth of it.
[[nodiscard]] bool properBellHit(const std::map<std::string, std::string>& properties, const entity::BlockHit& hit) {
    if (registry::axisOf(hit.face) == 1) {
        return false;
    }
    if (hit.y - std::floor(hit.y) > kBellTopEdge) {
        return false;
    }
    const auto value = [&properties](const std::string& key) {
        const auto found = properties.find(key);
        return found == properties.end() ? std::string_view{} : std::string_view{found->second};
    };
    const std::optional<registry::BlockFace> facing = faceNamed(value("facing"));
    if (!facing.has_value()) {
        return false;
    }
    const std::string_view attachment = value("attachment");
    if (attachment == "ceiling") {
        return true;
    }
    const bool sameAxis = registry::axisOf(*facing) == registry::axisOf(hit.face);
    if (attachment == "floor") {
        return sameAxis;
    }
    return (attachment == "single_wall" || attachment == "double_wall") && !sameAxis;
}

} // namespace

std::optional<BlockChange> onHit(WorldState& world, std::int64_t gameTime, const entity::BlockHit& hit, bool byArrow,
                                 bool burning, bool tntExplodes) {
    const std::optional<std::int32_t> stateId = world.blockState(hit.blockX, hit.blockY, hit.blockZ);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    const std::string& path = block->path();
    if (path == "target") {
        return hitTarget(world, hit, *stateId, *block, byArrow);
    }
    // parity: `TntBlock.onProjectileHit`, one of the few ways tnt goes off with no redstone and no
    // hand: the projectile has to be alight, and then the block is gone and the entity is ticking.
    if (path == "tnt") {
        return burning ? primeTnt(world, hit, tntExplodes) : std::nullopt;
    }
    // parity: `ChorusFlowerBlock` and `DecoratedPotBlock`, which simply break.
    if (path == "chorus_flower" || path == "decorated_pot") {
        return shatter(world, hit);
    }
    // parity: a lit candle or campfire is a light source and a block state, so a flaming arrow that
    // sets one going is as visible as one that sets off tnt.
    if (burning && (path.ends_with("candle") || path.ends_with("candle_cake") || path.ends_with("campfire"))) {
        return light(world, hit, *stateId, *block);
    }
    // parity: `BellBlock.onProjectileHit`, which rings it through the same `onHit` a hand uses and
    // with the same "was it a proper hit" rule. The block state does not move; what comes out is the
    // ring itself, which a sculk sensor hears as eleven.
    if (path == "bell") {
        const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
        if (!properBellHit(properties, hit)) {
            return std::nullopt;
        }
        if (world.hasSculkSensorNear(hit.blockX, hit.blockY, hit.blockZ, vibrations::kRange)) {
            vibrations::emit(world, "block_change", hit.blockX + 0.5, hit.blockY + 0.5, hit.blockZ + 0.5, gameTime);
        }
        return BlockChange{hit.blockX, hit.blockY, hit.blockZ, *stateId, {}};
    }
    if (path == "big_dripleaf") {
        // parity: `BigDripleafBlock.onProjectileHit` tips it right over **and books the hundred
        // ticks it lies flat**, which is what brings it back up.
        return dripleaves::setTilt(world, hit.blockX, hit.blockY, hit.blockZ, "full", gameTime);
    }
    return std::nullopt;
}

std::int32_t signalFor(const entity::BlockHit& hit) {
    const double fracX = std::abs(fraction(hit.x) - kHalf);
    const double fracY = std::abs(fraction(hit.y) - kHalf);
    const double fracZ = std::abs(fraction(hit.z) - kHalf);
    const int axis = registry::axisOf(hit.face);
    double distance = std::max(fracY, fracZ);
    if (axis == 1) {
        distance = std::max(fracX, fracZ);
    } else if (axis == 2) {
        distance = std::max(fracX, fracY);
    }
    const double scaled = std::clamp((kHalf - distance) / kHalf, 0.0, 1.0);
    return std::max(1, static_cast<std::int32_t>(std::ceil(kMaxSignal * scaled)));
}

std::optional<BlockChange> releaseTarget(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr || block->path() != "target") {
        return std::nullopt;
    }
    const std::map<std::string, std::string> current = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto power = current.find("power");
    if (power == current.end() || power->second == "0") {
        return std::nullopt;
    }
    const std::map<std::string, std::string> properties{{"power", "0"}};
    const std::optional<std::int32_t> turned = registry::blockStateDefinitions::stateIdOf(*block, properties, *stateId);
    if (!turned.has_value()) {
        return std::nullopt;
    }
    if (!world.setBlockState(x, y, z, *turned)) {
        return std::nullopt;
    }
    return BlockChange{x, y, z, *turned, {}};
}

} // namespace basalt::server::runtime::world::projectileHits
