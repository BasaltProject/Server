#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

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

/// parity: `SculkSensorBlockEntity.last_vibration_frequency`, what kind of noise it last heard. It
/// is not the same number as the state's `power`, which says how close the noise was, and a
/// comparator reads this one.
inline constexpr std::string_view kFrequencyKey = "last_vibration_frequency";

/// parity: `SculkSensorBlockEntity`'s own listener range.
inline constexpr std::int32_t kRange = 8;

/// parity: `VibrationSystem.Data.NBT_TAG_KEY`, where the sensor keeps the noise it is still waiting
/// for. A vibration is not heard where it happens: it travels one block per tick.
inline constexpr std::string_view kListenerKey = "listener";

/// The signal this event is worth, or zero for one nothing listens to.
[[nodiscard]] std::int32_t frequencyOf(std::string_view event);

/// Tells every sensor in range that something happened. Nothing lights up here: the sensor only
/// writes the noise down and books itself a turn, because a vibration takes a tick per block to
/// arrive.
///
/// parity: `VibrationSystem.Listener.handleGameEvent`, which adds a candidate and returns. The
/// sensor takes it only while it is inactive and has nothing already on the way.
void emit(WorldState& world, std::string_view event, double x, double y, double z, std::int64_t gameTime);

/// parity: `SculkSensorBlock.stepOn` over `Listener.forceScheduleVibration`: standing straight on a
/// sensor puts a step into it **without** asking whether one is already on the way and without the
/// wool check, which is the difference between walking past one and walking on it.
///
/// Answers whether the sensor took it.
bool stoodOn(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, double entityX, double entityY,
             double entityZ, std::int64_t gameTime);

/// The scheduled half, and the sensor's whole clock: an active sensor goes to cooldown, one cooling
/// down goes quiet, and an inactive one moves whatever vibration is travelling towards it.
///
/// parity: `SculkSensorBlock.tick` over `deactivate`, plus `VibrationSystem.Ticker.tick`.
[[nodiscard]] std::optional<BlockChange> tick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                              std::int64_t gameTime);

} // namespace vibrations

} // namespace basalt::server::runtime::world
