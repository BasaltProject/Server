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

/// The signal this event is worth, or zero for one nothing listens to.
[[nodiscard]] std::int32_t frequencyOf(std::string_view event);

/// Tells every sensor in range that something happened. Answers what changed, which the caller sends
/// out and settles the redstone around.
///
/// parity: `VibrationSystem.Listener.handleGameEvent`: the sensor takes the vibration only while it
/// is inactive, and the signal it gives is the event's own frequency.
void emit(WorldState& world, std::string_view event, double x, double y, double z, std::vector<BlockChange>& changes);

/// The scheduled half: an active sensor goes to cooldown and gives up its signal, and one already
/// cooling down goes quiet.
///
/// parity: `SculkSensorBlock.tick` over `deactivate`.
[[nodiscard]] std::optional<BlockChange> tick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// How far a vibration reaches, for the callers that filter before they emit.
[[nodiscard]] bool isInRange(std::int32_t sensorX, std::int32_t sensorY, std::int32_t sensorZ, double x, double y,
                             double z);

} // namespace vibrations

} // namespace basalt::server::runtime::world
