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

/// parity: `WeatherAttributes.RAIN` and `THUNDER` modify `SKY_LIGHT_LEVEL` with
/// `FloatModifier.ALPHA_BLEND`, which is `lerp(alpha, value, 4.0)`. They do **not** multiply: 26.2
/// moved the old five sixteenths into an alpha that lands on the same number in full daylight and on
/// a different one everywhere else, because the blend has a floor and the product does not.
constexpr float kNightSkyLightLevel = 4.0F;
constexpr float kRainAlpha = 0.3125F;
constexpr float kThunderAlpha = 0.52734375F;

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

/// parity: `EasingType.CubicBezier.solveT`, Newton-Raphson with a bounded step and a bisection behind it.
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

/// The share of the full sky light the time of day allows. parity: 26.2 put this on a
/// `FloatModifier.MULTIPLY` track with four keyframes and no easing, so it is linear. `bakeSegments`
/// wraps the ends, leaving two ramps of 1803 ticks: one down from 11867 and one up from 22330
/// through midnight. Fifteen times 0.26666668 is the four of `NIGHT_SKY_LIGHT_LEVEL`.
constexpr float kNightShare = 0.26666668F;
constexpr std::int64_t kDuskStart = 11867;
constexpr std::int64_t kDuskEnd = 13670;
constexpr std::int64_t kDawnStart = 22330;
constexpr std::int64_t kDawnEnd = 133;
constexpr float kRampTicks = 1803.0F;

[[nodiscard]] float skyLightShare(const WorldClock& clock) {
    const std::int64_t looped = ((clock.totalTicks() % kDayTicks) + kDayTicks) % kDayTicks;
    if (looped < kDawnEnd) {
        // The tail of the ramp that started at 22330 and crosses midnight.
        const float alpha = static_cast<float>(looped + (kDayTicks - kDawnStart)) / kRampTicks;
        return wire::common::mth::lerp(alpha, kNightShare, 1.0F);
    }
    if (looped < kDuskStart) {
        return 1.0F;
    }
    if (looped < kDuskEnd) {
        const float alpha = static_cast<float>(looped - kDuskStart) / kRampTicks;
        return wire::common::mth::lerp(alpha, 1.0F, kNightShare);
    }
    if (looped < kDawnStart) {
        return kNightShare;
    }
    const float alpha = static_cast<float>(looped - kDawnStart) / kRampTicks;
    return wire::common::mth::lerp(alpha, kNightShare, 1.0F);
}

/// parity: `Math.round(float)`, which is `floor(x + 0.5)` and not the away-from-zero rounding of
/// `lround`: the two differ on every negative half.
[[nodiscard]] std::int32_t roundLikeJava(float value) {
    return static_cast<std::int32_t>(std::floor(static_cast<double>(value) + 0.5));
}

/// parity: `LevelReader.getEffectiveSkyBrightness`, the sky light above the block minus what the weather
/// takes off it.
[[nodiscard]] std::int32_t skyBrightness(WorldState& world, const WorldClock& clock, const WorldWeather& weather,
                                         std::int32_t x, std::int32_t y, std::int32_t z, bool hasSkylight) {
    return std::clamp(worldLight::skyLight(world, x, y + 1, z, hasSkylight) - skyDarken(clock, weather), 0, kMax);
}

} // namespace

std::int32_t skyDarken(const WorldClock& clock, const WorldWeather& weather) {
    float value = static_cast<float>(kMax) * skyLightShare(clock);
    // parity: `WeatherAttributes.addLayer` takes the thunder level **off** the rain level before
    // blending, so a full thunderstorm skips the rain layer altogether instead of stacking on it.
    const float thunder = weather.thunderLevel();
    const float rain = weather.rainLevel() - thunder;
    if (rain > 0.0F) {
        value = wire::common::mth::lerp(rain, value, wire::common::mth::lerp(kRainAlpha, value, kNightSkyLightLevel));
    }
    if (thunder > 0.0F) {
        value =
            wire::common::mth::lerp(thunder, value, wire::common::mth::lerp(kThunderAlpha, value, kNightSkyLightLevel));
    }
    // parity: `Level.updateSkyBrightness` writes
    // `skyDarken = (int)(15.0F - environmentAttributes().getDimensionValue(SKY_LIGHT_LEVEL))`, and
    // that `(int)` truncates towards zero.
    return static_cast<std::int32_t>(static_cast<float>(kMax) - value);
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
