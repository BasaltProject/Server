#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/common/Uuid.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `VibrationSystem`.
namespace vibrations {

inline constexpr std::int32_t kActiveTicks = 30;
inline constexpr std::int32_t kCooldownTicks = 10;

/// parity: `CalibratedSculkSensorBlock.getActiveTicks`.
inline constexpr std::int32_t kCalibratedActiveTicks = 10;

/// parity: `SculkSensorBlockEntity.last_vibration_frequency`.
inline constexpr std::string_view kFrequencyKey = "last_vibration_frequency";

inline constexpr std::int32_t kRange = 8;

/// parity: `CalibratedSculkSensorBlockEntity.VibrationUser.getListenerRadius`.
inline constexpr std::int32_t kCalibratedRange = 16;

/// parity: `VibrationSystem.Data.NBT_TAG_KEY`.
inline constexpr std::string_view kListenerKey = "listener";

inline constexpr std::int32_t kShriekerRange = 8;

/// parity: `GameEvent.SCULK_SENSOR_TENDRILS_CLICKING`.
inline constexpr std::string_view kTendrilsClicking = "sculk_sensor_tendrils_clicking";

/// parity: `VibrationInfo`.
struct Source {
    std::optional<wire::common::Uuid> entity;
    std::optional<wire::common::Uuid> projectileOwner;
    /// parity: `Entity.isSteppingCarefully`.
    bool steppingCarefully = false;
    /// parity: `CriteriaTriggers.AVOID_VIBRATION`.
    std::function<void()> avoided;
};

/// parity: `GameEventTags.IGNORE_VIBRATIONS_SNEAKING`.
[[nodiscard]] bool ignoredWhenSneaking(std::string_view event);

struct ShriekerArrival {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    Source source;
};

/// parity: `VibrationSystem.Listener.isOccluded`.
[[nodiscard]] bool occludedBetween(WorldState& world, double fromX, double fromY, double fromZ, double toX, double toY,
                                   double toZ);

[[nodiscard]] std::int32_t frequencyOf(std::string_view event);

/// parity: `Listener.handleGameEvent`.
void emit(WorldState& world, std::string_view event, double x, double y, double z, std::int64_t gameTime,
          const Source& source = {});

/// parity: `SculkSensorBlock.stepOn`.
bool stoodOn(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, double entityX, double entityY,
             double entityZ, std::int64_t gameTime, const Source& source = {});

/// parity: `SculkSensorBlock.tick` over `deactivate`.
[[nodiscard]] std::optional<BlockChange> tick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                              std::int64_t gameTime);

/// parity: `VibrationSystem.Ticker.tick`.
[[nodiscard]] std::optional<BlockChange> travelTick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                    std::int64_t gameTime);

/// `ticking` says which chunks run, unset means all. parity: `VibrationSystem.Ticker.tick`.
[[nodiscard]] std::optional<ShriekerArrival> shriekerTravelTick(WorldState& world, std::int32_t x, std::int32_t y,
                                                                std::int32_t z, std::int64_t gameTime,
                                                                const WorldState::Ticking& ticking = {});

} // namespace vibrations

} // namespace basalt::server::runtime::world
