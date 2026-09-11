#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/BlockInteractions.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/runtime/world/WindBursts.hpp>

#include <cstdint>
#include <map>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world::windBursts {
namespace {

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

void heard(WorldState& world, std::string_view event, std::int32_t x, std::int32_t y, std::int32_t z,
           std::int64_t gameTime) {
    if (!world.hasSculkSensorNear(x, y, z, vibrations::kCalibratedRange)) {
        return;
    }
    vibrations::emit(world, event, x + 0.5, y + 0.5, z + 0.5, gameTime);
}

/// parity: `AbstractCandleBlock.extinguish`, the one thing the gust puts out rather than opens.
[[nodiscard]] bool isCandle(std::string_view path) { return path.ends_with("candle") || path.ends_with("candle_cake"); }

/// What the gust does to one square, or nothing when the block is not one it moves.
void knock(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int64_t gameTime,
           std::vector<BlockChange>& changes) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr) {
        return;
    }
    const std::string_view path = block->path();
    if (isCandle(path)) {
        if (propertyOf(*stateId, "lit") != "true") {
            return;
        }
        const std::optional<std::int32_t> out =
            registry::blockStateDefinitions::stateIdOf(*block, {{"lit", "false"}}, *stateId);
        if (!out.has_value() || !world.setBlockState(x, y, z, *out)) {
            return;
        }
        changes.push_back(BlockChange{.x = x, .y = y, .z = z, .stateId = *out});
        heard(world, "block_change", x, y, z, gameTime);
        return;
    }
    // parity: `BellBlock.onExplosionHit` rings it through `attemptToRing` with no direction of its
    // own and no "was it a proper hit" test. No state moves; what comes out is the ring.
    if (path == "bell") {
        heard(world, "block_change", x, y, z, gameTime);
        return;
    }
    const bool button = path.ends_with("_button");
    const bool lever = path == "lever";
    const bool door = path.ends_with("_door") && !path.ends_with("_trapdoor");
    const bool opens = door || path.ends_with("_trapdoor") || path.ends_with("_fence_gate");
    if (!button && !lever && !opens) {
        return;
    }
    // parity: every `onExplosionHit` but the lever's refuses while the block is held by a signal,
    // and a door only listens on its lower half.
    if (!lever && propertyOf(*stateId, "powered") == "true") {
        return;
    }
    if (door && propertyOf(*stateId, "half") != "lower") {
        return;
    }
    const std::optional<blockInteractions::BlockToggle> toggled = blockInteractions::toggle(world, x, y, z);
    if (!toggled.has_value() || toggled->changes.empty()) {
        return;
    }
    // parity: `ButtonBlock.press` books the release the same way a hand's click does.
    if (button) {
        if (const std::optional<std::int32_t> pressed = blockInteractions::pressedTicks(*stateId);
            pressed.has_value()) {
            world.scheduleBlockTick(x, y, z, *block, *pressed);
        }
    }
    const std::int32_t after = toggled->changes.front().stateId;
    if (opens) {
        heard(world, propertyOf(after, "open") == "true" ? "block_open" : "block_close", x, y, z, gameTime);
    } else {
        heard(world, propertyOf(after, "powered") == "true" ? "block_activate" : "block_deactivate", x, y, z, gameTime);
    }
    changes.insert(changes.end(), toggled->changes.begin(), toggled->changes.end());
}

} // namespace

std::vector<BlockChange> burst(WorldState& world, std::int32_t minY, std::int32_t maxY, double x, double y, double z,
                               std::int64_t gameTime, random::RandomSource& random, const explosions::Spares& stops) {
    std::vector<BlockChange> changes;
    for (const explosions::Broken& square : explosions::reaches(world, minY, maxY, x, y, z, kRadius, random, stops)) {
        knock(world, square.x, square.y, square.z, gameTime, changes);
    }
    return changes;
}

} // namespace basalt::server::runtime::world::windBursts
