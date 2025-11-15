#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <cmath>
#include <map>
#include <string>
#include <unordered_map>

namespace basalt::server::runtime::world::vibrations {
namespace {

[[nodiscard]] const wire::common::Identifier& sensor() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("sculk_sensor");
    return instance;
}

[[nodiscard]] const wire::common::Identifier& calibrated() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("calibrated_sculk_sensor");
    return instance;
}

/// parity: `VibrationSystem.VIBRATION_FREQUENCY_FOR_EVENT`, the whole table. What is missing from it
/// is the events this build does not raise yet, not a different set of numbers.
[[nodiscard]] const std::unordered_map<std::string_view, std::int32_t>& frequencies() {
    static const std::unordered_map<std::string_view, std::int32_t> table{
        {"step", 1},
        {"swim", 1},
        {"flap", 1},
        {"projectile_land", 2},
        {"hit_ground", 2},
        {"splash", 2},
        {"bounce", 2},
        {"item_interact_finish", 3},
        {"projectile_shoot", 3},
        {"instrument_play", 3},
        {"entity_action", 4},
        {"elytra_glide", 4},
        {"unequip", 4},
        {"entity_dismount", 5},
        {"equip", 5},
        {"entity_interact", 6},
        {"shear", 6},
        {"entity_mount", 6},
        {"entity_damage", 7},
        {"drink", 8},
        {"eat", 8},
        {"container_close", 9},
        {"block_close", 9},
        {"block_deactivate", 9},
        {"block_detach", 9},
        {"container_open", 10},
        {"block_open", 10},
        {"block_activate", 10},
        {"block_attach", 10},
        {"prime_fuse", 10},
        {"note_block_play", 10},
        {"block_change", 11},
        {"block_destroy", 12},
        {"fluid_pickup", 12},
        {"block_place", 13},
        {"fluid_place", 13},
        {"entity_place", 14},
        {"lightning_strike", 14},
        {"teleport", 14},
        {"entity_die", 15},
        {"explode", 15},
    };
    return table;
}

[[nodiscard]] bool isSensor(const wire::common::Identifier& block) {
    return block == sensor() || block == calibrated();
}

/// parity: `SculkSensorBlock.activate`, which lights it and books the thirty ticks it stays on.
void activate(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
              const wire::common::Identifier& block, std::int32_t frequency, std::vector<BlockChange>& changes) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto phase = properties.find("sculk_sensor_phase");
    if (phase == properties.end() || phase->second != "inactive") {
        return;
    }
    const std::optional<std::int32_t> turned = registry::blockStateDefinitions::stateIdOf(
        block, {{"sculk_sensor_phase", "active"}, {"power", std::to_string(frequency)}}, stateId);
    if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
        return;
    }
    changes.push_back(BlockChange{.x = x, .y = y, .z = z, .stateId = *turned});
    world.scheduleBlockTick(x, y, z, block, kActiveTicks);
}

} // namespace

std::int32_t frequencyOf(std::string_view event) {
    const auto found = frequencies().find(event);
    return found == frequencies().end() ? 0 : found->second;
}

void emit(WorldState& world, std::string_view event, double x, double y, double z, std::vector<BlockChange>& changes) {
    const std::int32_t frequency = frequencyOf(event);
    if (frequency <= 0) {
        return;
    }
    const auto blockX = static_cast<std::int32_t>(std::floor(x));
    const auto blockY = static_cast<std::int32_t>(std::floor(y));
    const auto blockZ = static_cast<std::int32_t>(std::floor(z));
    for (std::int32_t offsetY = -kRange; offsetY <= kRange; ++offsetY) {
        for (std::int32_t offsetZ = -kRange; offsetZ <= kRange; ++offsetZ) {
            for (std::int32_t offsetX = -kRange; offsetX <= kRange; ++offsetX) {
                const std::int32_t sensorX = blockX + offsetX;
                const std::int32_t sensorY = blockY + offsetY;
                const std::int32_t sensorZ = blockZ + offsetZ;
                const std::optional<std::int32_t> stateId = world.blockState(sensorX, sensorY, sensorZ);
                if (!stateId.has_value()) {
                    continue;
                }
                const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
                if (block == nullptr || !isSensor(*block)) {
                    continue;
                }
                // parity: the listener's own radius is a sphere, not the cube walked here.
                const double distance =
                    std::sqrt(static_cast<double>((offsetX * offsetX) + (offsetY * offsetY) + (offsetZ * offsetZ)));
                if (distance > kRange) {
                    continue;
                }
                activate(world, sensorX, sensorY, sensorZ, *stateId, *block, frequency, changes);
            }
        }
    }
}

std::optional<BlockChange> tick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr || !isSensor(*block)) {
        return std::nullopt;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto found = properties.find("sculk_sensor_phase");
    if (found == properties.end()) {
        return std::nullopt;
    }
    const std::string& phase = found->second;
    std::map<std::string, std::string> wanted;
    if (phase == "active") {
        wanted = {{"sculk_sensor_phase", "cooldown"}, {"power", "0"}};
    } else if (phase == "cooldown") {
        wanted = {{"sculk_sensor_phase", "inactive"}, {"power", "0"}};
    } else {
        return std::nullopt;
    }
    const std::optional<std::int32_t> turned = registry::blockStateDefinitions::stateIdOf(*block, wanted, *stateId);
    if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
        return std::nullopt;
    }
    if (phase == "active") {
        world.scheduleBlockTick(x, y, z, *block, kCooldownTicks);
    }
    return BlockChange{.x = x, .y = y, .z = z, .stateId = *turned};
}

bool isInRange(std::int32_t sensorX, std::int32_t sensorY, std::int32_t sensorZ, double x, double y, double z) {
    return std::abs(sensorX - x) <= kRange && std::abs(sensorY - y) <= kRange && std::abs(sensorZ - z) <= kRange;
}

} // namespace basalt::server::runtime::world::vibrations
