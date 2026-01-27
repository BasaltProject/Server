#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
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

/// parity: `VibrationSystem.getRedstoneStrengthForDistance`, fifteen at the sensor and one at the
/// edge of what it hears.
[[nodiscard]] std::int32_t strengthFor(double distance) {
    const double scale = static_cast<double>(redstone::kMaxSignal) / static_cast<double>(kRange);
    return std::max(1, redstone::kMaxSignal - static_cast<std::int32_t>(std::floor(scale * distance)));
}

[[nodiscard]] bool isSensor(const wire::common::Identifier& block) {
    return block == sensor() || block == calibrated();
}

/// What a sensor has written down: either the noise on its way or the best one heard this turn.
///
/// parity: `VibrationInfo`, minus the two uuids. They exist for the resonance through amethyst and
/// for the advancement, and this build models neither.
struct Noise {
    std::string event;
    float distance = 0.0F;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

[[nodiscard]] wire::nbt::NbtCompound written(const Noise& noise) {
    wire::nbt::NbtCompound tag;
    tag.putString("game_event", "minecraft:" + noise.event);
    tag.putFloat("distance", noise.distance);
    tag.set("pos", wire::nbt::NbtTag::ofList(
                       wire::nbt::NbtList{wire::nbt::NbtType::Double,
                                          {wire::nbt::NbtTag::ofDouble(noise.x), wire::nbt::NbtTag::ofDouble(noise.y),
                                           wire::nbt::NbtTag::ofDouble(noise.z)}}));
    return tag;
}

[[nodiscard]] std::optional<Noise> read(const wire::nbt::NbtCompound* tag) {
    if (tag == nullptr) {
        return std::nullopt;
    }
    const wire::nbt::NbtList* pos = tag->getList("pos");
    if (pos == nullptr || pos->size() != 3) {
        return std::nullopt;
    }
    std::string_view name = tag->getString("game_event");
    if (const std::size_t colon = name.find(':'); colon != std::string_view::npos) {
        name = name.substr(colon + 1);
    }
    const auto at = [pos](std::size_t index) {
        const double* value = (*pos)[index].get<double>();
        return value == nullptr ? 0.0 : *value;
    };
    return Noise{.event = std::string{name}, .distance = tag->getFloat("distance"), .x = at(0), .y = at(1), .z = at(2)};
}

/// The sensor's own block entity, made on the spot if the world never gave it one.
[[nodiscard]] wire::nbt::NbtCompound* dataOf(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                             const wire::common::Identifier& block) {
    server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    if (chunk == nullptr) {
        return nullptr;
    }
    if (server::world::ChunkBlockEntity* entity = chunk->blockEntityAt(x & 15, y, z & 15); entity != nullptr) {
        return &entity->data();
    }
    chunk->setBlockEntity(server::world::ChunkBlockEntity{x & 15, y, z & 15, block, wire::nbt::NbtCompound{}});
    server::world::ChunkBlockEntity* made = chunk->blockEntityAt(x & 15, y, z & 15);
    return made == nullptr ? nullptr : &made->data();
}

/// parity: `SculkSensorBlock.activate`, which lights it and books the thirty ticks it stays on.
///
/// `power` and the frequency are **two different numbers**, and mixing them was a real bug here: the
/// state's `power` is how close the noise was (`VibrationSystem.getRedstoneStrengthForDistance`) and
/// the frequency is what kind of noise it was, which lives in the block entity and is what a
/// comparator reads. Measured in `sensormuerte`: a pig dying three and pico blocks away leaves the
/// state at ten and the comparator at fifteen.
[[nodiscard]] std::optional<BlockChange> activate(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                  std::int32_t stateId, const wire::common::Identifier& block,
                                                  std::int32_t frequency, double distance) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto phase = properties.find("sculk_sensor_phase");
    if (phase == properties.end() || phase->second != "inactive") {
        return std::nullopt;
    }
    const std::optional<std::int32_t> turned = registry::blockStateDefinitions::stateIdOf(
        block, {{"sculk_sensor_phase", "active"}, {"power", std::to_string(strengthFor(distance))}}, stateId);
    if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
        return std::nullopt;
    }
    // parity: `SculkSensorBlockEntity.setLastVibrationFrequency`, written under vanilla's own name.
    if (wire::nbt::NbtCompound* data = dataOf(world, x, y, z, block); data != nullptr) {
        data->putInt(kFrequencyKey, frequency);
    }
    world.scheduleBlockTick(x, y, z, block, kActiveTicks);
    return BlockChange{.x = x, .y = y, .z = z, .stateId = *turned};
}

/// The listener's own tag, without making one where there is none.
[[nodiscard]] wire::nbt::NbtCompound* heldBy(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    if (chunk == nullptr) {
        return nullptr;
    }
    server::world::ChunkBlockEntity* entity = chunk->blockEntityAt(x & 15, y, z & 15);
    return entity == nullptr ? nullptr : &entity->data();
}

/// Books the sensor its next turn, but only while it still has something to move. Vanilla's block
/// entity ticks every turn of the world and never has to ask; here the turn is booked by whoever
/// leaves work behind, which includes a sensor loaded from disk with a noise still on its way.
void wake(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, const wire::common::Identifier& block) {
    const wire::nbt::NbtCompound* data = heldBy(world, x, y, z);
    if (data == nullptr) {
        return;
    }
    const wire::nbt::NbtCompound* listener = data->getCompound(kListenerKey);
    if (listener == nullptr) {
        return;
    }
    const wire::nbt::NbtCompound* selector = listener->getCompound("selector");
    if (listener->getCompound("event") == nullptr &&
        (selector == nullptr || selector->getCompound("event") == nullptr)) {
        return;
    }
    world.scheduleBlockTick(x, y, z, block, 1);
}

/// parity: `VibrationSystem.Listener.handleGameEvent` over `VibrationSelector.addCandidate`. Nothing
/// lights up here: the sensor writes the noise down and waits its turn.
void hear(WorldState& world, std::int32_t sensorX, std::int32_t sensorY, std::int32_t sensorZ, std::int32_t stateId,
          const wire::common::Identifier& block, std::string_view event, double x, double y, double z,
          std::int64_t gameTime) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto phase = properties.find("sculk_sensor_phase");
    if (phase == properties.end() || phase->second != "inactive") {
        return;
    }
    // parity: `SculkSensorBlockEntity.VibrationUser.canReceiveVibration`, the one event it refuses:
    // a sensor does not hear its own square being placed or broken.
    if (static_cast<std::int32_t>(std::floor(x)) == sensorX && static_cast<std::int32_t>(std::floor(y)) == sensorY &&
        static_cast<std::int32_t>(std::floor(z)) == sensorZ && (event == "block_destroy" || event == "block_place")) {
        return;
    }
    wire::nbt::NbtCompound* data = dataOf(world, sensorX, sensorY, sensorZ, block);
    if (data == nullptr) {
        return;
    }
    const wire::nbt::NbtCompound* held = data->getCompound(kListenerKey);
    wire::nbt::NbtCompound listener = held == nullptr ? wire::nbt::NbtCompound{} : *held;
    // parity: one already on its way makes the sensor deaf until it lands.
    if (listener.getCompound("event") != nullptr) {
        return;
    }
    // parity: the distance that decides the travel is measured from where the noise happened to the
    // **centre** of the sensor's square, not between the two squares.
    const double dx = (static_cast<double>(sensorX) + 0.5) - x;
    const double dy = (static_cast<double>(sensorY) + 0.5) - y;
    const double dz = (static_cast<double>(sensorZ) + 0.5) - z;
    const auto distance = static_cast<float>(std::sqrt((dx * dx) + (dy * dy) + (dz * dz)));
    const wire::nbt::NbtCompound* chosen = listener.getCompound("selector");
    wire::nbt::NbtCompound selector = chosen == nullptr ? wire::nbt::NbtCompound{} : *chosen;
    // parity: `VibrationSelector.shouldReplaceVibration`: only a noise from this same turn competes,
    // the closer one wins, and a tie goes to the louder frequency.
    if (const std::optional<Noise> standing = read(selector.getCompound("event")); standing.has_value()) {
        if (selector.getLong("tick", -1) != gameTime || distance > standing->distance) {
            return;
        }
        if (distance == standing->distance && frequencyOf(event) <= frequencyOf(standing->event)) {
            return;
        }
    }
    selector.set("event", wire::nbt::NbtTag::ofCompound(written(
                              Noise{.event = std::string{event}, .distance = distance, .x = x, .y = y, .z = z})));
    selector.putLong("tick", gameTime);
    listener.set("selector", wire::nbt::NbtTag::ofCompound(std::move(selector)));
    data->set(kListenerKey, wire::nbt::NbtTag::ofCompound(std::move(listener)));
    world.scheduleBlockTick(sensorX, sensorY, sensorZ, block, 1);
}

/// parity: `VibrationSystem.Ticker.tick`: pick what was heard last turn, then move it one block
/// closer, and light the sensor on the turn it arrives.
[[nodiscard]] std::optional<BlockChange> travel(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                std::int32_t stateId, const wire::common::Identifier& block,
                                                std::int64_t gameTime) {
    wire::nbt::NbtCompound* data = heldBy(world, x, y, z);
    if (data == nullptr) {
        return std::nullopt;
    }
    const wire::nbt::NbtCompound* held = data->getCompound(kListenerKey);
    if (held == nullptr) {
        return std::nullopt;
    }
    wire::nbt::NbtCompound listener = *held;
    std::optional<Noise> current = read(listener.getCompound("event"));
    std::int32_t delay = listener.getInt("event_delay", 0);
    if (!current.has_value()) {
        const wire::nbt::NbtCompound* chosen = listener.getCompound("selector");
        wire::nbt::NbtCompound selector = chosen == nullptr ? wire::nbt::NbtCompound{} : *chosen;
        current = read(selector.getCompound("event"));
        if (!current.has_value()) {
            return std::nullopt;
        }
        // parity: `VibrationSelector.chosenCandidate` answers nothing for a noise heard on this same
        // turn: the sensor is still collecting, and picks on the next one.
        if (selector.getLong("tick", -1) >= gameTime) {
            world.scheduleBlockTick(x, y, z, block, 1);
            return std::nullopt;
        }
        // parity: `VibrationSystem.User.calculateTravelTimeInTicks`, a tick per whole block.
        delay = static_cast<std::int32_t>(std::floor(current->distance));
        selector.remove("event");
        selector.putLong("tick", -1);
        listener.set("selector", wire::nbt::NbtTag::ofCompound(std::move(selector)));
    }
    // parity: the countdown spends a turn on the very tick the noise is picked, so one born at zero
    // or one distance arrives right there.
    delay = std::max(0, delay - 1);
    if (delay > 0) {
        listener.set("event", wire::nbt::NbtTag::ofCompound(written(*current)));
        listener.putInt("event_delay", delay);
        data->set(kListenerKey, wire::nbt::NbtTag::ofCompound(std::move(listener)));
        world.scheduleBlockTick(x, y, z, block, 1);
        return std::nullopt;
    }
    listener.remove("event");
    listener.putInt("event_delay", 0);
    data->set(kListenerKey, wire::nbt::NbtTag::ofCompound(std::move(listener)));
    // parity: `VibrationSystem.Listener.distanceBetweenInBlocks`. The signal is how far the two
    // **squares** were, which is not the distance the noise travelled.
    const double toX = static_cast<double>(x) - std::floor(current->x);
    const double toY = static_cast<double>(y) - std::floor(current->y);
    const double toZ = static_cast<double>(z) - std::floor(current->z);
    return activate(world, x, y, z, stateId, block, frequencyOf(current->event),
                    std::sqrt((toX * toX) + (toY * toY) + (toZ * toZ)));
}
} // namespace

std::int32_t frequencyOf(std::string_view event) {
    const auto found = frequencies().find(event);
    return found == frequencies().end() ? 0 : found->second;
}

void emit(WorldState& world, std::string_view event, double x, double y, double z, std::int64_t gameTime) {
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
                // parity: `EuclideanGameEventListenerRegistry.filterInvalidListeners` measures the
                // range between the two **blocks**, not between the exact points.
                if ((offsetX * offsetX) + (offsetY * offsetY) + (offsetZ * offsetZ) > kRange * kRange) {
                    continue;
                }
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
                hear(world, sensorX, sensorY, sensorZ, *stateId, *block, event, x, y, z, gameTime);
            }
        }
    }
}

std::optional<BlockChange> tick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                std::int64_t gameTime) {
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
    // parity: `VibrationSystem.Ticker.tick`, which runs on the block entity's own turn and is the
    // only thing that ever lights the sensor. An active one is busy and hears nothing.
    if (phase == "inactive") {
        return travel(world, x, y, z, *stateId, *block, gameTime);
    }
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
    } else {
        // A world saved while a noise was travelling comes back with it written down and nothing
        // booked to move it: this is where such a sensor picks the countdown back up.
        wake(world, x, y, z, *block);
    }
    return BlockChange{.x = x, .y = y, .z = z, .stateId = *turned};
}

bool isInRange(std::int32_t sensorX, std::int32_t sensorY, std::int32_t sensorZ, double x, double y, double z) {
    return std::abs(sensorX - x) <= kRange && std::abs(sensorY - y) <= kRange && std::abs(sensorZ - z) <= kRange;
}

} // namespace basalt::server::runtime::world::vibrations
