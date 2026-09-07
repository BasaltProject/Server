#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/ProjectileHits.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>

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

/// parity: `BigDripleafBlock.onProjectileHit`, which tips it right over.
[[nodiscard]] std::optional<BlockChange> tiltDripleaf(WorldState& world, const entity::BlockHit& hit,
                                                      std::int32_t stateId, const wire::common::Identifier& block) {
    const std::map<std::string, std::string> properties{{"tilt", "full"}};
    const std::optional<std::int32_t> turned = registry::blockStateDefinitions::stateIdOf(block, properties, stateId);
    if (!turned.has_value() || *turned == stateId) {
        return std::nullopt;
    }
    if (!world.setBlockState(hit.blockX, hit.blockY, hit.blockZ, *turned)) {
        return std::nullopt;
    }
    return BlockChange{hit.blockX, hit.blockY, hit.blockZ, *turned, {}};
}

} // namespace

std::optional<BlockChange> onHit(WorldState& world, const entity::BlockHit& hit, bool byArrow) {
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
    // parity: `ChorusFlowerBlock` and `DecoratedPotBlock`, which simply break.
    if (path == "chorus_flower" || path == "decorated_pot") {
        return shatter(world, hit);
    }
    if (path == "big_dripleaf") {
        return tiltDripleaf(world, hit, *stateId, *block);
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
