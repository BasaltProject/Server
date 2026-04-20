#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldClock.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {

/// parity: `DaylightDetectorBlock.updateSignalStrength`.
namespace daylightDetectors {

inline constexpr std::int32_t kIntervalTicks = 20;

/// parity: `SKY_LIGHT_LEVEL`.
struct Sky {
    float level = 15.0F;
    bool dayTimeline = true;
};

/// parity: `Level.updateSkyBrightness`.
[[nodiscard]] std::int32_t skyDarken(const WorldClock& clock, const WorldWeather& weather, Sky sky = {});

[[nodiscard]] std::int32_t signalFor(WorldState& world, const WorldClock& clock, std::int32_t darken, std::int32_t x,
                                     std::int32_t y, std::int32_t z, bool inverted, bool hasSkylight = true,
                                     bool sunMoves = true);

[[nodiscard]] std::optional<BlockChange> update(WorldState& world, const WorldClock& clock, std::int32_t darken,
                                                std::int32_t x, std::int32_t y, std::int32_t z, bool hasSkylight = true,
                                                bool sunMoves = true);

} // namespace daylightDetectors

} // namespace basalt::server::runtime::world
