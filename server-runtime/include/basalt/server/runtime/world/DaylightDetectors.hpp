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

/// What the sky over a world is made of, which tells the three dimensions apart. parity:
/// `SKY_LIGHT_LEVEL` starts at the dimension's constant and only the `minecraft:day` timeline bends
/// it, tagged `#minecraft:in_overworld` alone: the Nether sits at four and the End at fifteen.
struct Sky {
    float level = 15.0F;
    bool dayTimeline = true;
};

/// How much of the sky light the time of day and the weather take away.
///
/// parity: `Level.updateSkyBrightness`, which is what makes a detector read zero at night rather
/// than the fifteen the sky light itself still says.
[[nodiscard]] std::int32_t skyDarken(const WorldClock& clock, const WorldWeather& weather, Sky sky = {});

/// The power the detector at these coordinates should be showing. parity: `Level.getSunAngle` bends
/// the reading, so the light sits at fifteen for hours around noon and the detector does not.
///
/// `darken` is the world's stored reading, written once per tick before the clock moves, while the
/// sun angle is read live. Where `sunMoves` is absent the angle defaults to zero and nothing bends.
[[nodiscard]] std::int32_t signalFor(WorldState& world, const WorldClock& clock, std::int32_t darken, std::int32_t x,
                                     std::int32_t y, std::int32_t z, bool inverted, bool hasSkylight = true,
                                     bool sunMoves = true);

/// Updates the block if its reading moved, answering the change.
[[nodiscard]] std::optional<BlockChange> update(WorldState& world, const WorldClock& clock, std::int32_t darken,
                                                std::int32_t x, std::int32_t y, std::int32_t z, bool hasSkylight = true,
                                                bool sunMoves = true);

} // namespace daylightDetectors

} // namespace basalt::server::runtime::world
