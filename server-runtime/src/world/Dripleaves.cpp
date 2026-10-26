#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/config/PurpurRules.hpp>
#include <basalt/server/runtime/world/Dripleaves.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <map>
#include <string>

namespace basalt::server::runtime::world::dripleaves {
namespace {

[[nodiscard]] const wire::common::Identifier& block() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("big_dripleaf");
    return instance;
}

[[nodiscard]] std::string tiltIn(std::int32_t stateId) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find("tilt");
    return found == properties.end() ? std::string{} : found->second;
}

/// parity: `Tilt.causesVibration`, true for every stage but `unstable`.
[[nodiscard]] bool causesVibration(std::string_view tilt) { return tilt != "unstable"; }

/// parity: `DELAY_UNTIL_NEXT_TILT_STATE`, with `none` booking nothing.
[[nodiscard]] std::int32_t delayFor(const WorldState& world, std::string_view tilt) {
    const config::PurpurRules::Blocks& rules = world.gameplay().blocks;
    if (tilt == "unstable") {
        return rules.bigDripleafTiltDelayUnstable;
    }
    if (tilt == "partial") {
        return rules.bigDripleafTiltDelayPartial;
    }
    return tilt == "full" ? rules.bigDripleafTiltDelayFull : 0;
}

} // namespace

bool isDripleaf(std::int32_t stateId) {
    const wire::common::Identifier* here = registry::blockStateDefinitions::blockOf(stateId);
    return here != nullptr && *here == block();
}

std::optional<BlockChange> setTilt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                   std::string_view tilt, std::int64_t gameTime) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || !isDripleaf(*stateId)) {
        return std::nullopt;
    }
    const std::string was = tiltIn(*stateId);
    const std::optional<std::int32_t> turned =
        registry::blockStateDefinitions::stateIdOf(block(), {{"tilt", std::string{tilt}}}, *stateId);
    if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
        return std::nullopt;
    }
    if (const std::int32_t delay = delayFor(world, tilt); delay > 0) {
        world.scheduleBlockTick(x, y, z, block(), delay);
    }
    if (was == tilt) {
        return std::nullopt;
    }
    if (causesVibration(tilt) && world.hasSculkSensorNear(x, y, z, vibrations::kCalibratedRange)) {
        vibrations::emit(world, "block_change", x + 0.5, y + 0.5, z + 0.5, gameTime);
    }
    return BlockChange{.x = x, .y = y, .z = z, .stateId = *turned};
}

std::optional<BlockChange> stoodOn(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y,
                                   std::int32_t z, double entityY, bool onGround, std::int64_t gameTime) {
    if (!onGround || entityY <= static_cast<double>(y) + kStandingHeight) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || !isDripleaf(*stateId) || tiltIn(*stateId) != "none") {
        return std::nullopt;
    }
    // parity: a leaf held up by a signal does not start tipping at all, which is what makes a.
    if (redstone::bestNeighbourSignal(world, worldName, x, y, z) > 0) {
        return std::nullopt;
    }
    return setTilt(world, x, y, z, "unstable", gameTime);
}

std::optional<BlockChange> react(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y,
                                 std::int32_t z, std::int64_t gameTime) {
    if (redstone::bestNeighbourSignal(world, worldName, x, y, z) == 0) {
        return std::nullopt;
    }
    return setTilt(world, x, y, z, "none", gameTime);
}

std::optional<BlockChange> tick(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y,
                                std::int32_t z, std::int64_t gameTime) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || !isDripleaf(*stateId)) {
        return std::nullopt;
    }
    if (redstone::bestNeighbourSignal(world, worldName, x, y, z) > 0) {
        return setTilt(world, x, y, z, "none", gameTime);
    }
    const std::string tilt = tiltIn(*stateId);
    if (tilt == "unstable") {
        return setTilt(world, x, y, z, "partial", gameTime);
    }
    if (tilt == "partial") {
        return setTilt(world, x, y, z, "full", gameTime);
    }
    if (tilt == "full") {
        return setTilt(world, x, y, z, "none", gameTime);
    }
    return std::nullopt;
}

} // namespace basalt::server::runtime::world::dripleaves
