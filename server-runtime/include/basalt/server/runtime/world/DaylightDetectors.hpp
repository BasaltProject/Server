#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldClock.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {

/// The block that reads the sky.
///
/// parity: `DaylightDetectorBlock.updateSignalStrength`: the sky light where it stands, bent by the
/// angle of the sun so it peaks at noon rather than following the raw light. Inverted, it answers
/// fifteen minus that and skips the curve, which is why an inverted one is a clean night switch.
namespace daylightDetectors {

/// How often vanilla lets one look, which is once every twenty ticks in its block-entity ticker.
inline constexpr std::int32_t kIntervalTicks = 20;

/// How much of the sky light the time of day and the weather take away.
///
/// parity: `Level.updateSkyBrightness`, which is what makes a detector read zero at night rather
/// than the fifteen the sky light itself still says.
[[nodiscard]] std::int32_t skyDarken(const WorldClock& clock, const WorldWeather& weather);

/// The power the detector at these coordinates should be showing.
///
/// parity: `Level.getSunAngle`, which is what bends the reading: the light is at fifteen for hours
/// around noon, and the detector is not.
[[nodiscard]] std::int32_t signalFor(WorldState& world, const WorldClock& clock, const WorldWeather& weather,
                                     std::int32_t x, std::int32_t y, std::int32_t z, bool inverted,
                                     bool hasSkylight = true);

/// Updates the block if its reading moved, answering the change.
[[nodiscard]] std::optional<BlockChange> update(WorldState& world, const WorldClock& clock, const WorldWeather& weather,
                                                std::int32_t x, std::int32_t y, std::int32_t z,
                                                bool hasSkylight = true);

} // namespace daylightDetectors

} // namespace basalt::server::runtime::world
