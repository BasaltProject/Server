#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/DaylightDetectors.hpp>
#include <basalt/server/runtime/world/WorldLight.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <map>
#include <numbers>
#include <string>

namespace basalt::server::runtime::world::daylightDetectors {
namespace {

constexpr std::int32_t kMax = 15;
constexpr std::int64_t kDayTicks = 24000;
constexpr float kDegrees = 360.0F;

/// parity: `updateSkyBrightness`, which in 26.2 is `15 - EnvironmentAttributes.SKY_LIGHT_LEVEL` and is
/// no longer written as a formula: it comes off a track with keyframes at 133, 11867, 13670 and 22330.
/// The sum here lands on the same numbers at both ends, and at night both sides answer eleven,
/// checked against the jar. Rain and thunder each take five sixteenths off the daylight.
constexpr double kRainShare = 1.0 - (5.0 / 16.0);
constexpr double kDarkenRange = 11.0;

/// The curved fraction of the day everything else reads.
///
/// parity: this was `DimensionType.timeOfDay` until 26.2, which moved it to a **timeline track**:
/// `EnvironmentAttributes.SUN_ANGLE`, eased with `symmetricCubicBezier(0.362, 0.241)` over keyframes
/// running 360 to 0 degrees on a 24000 period. That Bezier all but reproduces the blend here: walked
/// against each other over a whole day, a detector's power differs on 86 ticks out of 24000 and never
/// by more than one. Measured against the jar too, in `detectorluz`, which reads noon and midnight:
/// both sides answer the same.
[[nodiscard]] float timeOfDay(const WorldClock& clock) {
    const float dayTime = static_cast<float>(clock.totalTicks() % kDayTicks) / static_cast<float>(kDayTicks);
    float fraction = dayTime - 0.25F;
    if (fraction < 0.0F) {
        fraction += 1.0F;
    }
    return fraction + (1.0F - ((std::cos(fraction * std::numbers::pi_v<float>) + 1.0F) / 2.0F) - fraction) / 3.0F;
}

/// parity: `Level.getEffectiveSkyBrightness`, the sky light above the block minus what the weather
/// takes off it.
[[nodiscard]] std::int32_t skyBrightness(WorldState& world, const WorldClock& clock, const WorldWeather& weather,
                                         std::int32_t x, std::int32_t y, std::int32_t z, bool hasSkylight) {
    return std::clamp(worldLight::skyLight(world, x, y + 1, z, hasSkylight) - skyDarken(clock, weather), 0, kMax);
}

} // namespace

std::int32_t skyDarken(const WorldClock& clock, const WorldWeather& weather) {
    const double rain = weather.raining ? kRainShare : 1.0;
    const double thunder = weather.thundering ? kRainShare : 1.0;
    // The fraction is worked out in float, as vanilla does, and widened once here rather than by
    // the multiplication: an implicit promotion is what the build refuses.
    const double fraction = static_cast<double>(timeOfDay(clock));
    const double curve = 0.5 + (2.0 * std::clamp(std::cos(fraction * 2.0 * std::numbers::pi), -0.25, 0.25));
    return static_cast<std::int32_t>((1.0 - (curve * rain * thunder)) * kDarkenRange);
}

std::int32_t signalFor(WorldState& world, const WorldClock& clock, const WorldWeather& weather, std::int32_t x,
                       std::int32_t y, std::int32_t z, bool inverted, bool hasSkylight) {
    std::int32_t target = skyBrightness(world, clock, weather, x, y, z, hasSkylight);
    if (inverted) {
        target = kMax - target;
    } else if (target > 0) {
        float angle = timeOfDay(clock) * kDegrees * (std::numbers::pi_v<float> / 180.0F);
        const float offset = angle < std::numbers::pi_v<float> ? 0.0F : std::numbers::pi_v<float> * 2.0F;
        angle += (offset - angle) * 0.2F;
        target = static_cast<std::int32_t>(std::lround(static_cast<float>(target) * std::cos(angle)));
    }
    return std::clamp(target, 0, kMax);
}

std::optional<BlockChange> update(WorldState& world, const WorldClock& clock, const WorldWeather& weather,
                                  std::int32_t x, std::int32_t y, std::int32_t z, bool hasSkylight) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr || block->path() != "daylight_detector") {
        return std::nullopt;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto invertedAt = properties.find("inverted");
    const std::int32_t wanted = signalFor(world, clock, weather, x, y, z,
                                          invertedAt != properties.end() && invertedAt->second == "true", hasSkylight);

    std::int32_t showing = 0;
    if (const auto powerAt = properties.find("power"); powerAt != properties.end()) {
        const std::string& text = powerAt->second;
        std::int32_t parsed = 0;
        const std::from_chars_result result = std::from_chars(text.data(), text.data() + text.size(), parsed);
        if (result.ec == std::errc{} && result.ptr == text.data() + text.size()) {
            showing = parsed;
        }
    }
    if (showing == wanted) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> turned =
        registry::blockStateDefinitions::stateIdOf(*block, {{"power", std::to_string(wanted)}}, *stateId);
    if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
        return std::nullopt;
    }
    return BlockChange{.x = x, .y = y, .z = z, .stateId = *turned};
}

} // namespace basalt::server::runtime::world::daylightDetectors
