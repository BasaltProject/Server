#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/DaylightDetectors.hpp>
#include <basalt/server/runtime/world/WorldLight.hpp>
#include <basalt/wire/common/Mth.hpp>

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
constexpr std::int64_t kNoon = 6000;
constexpr float kDegrees = 360.0F;

/// parity: `updateSkyBrightness`, which in 26.2 is `15 - EnvironmentAttributes.SKY_LIGHT_LEVEL` and is
/// no longer written as a formula: it comes off a track with keyframes at 133, 11867, 13670 and 22330.
/// The sum here lands on the same numbers at both ends, and at night both sides answer eleven,
/// checked against the jar. Rain and thunder each take five sixteenths off the daylight.
constexpr double kRainShare = 1.0 - (5.0 / 16.0);
constexpr double kDarkenRange = 11.0;

/// The curved fraction of the day everything else reads. parity: 26.2 moved this off
/// `DimensionType.timeOfDay` onto the `SUN_ANGLE` timeline track, eased with
/// `symmetricCubicBezier(0.362, 0.241)`. Walked against each other over a day, a detector's power
/// differs on 86 ticks out of 24000 and never by more than one.
[[nodiscard]] float timeOfDay(const WorldClock& clock) {
    const float dayTime = static_cast<float>(clock.totalTicks() % kDayTicks) / static_cast<float>(kDayTicks);
    float fraction = dayTime - 0.25F;
    if (fraction < 0.0F) {
        fraction += 1.0F;
    }
    return fraction + (1.0F - ((std::cos(fraction * std::numbers::pi_v<float>) + 1.0F) / 2.0F) - fraction) / 3.0F;
}

/// One of the two cubic curves a `EasingType.CubicBezier` is made of.
struct CubicCurve {
    float a;
    float b;
    float c;
};

/// parity: `EasingType.CubicBezier.curveFromControls`.
[[nodiscard]] constexpr CubicCurve curveFrom(float v1, float v2) {
    return CubicCurve{.a = 3.0F * v1 - 3.0F * v2 + 1.0F, .b = -6.0F * v1 + 3.0F * v2, .c = 3.0F * v1};
}

[[nodiscard]] constexpr float sampleCurve(const CubicCurve& curve, float t) {
    return ((curve.a * t + curve.b) * t + curve.c) * t;
}

[[nodiscard]] constexpr float gradientOf(const CubicCurve& curve, float t) {
    return (3.0F * curve.a * t + 2.0F * curve.b) * t + curve.c;
}

/// parity: `symmetricCubicBezier(0.362, 0.241)`, which is `cubicBezier(x1, y1, 1 - x1, 1 - y1)`, the
/// easing every angle track in `Timelines.bootstrap` is built with.
constexpr float kSkyEaseX = 0.362F;
constexpr float kSkyEaseY = 0.241F;
constexpr CubicCurve kSkyEaseXCurve = curveFrom(kSkyEaseX, 1.0F - kSkyEaseX);
constexpr CubicCurve kSkyEaseYCurve = curveFrom(kSkyEaseY, 1.0F - kSkyEaseY);
constexpr float kEaseTolerance = 1.0e-5F;
constexpr float kEaseMaxStep = 0.25F;
constexpr std::int32_t kEaseIterations = 4;

/// parity: `CubicBezier.solveT`, Newton-Raphson with a bounded step and a bisection behind it.
[[nodiscard]] float solveEaseT(float x) {
    float t = x;
    for (std::int32_t attempt = 0; attempt < kEaseIterations; ++attempt) {
        const float error = sampleCurve(kSkyEaseXCurve, t) - x;
        if (std::abs(error) < kEaseTolerance) {
            return t;
        }
        const float gradient = gradientOf(kSkyEaseXCurve, t);
        if (gradient < kEaseTolerance) {
            break;
        }
        t -= std::clamp(error / gradient, -kEaseMaxStep, kEaseMaxStep);
    }
    // parity: `solveTBisect` keeps the t it was handed and only halves after the first look.
    float low = 0.0F;
    float high = 1.0F;
    for (; low < high; t = (high + low) / 2.0F) {
        const float error = sampleCurve(kSkyEaseXCurve, t) - x;
        if (std::abs(error) < kEaseTolerance) {
            return t;
        }
        if (error < 0.0F) {
            low = t;
        } else {
            high = t;
        }
    }
    return t;
}

[[nodiscard]] float skyEase(float x) { return sampleCurve(kSkyEaseYCurve, solveEaseT(x)); }

/// The sun's angle in degrees. parity: 26.2 put this on the `SUN_ANGLE` timeline track, two
/// keyframes at the same tick over a 24000 period. `bakeSegments` turns them into one segment from
/// noon to noon a day later, so the whole thing collapses to easing the fraction of the day.
[[nodiscard]] float sunAngleDegrees(const WorldClock& clock) {
    const std::int64_t looped = ((clock.totalTicks() % kDayTicks) + kDayTicks) % kDayTicks;
    const std::int64_t from = looped < kNoon ? kNoon - kDayTicks : kNoon;
    const float alpha = static_cast<float>(looped - from) / static_cast<float>(kDayTicks);
    return skyEase(alpha) * kDegrees;
}

/// parity: `Math.round(float)`, which is `floor(x + 0.5)` and not the away-from-zero rounding of
/// `lround`: the two differ on every negative half.
[[nodiscard]] std::int32_t roundLikeJava(float value) {
    return static_cast<std::int32_t>(std::floor(static_cast<double>(value) + 0.5));
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
        // parity: `DaylightDetectorBlock.updateSignalStrength` reads `SUN_ANGLE` off the track and
        // turns it into radians, then pulls it towards zero or towards a full turn by a fifth.
        float angle = sunAngleDegrees(clock) * (std::numbers::pi_v<float> / 180.0F);
        const float offset = angle < std::numbers::pi_v<float> ? 0.0F : std::numbers::pi_v<float> * 2.0F;
        angle += (offset - angle) * 0.2F;
        // parity: `Mth.cos`, the 65536-entry table, not the platform's cosine.
        target = roundLikeJava(static_cast<float>(target) * wire::common::mth::cos(angle));
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
