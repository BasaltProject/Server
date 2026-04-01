#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/entity/BlockRay.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <unordered_map>

namespace basalt::server::runtime::world::vibrations {
namespace {

/// parity: `Direction.values()`, which is what `isOccluded` walks. The order does not decide anything
/// here (all six have to be blocked), but the set does.
constexpr std::array<registry::BlockFace, 6> kFaces{registry::BlockFace::Down,  registry::BlockFace::Up,
                                                    registry::BlockFace::North, registry::BlockFace::South,
                                                    registry::BlockFace::West,  registry::BlockFace::East};

[[nodiscard]] const wire::common::Identifier& sensor() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("sculk_sensor");
    return instance;
}

/// The four horizontal names a `facing` can hold, which is all a calibrated sensor ever has.
[[nodiscard]] std::optional<registry::BlockFace> faceNamed(std::string_view name) {
    if (name == "north") {
        return registry::BlockFace::North;
    }
    if (name == "south") {
        return registry::BlockFace::South;
    }
    if (name == "west") {
        return registry::BlockFace::West;
    }
    if (name == "east") {
        return registry::BlockFace::East;
    }
    return std::nullopt;
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
[[nodiscard]] std::int32_t strengthFor(double distance, std::int32_t radius) {
    const double scale = static_cast<double>(redstone::kMaxSignal) / static_cast<double>(radius);
    return std::max(1, redstone::kMaxSignal - static_cast<std::int32_t>(std::floor(scale * distance)));
}

/// parity: the two sensors have different radii, and it is the block that says which.
[[nodiscard]] std::int32_t radiusOf(const wire::common::Identifier& block) {
    return block == calibrated() ? kCalibratedRange : kRange;
}

/// parity: `getActiveTicks`, overridden by the calibrated sensor and by nothing else.
[[nodiscard]] std::int32_t activeTicksOf(const wire::common::Identifier& block) {
    return block == calibrated() ? kCalibratedActiveTicks : kActiveTicks;
}

[[nodiscard]] bool isSensor(const wire::common::Identifier& block) {
    return block == sensor() || block == calibrated();
}

[[nodiscard]] const wire::common::Identifier& shrieker() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("sculk_shrieker");
    return instance;
}

/// What a listener has written down: either the noise on its way or the best one heard this turn.
///
/// parity: `VibrationInfo`.
struct Noise {
    std::string event;
    float distance = 0.0F;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    Source source;
};

/// parity: `UUIDUtil.CODEC`, four ints with the high word first.
void putUuid(wire::nbt::NbtCompound& tag, std::string_view key, const std::optional<wire::common::Uuid>& uuid) {
    if (!uuid.has_value()) {
        return;
    }
    tag.putIntArray(key, {static_cast<std::int32_t>(static_cast<std::uint32_t>(uuid->high() >> 32)),
                          static_cast<std::int32_t>(static_cast<std::uint32_t>(uuid->high())),
                          static_cast<std::int32_t>(static_cast<std::uint32_t>(uuid->low() >> 32)),
                          static_cast<std::int32_t>(static_cast<std::uint32_t>(uuid->low()))});
}

[[nodiscard]] std::optional<wire::common::Uuid> uuidAt(const wire::nbt::NbtCompound& tag, std::string_view key) {
    const std::span<const std::int32_t> parts = tag.getIntArray(key);
    if (parts.size() != 4) {
        return std::nullopt;
    }
    const auto word = [parts](std::size_t at) {
        return static_cast<std::uint64_t>(static_cast<std::uint32_t>(parts[at]));
    };
    return wire::common::Uuid{(word(0) << 32) | word(1), (word(2) << 32) | word(3)};
}

[[nodiscard]] wire::nbt::NbtCompound written(const Noise& noise) {
    wire::nbt::NbtCompound tag;
    tag.putString("game_event", "minecraft:" + noise.event);
    tag.putFloat("distance", noise.distance);
    tag.set("pos", wire::nbt::NbtTag::ofList(
                       wire::nbt::NbtList{wire::nbt::NbtType::Double,
                                          {wire::nbt::NbtTag::ofDouble(noise.x), wire::nbt::NbtTag::ofDouble(noise.y),
                                           wire::nbt::NbtTag::ofDouble(noise.z)}}));
    putUuid(tag, "source", noise.source.entity);
    putUuid(tag, "projectile_owner", noise.source.projectileOwner);
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
    return Noise{.event = std::string{name},
                 .distance = tag->getFloat("distance"),
                 .x = at(0),
                 .y = at(1),
                 .z = at(2),
                 .source =
                     Source{.entity = uuidAt(*tag, "source"), .projectileOwner = uuidAt(*tag, "projectile_owner")}};
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
/// `power` and the frequency are two different numbers: `power` is how close the noise was
/// (`getRedstoneStrengthForDistance`), the frequency is what kind it was and is what a comparator
/// reads. parity: the tag `vibration_resonators`, `minecraft:amethyst_block` and nothing else.
[[nodiscard]] bool resonates(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && block->path() == "amethyst_block";
}

[[nodiscard]] std::optional<BlockChange> activate(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                  std::int32_t stateId, const wire::common::Identifier& block,
                                                  std::int32_t frequency, double distance, std::int64_t gameTime,
                                                  const Source& source) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto phase = properties.find("sculk_sensor_phase");
    if (phase == properties.end() || phase->second != "inactive") {
        return std::nullopt;
    }
    const std::optional<std::int32_t> turned = registry::blockStateDefinitions::stateIdOf(
        block, {{"sculk_sensor_phase", "active"}, {"power", std::to_string(strengthFor(distance, radiusOf(block)))}},
        stateId);
    if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
        return std::nullopt;
    }
    // parity: `SculkSensorBlockEntity.setLastVibrationFrequency`, written under vanilla's own name.
    if (wire::nbt::NbtCompound* data = dataOf(world, x, y, z, block); data != nullptr) {
        data->putInt(kFrequencyKey, frequency);
    }
    world.scheduleBlockTick(x, y, z, block, activeTicksOf(block));
    // parity: `SculkSensorBlock.tryResonateVibration`. An amethyst block touching a sensor that has
    // just fired sends the same frequency out again from its own square, and it does it for **every**
    // side that has one: the loop has no `break`. That is how a sculk farm carries a reading past the
    // eight squares a sensor reaches.
    if (frequency >= 1 && frequency <= redstone::kMaxSignal) {
        const std::string resonance = "resonate_" + std::to_string(frequency);
        for (const registry::BlockFace face : kFaces) {
            const std::int32_t ax = x + registry::stepX(face);
            const std::int32_t ay = y + registry::stepY(face);
            const std::int32_t az = z + registry::stepZ(face);
            if (const std::optional<std::int32_t> beside = world.blockState(ax, ay, az);
                beside.has_value() && resonates(*beside)) {
                emit(world, resonance, ax + 0.5, ay + 0.5, az + 0.5, gameTime, source);
            }
        }
    }
    // parity: `level.gameEvent(sourceEntity, SCULK_SENSOR_TENDRILS_CLICKING, pos)`, from the middle of
    // the square and still carrying who made the first noise. Only a shrieker hears it.
    emit(world, kTendrilsClicking, x + 0.5, y + 0.5, z + 0.5, gameTime, source);
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
    static_cast<void>(world);
    static_cast<void>(x);
    static_cast<void>(y);
    static_cast<void>(z);
    static_cast<void>(block);
}

/// parity: the tag `occludes_vibration_signals`, which is `#minecraft:wool` and nothing else. Every
/// wool block is named for its colour, so the name is the whole test.
[[nodiscard]] bool occludesVibrations(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && block->path().ends_with("_wool");
}

/// parity: `VibrationSystem.Listener.isOccluded`. Six lines from the middle of one square to the
/// middle of the other, each nudged a hundred-thousandth in one of the six directions, blocked only
/// when all six meet wool. They cross the same squares unless the line runs exactly along a face,
/// and that boundary case is the whole reason there are six.
[[nodiscard]] bool occluded(WorldState& world, double x, double y, double z, std::int32_t sensorX, std::int32_t sensorY,
                            std::int32_t sensorZ) {
    const double fromX = std::floor(x) + 0.5;
    const double fromY = std::floor(y) + 0.5;
    const double fromZ = std::floor(z) + 0.5;
    const double toX = static_cast<double>(sensorX) + 0.5;
    const double toY = static_cast<double>(sensorY) + 0.5;
    const double toZ = static_cast<double>(sensorZ) + 0.5;
    // parity: `Vec3.relative(direction, 1.0E-5F)`, a float widened to a double and not `1.0e-5`.
    constexpr double kNudge = static_cast<double>(1.0e-5F);
    for (const registry::BlockFace face : kFaces) {
        const double nudgedX = fromX + (static_cast<double>(registry::stepX(face)) * kNudge);
        const double nudgedY = fromY + (static_cast<double>(registry::stepY(face)) * kNudge);
        const double nudgedZ = fromZ + (static_cast<double>(registry::stepZ(face)) * kNudge);
        if (!entity::blockRay::crosses(world, nudgedX, nudgedY, nudgedZ, toX, toY, toZ, occludesVibrations)) {
            return false;
        }
    }
    return true;
}

void offer(WorldState& world, std::int32_t sensorX, std::int32_t sensorY, std::int32_t sensorZ,
           const wire::common::Identifier& block, std::string_view event, double x, double y, double z,
           std::int64_t gameTime, bool refuseWhileCarrying, const Source& source);

/// parity: `VibrationSystem.Listener.handleGameEvent` over `VibrationSelector.addCandidate`. Nothing
/// lights up here: the sensor writes the noise down and waits its turn.
void hear(WorldState& world, std::int32_t sensorX, std::int32_t sensorY, std::int32_t sensorZ, std::int32_t stateId,
          const wire::common::Identifier& block, std::string_view event, double x, double y, double z,
          std::int64_t gameTime, const Source& source) {
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
    // parity: `CalibratedSculkSensorBlockEntity.VibrationUser.canReceiveVibration`. A calibrated
    // sensor reads the signal on its **back** and, when that is not zero, hears only the noise whose
    // frequency is that number. A zero back leaves it hearing everything, like a plain sensor.
    if (block == calibrated()) {
        const auto named = properties.find("facing");
        const std::optional<registry::BlockFace> facing =
            named == properties.end() ? std::nullopt : faceNamed(named->second);
        if (facing.has_value()) {
            const registry::BlockFace behind = registry::opposite(*facing);
            // The world's name is what `containerOpeners` is keyed by and a vibration never carries
            // one, so a **trapped chest** tuned into the back of a calibrated sensor reads here as
            // if nobody had it open. Everything else that can sit there answers from the block alone.
            const std::int32_t back =
                redstone::signalFrom(world, {}, sensorX + registry::stepX(behind), sensorY + registry::stepY(behind),
                                     sensorZ + registry::stepZ(behind), behind);
            if (back != 0 && frequencyOf(event) != back) {
                return;
            }
        }
    }
    // parity: `handleGameEvent` asks this last of all, after the sensor has said it can hear it.
    if (occluded(world, x, y, z, sensorX, sensorY, sensorZ)) {
        return;
    }
    offer(world, sensorX, sensorY, sensorZ, block, event, x, y, z, gameTime, true, source);
}

/// parity: `handleGameEvent` for a shrieker, whose `canReceiveVibration` asks two things: that it is
/// not already shrieking, and that the noise leads back to a player.
void hearShrieker(WorldState& world, std::int32_t shriekerX, std::int32_t shriekerY, std::int32_t shriekerZ,
                  std::int32_t stateId, double x, double y, double z, std::int64_t gameTime, const Source& source) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    if (const auto shrieking = properties.find("shrieking");
        shrieking == properties.end() || shrieking->second != "false") {
        return;
    }
    if (!source.entity.has_value() || !world.leadsToPlayer(*source.entity)) {
        return;
    }
    if (occluded(world, x, y, z, shriekerX, shriekerY, shriekerZ)) {
        return;
    }
    offer(world, shriekerX, shriekerY, shriekerZ, shrieker(), kTendrilsClicking, x, y, z, gameTime, true, source);
}

/// The write half of `VibrationSelector.addCandidate`. `refuseWhileCarrying` is what tells the two
/// callers apart: `handleGameEvent` gives up when the sensor already has one on the way, and
/// `forceScheduleVibration` (which is what stepping straight on the sensor uses) does not ask.
void offer(WorldState& world, std::int32_t sensorX, std::int32_t sensorY, std::int32_t sensorZ,
           const wire::common::Identifier& block, std::string_view event, double x, double y, double z,
           std::int64_t gameTime, bool refuseWhileCarrying, const Source& source) {
    wire::nbt::NbtCompound* data = dataOf(world, sensorX, sensorY, sensorZ, block);
    if (data == nullptr) {
        return;
    }
    const wire::nbt::NbtCompound* held = data->getCompound(kListenerKey);
    wire::nbt::NbtCompound listener = held == nullptr ? wire::nbt::NbtCompound{} : *held;
    // parity: one already on its way makes the sensor deaf until it lands.
    if (refuseWhileCarrying && listener.getCompound("event") != nullptr) {
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
    selector.set("event",
                 wire::nbt::NbtTag::ofCompound(written(Noise{
                     .event = std::string{event}, .distance = distance, .x = x, .y = y, .z = z, .source = source})));
    selector.putLong("tick", gameTime);
    listener.set("selector", wire::nbt::NbtTag::ofCompound(std::move(selector)));
    data->set(kListenerKey, wire::nbt::NbtTag::ofCompound(std::move(listener)));
    static_cast<void>(block);
}

/// parity: `VibrationSystem.Ticker.tick`: pick what was heard last turn, then move it one block
/// closer, whatever the listener is.
///
/// `mayLand` is `receiveVibration`'s own gate: when it says no, the noise stays written down with
/// its countdown spent and tries again next turn. Answers the noise that landed, if one did.
[[nodiscard]] std::optional<Noise> countDown(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                             std::int64_t gameTime, const std::function<bool()>& mayLand) {
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
        return std::nullopt;
    }
    if (mayLand && !mayLand()) {
        listener.set("event", wire::nbt::NbtTag::ofCompound(written(*current)));
        listener.putInt("event_delay", 0);
        data->set(kListenerKey, wire::nbt::NbtTag::ofCompound(std::move(listener)));
        return std::nullopt;
    }
    listener.remove("event");
    listener.putInt("event_delay", 0);
    data->set(kListenerKey, wire::nbt::NbtTag::ofCompound(std::move(listener)));
    return current;
}

/// A sensor's countdown, which lights it on the turn the noise lands.
[[nodiscard]] std::optional<BlockChange> travel(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                const wire::common::Identifier& block, std::int64_t gameTime) {
    const std::optional<Noise> current = countDown(world, x, y, z, gameTime, {});
    if (!current.has_value()) {
        return std::nullopt;
    }
    // parity: `VibrationSystem.Listener.distanceBetweenInBlocks`. The signal is how far the two
    // **squares** were, which is not the distance the noise travelled.
    const double toX = static_cast<double>(x) - std::floor(current->x);
    const double toY = static_cast<double>(y) - std::floor(current->y);
    const double toZ = static_cast<double>(z) - std::floor(current->z);
    const std::optional<std::int32_t> now = world.blockState(x, y, z);
    if (!now.has_value()) {
        return std::nullopt;
    }
    return activate(world, x, y, z, *now, block, frequencyOf(current->event),
                    std::sqrt((toX * toX) + (toY * toY) + (toZ * toZ)), gameTime, current->source);
}
} // namespace

bool occludedBetween(WorldState& world, double fromX, double fromY, double fromZ, double toX, double toY,
                     double toZ) {
    return occluded(world, fromX, fromY, fromZ, static_cast<std::int32_t>(std::floor(toX)),
                    static_cast<std::int32_t>(std::floor(toY)), static_cast<std::int32_t>(std::floor(toZ)));
}

std::int32_t frequencyOf(std::string_view event) {
    // parity: the table's own tail, `for (int i = 1; i <= 15; i++) map.put(getResonanceEventByFrequency(i), i)`.
    // A resonance event is worth the number in its name, which is what carries the frequency across
    // the amethyst.
    if (event.starts_with("resonate_")) {
        const std::string_view number = event.substr(std::string_view{"resonate_"}.size());
        std::int32_t frequency = 0;
        for (const char digit : number) {
            if (digit < '0' || digit > '9') {
                return 0;
            }
            frequency = (frequency * 10) + (digit - '0');
        }
        return frequency >= 1 && frequency <= redstone::kMaxSignal ? frequency : 0;
    }
    const auto found = frequencies().find(event);
    return found == frequencies().end() ? 0 : found->second;
}

void emit(WorldState& world, std::string_view event, double x, double y, double z, std::int64_t gameTime,
          const Source& source) {
    world.recordGameEvent(GameEventRecord{.event = std::string{event},
                                          .x = x,
                                          .y = y,
                                          .z = z,
                                          .gameTime = gameTime,
                                          .entity = source.entity,
                                          .projectileOwner = source.projectileOwner});
    const auto blockX = static_cast<std::int32_t>(std::floor(x));
    const auto blockY = static_cast<std::int32_t>(std::floor(y));
    const auto blockZ = static_cast<std::int32_t>(std::floor(z));
    // parity: the tags `vibrations` and `shrieker_can_listen` share nothing, so a click only ever
    // reaches shriekers and every other noise only ever reaches sensors.
    if (event == kTendrilsClicking) {
        for (std::int32_t offsetY = -kShriekerRange; offsetY <= kShriekerRange; ++offsetY) {
            for (std::int32_t offsetZ = -kShriekerRange; offsetZ <= kShriekerRange; ++offsetZ) {
                for (std::int32_t offsetX = -kShriekerRange; offsetX <= kShriekerRange; ++offsetX) {
                    if ((offsetX * offsetX) + (offsetY * offsetY) + (offsetZ * offsetZ) >
                        kShriekerRange * kShriekerRange) {
                        continue;
                    }
                    const std::int32_t atX = blockX + offsetX;
                    const std::int32_t atY = blockY + offsetY;
                    const std::int32_t atZ = blockZ + offsetZ;
                    const std::optional<std::int32_t> stateId = world.blockState(atX, atY, atZ);
                    if (!stateId.has_value()) {
                        continue;
                    }
                    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
                    if (block != nullptr && *block == shrieker()) {
                        hearShrieker(world, atX, atY, atZ, *stateId, x, y, z, gameTime, source);
                    }
                }
            }
        }
        return;
    }
    const std::int32_t frequency = frequencyOf(event);
    if (frequency <= 0) {
        return;
    }
    // parity: the calibrated sensor hears twice as far, so the walk is the wider of the two and each
    // sensor answers for its own radius below.
    for (std::int32_t offsetY = -kCalibratedRange; offsetY <= kCalibratedRange; ++offsetY) {
        for (std::int32_t offsetZ = -kCalibratedRange; offsetZ <= kCalibratedRange; ++offsetZ) {
            for (std::int32_t offsetX = -kCalibratedRange; offsetX <= kCalibratedRange; ++offsetX) {
                // parity: `EuclideanGameEventListenerRegistry.getPostableListenerPosition`, which
                // measures `BlockPos.containing(listener).distSqr(BlockPos.containing(source))`
                // against the radius squared: the range runs between the two **blocks** and not
                // between the exact points, and only what is strictly greater falls outside.
                const std::int32_t reach = (offsetX * offsetX) + (offsetY * offsetY) + (offsetZ * offsetZ);
                if (reach > kCalibratedRange * kCalibratedRange) {
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
                const std::int32_t radius = radiusOf(*block);
                if (reach > radius * radius) {
                    continue;
                }
                hear(world, sensorX, sensorY, sensorZ, *stateId, *block, event, x, y, z, gameTime, source);
            }
        }
    }
}

bool stoodOn(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, double entityX, double entityY,
             double entityZ, std::int64_t gameTime, const Source& source) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return false;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr || !isSensor(*block)) {
        return false;
    }
    // parity: `canActivate`, the same inactive-only rule the rest of the sensor lives by.
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto phase = properties.find("sculk_sensor_phase");
    if (phase == properties.end() || phase->second != "inactive") {
        return false;
    }
    offer(world, x, y, z, *block, "step", entityX, entityY, entityZ, gameTime, false, source);
    return true;
}

std::optional<BlockChange> tick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                std::int64_t /*gameTime*/) {
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
    // parity: `SculkSensorBlock.tick`, the turn the block booked for itself. Thirty awake, then ten
    // of cooldown.
    std::optional<BlockChange> settled;
    if (phase == "active" || phase == "cooldown") {
        const std::map<std::string, std::string> wanted =
            phase == "active" ? std::map<std::string, std::string>{{"sculk_sensor_phase", "cooldown"}, {"power", "0"}}
                              : std::map<std::string, std::string>{{"sculk_sensor_phase", "inactive"}, {"power", "0"}};
        if (const std::optional<std::int32_t> turned =
                registry::blockStateDefinitions::stateIdOf(*block, wanted, *stateId);
            turned.has_value() && world.setBlockState(x, y, z, *turned)) {
            settled = BlockChange{.x = x, .y = y, .z = z, .stateId = *turned};
            if (phase == "active") {
                world.scheduleBlockTick(x, y, z, *block, kCooldownTicks);
            } else {
                // A world saved while a noise was travelling comes back with it written down and
                // nothing booked to move it: this is where such a sensor picks the countdown back up.
                wake(world, x, y, z, *block);
            }
        }
    }
    return settled;
}

std::optional<BlockChange> travelTick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                      std::int64_t gameTime) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr || !isSensor(*block)) {
        return std::nullopt;
    }
    return travel(world, x, y, z, *block, gameTime);
}

std::optional<ShriekerArrival> shriekerTravelTick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                  std::int64_t gameTime, const WorldState::Ticking& ticking) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr || *block != shrieker()) {
        return std::nullopt;
    }
    // parity: `requiresAdjacentChunksToBeTicking`, which only the shrieker turns on: the noise waits
    // until the three by three chunks around it are all running.
    const auto neighboursRun = [&world, &ticking, chunkX = x >> 4, chunkZ = z >> 4] {
        for (std::int32_t atX = chunkX - 1; atX <= chunkX + 1; ++atX) {
            for (std::int32_t atZ = chunkZ - 1; atZ <= chunkZ + 1; ++atZ) {
                if (world.chunkIfLoaded(atX, atZ) == nullptr || (ticking && !ticking(atX, atZ))) {
                    return false;
                }
            }
        }
        return true;
    };
    const std::optional<Noise> landed = countDown(world, x, y, z, gameTime, neighboursRun);
    if (!landed.has_value()) {
        return std::nullopt;
    }
    return ShriekerArrival{.x = x, .y = y, .z = z, .source = landed->source};
}

} // namespace basalt::server::runtime::world::vibrations
