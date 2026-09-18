#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/common/Uuid.hpp>

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// What a sculk sensor hears.
///
/// parity: `VibrationSystem` over `SculkSensorBlock`. Every game event carries a frequency, and that
/// frequency is the signal the sensor gives off. It stays awake thirty ticks and takes ten to
/// settle, which is what stops one alarm covering a whole corridor.
namespace vibrations {

/// parity: `SculkSensorBlock.ACTIVE_TICKS` and `COOLDOWN_TICKS`.
inline constexpr std::int32_t kActiveTicks = 30;
inline constexpr std::int32_t kCooldownTicks = 10;

/// parity: `CalibratedSculkSensorBlock.getActiveTicks`, which is a third of the plain sensor's: the
/// calibrated one hears twice as far and lets go three times as fast.
inline constexpr std::int32_t kCalibratedActiveTicks = 10;

/// parity: `SculkSensorBlockEntity.last_vibration_frequency`, what kind of noise it last heard. It
/// is not the same number as the state's `power`, which says how close the noise was, and a
/// comparator reads this one.
inline constexpr std::string_view kFrequencyKey = "last_vibration_frequency";

/// parity: `SculkSensorBlockEntity`'s own listener range.
inline constexpr std::int32_t kRange = 8;

/// parity: `CalibratedSculkSensorBlockEntity.VibrationUser.getListenerRadius`, which is twice the
/// plain sensor's. It decides both how far it hears and how strong what it hears comes out.
inline constexpr std::int32_t kCalibratedRange = 16;

/// parity: `VibrationSystem.Data.NBT_TAG_KEY`, where the sensor keeps the noise it is still waiting
/// for. A vibration is not heard where it happens: it travels one block per tick.
inline constexpr std::string_view kListenerKey = "listener";

/// parity: `SculkShriekerBlockEntity.VibrationUser.LISTENER_RADIUS`.
inline constexpr std::int32_t kShriekerRange = 8;

/// parity: `GameEvent.SCULK_SENSOR_TENDRILS_CLICKING`, what a sensor raises when it fires. Only the
/// tag `shrieker_can_listen` holds it, so no sensor ever hears another one click.
inline constexpr std::string_view kTendrilsClicking = "sculk_sensor_tendrils_clicking";

/// Who made the noise.
///
/// parity: `VibrationInfo`'s `source` and `projectile_owner`, kept by uuid because the entity may
/// be gone by the time the noise lands.
struct Source {
    std::optional<wire::common::Uuid> entity;
    std::optional<wire::common::Uuid> projectileOwner;
};

/// A noise that reached a shrieker, and who made it.
struct ShriekerArrival {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    Source source;
};

/// The signal this event is worth, or zero for one nothing listens to.
[[nodiscard]] std::int32_t frequencyOf(std::string_view event);

/// Tells every sensor in range that something happened. Nothing lights up here: the sensor only
/// writes the noise down and books itself a turn, because a vibration takes a tick per block to
/// arrive.
///
/// parity: `VibrationSystem.Listener.handleGameEvent`, which adds a candidate and returns. The
/// sensor takes it only while it is inactive and has nothing already on the way.
void emit(WorldState& world, std::string_view event, double x, double y, double z, std::int64_t gameTime,
          const Source& source = {});

/// parity: `SculkSensorBlock.stepOn` over `Listener.forceScheduleVibration`: standing straight on a
/// sensor puts a step into it **without** asking whether one is already on the way and without the
/// wool check, which is the difference between walking past one and walking on it.
///
/// Answers whether the sensor took it.
bool stoodOn(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, double entityX, double entityY,
             double entityZ, std::int64_t gameTime, const Source& source = {});

/// The sensor's booked turn: an active sensor goes to cooldown and one cooling down goes quiet.
///
/// parity: `SculkSensorBlock.tick` over `deactivate`.
[[nodiscard]] std::optional<BlockChange> tick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                              std::int64_t gameTime);

/// The other half, which is the block entity's and books nothing: a noise on its way moves one block
/// closer every turn of the world, and lights the sensor on the turn it arrives.
///
/// parity: `VibrationSystem.Ticker.tick`, which runs from `SculkSensorBlockEntity`'s ticker and not
/// from a booked turn. That is why the jar's chunk carries no pending tick while a noise travels.
[[nodiscard]] std::optional<BlockChange> travelTick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                    std::int64_t gameTime);

/// The same countdown for a shrieker; the arrival goes back to the caller, who decides whether it
/// shrieks. `ticking` says which chunks run, unset means all. parity: `VibrationSystem.Ticker.tick`.
[[nodiscard]] std::optional<ShriekerArrival> shriekerTravelTick(WorldState& world, std::int32_t x, std::int32_t y,
                                                                std::int32_t z, std::int64_t gameTime,
                                                                const WorldState::Ticking& ticking = {});

} // namespace vibrations

} // namespace basalt::server::runtime::world
