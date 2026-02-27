#include <basalt/server/runtime/world/WorldClock.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace basalt::server::runtime::world {
namespace {

constexpr std::array<std::pair<std::string_view, std::int32_t>, 4> kTimeMarkers{
    {{"day", 1000}, {"noon", 6000}, {"night", 13000}, {"midnight", 18000}}};

} // namespace

bool WorldClock::tick(bool advanceTime) {
    if (paused_ || !advanceTime) {
        return false;
    }
    partialTick_ += rate_;
    const std::int32_t whole = static_cast<std::int32_t>(std::floor(partialTick_));
    if (whole == 0) {
        return false;
    }
    partialTick_ -= static_cast<float>(whole);
    totalTicks_ += whole;
    return true;
}

void WorldClock::setTotalTicks(std::int64_t ticks) {
    totalTicks_ = ticks;
    partialTick_ = 0.0F;
}

void WorldClock::addTicks(std::int32_t ticks) { totalTicks_ = std::max(totalTicks_ + ticks, std::int64_t{0}); }

float WorldClock::networkRate(bool advanceTime) const noexcept { return paused_ || !advanceTime ? 0.0F : rate_; }

std::span<const std::pair<std::string_view, std::int32_t>> WorldClock::timeMarkers() { return kTimeMarkers; }

std::optional<std::int32_t> WorldClock::markerTicks(std::string_view name) {
    for (const auto& [marker, ticks] : kTimeMarkers) {
        if (marker == name) {
            return ticks;
        }
    }
    return std::nullopt;
}

std::int64_t WorldClock::timeOfNextMarker(std::int64_t from, std::int32_t markerTicks) {
    // The floored remainder, not the truncated one: a clock set back before the epoch still asks
    // for the next morning, not the previous one.
    const std::int64_t within = ((from % kDayLength) + kDayLength) % kDayLength;
    const std::int64_t delta = markerTicks - within;
    return from + (delta > 0 ? delta : kDayLength + delta);
}

void WorldWeather::tick(random::RandomSource& random, const bool advanceCycle) {
    if (advanceCycle) {
        if (clearWeatherTime > 0) {
            --clearWeatherTime;
            // parity: the counters are left at a flip that is one tick away, so the cycle resumes
            // the moment the clear spell ends rather than drawing a fresh delay.
            thunderTime = thundering ? 0 : 1;
            rainTime = raining ? 0 : 1;
            thundering = false;
            raining = false;
        } else {
            if (thunderTime > 0) {
                if (--thunderTime == 0) {
                    thundering = !thundering;
                }
            } else if (thundering) {
                thunderTime = randomDuration(kThunderDuration, random);
            } else {
                thunderTime = randomDuration(kThunderDelay, random);
            }

            if (rainTime > 0) {
                if (--rainTime == 0) {
                    raining = !raining;
                }
            } else if (raining) {
                rainTime = randomDuration(kRainDuration, random);
            } else {
                rainTime = randomDuration(kRainDelay, random);
            }
        }
    }
    // parity: a hundredth per tick towards the flag, thunder first, both clamped to [0,1]. This is
    // outside the game rule on purpose: the jar ramps even with the cycle frozen.
    thunderLevel_ = std::clamp(thunderLevel_ + (thundering ? kLevelStep : -kLevelStep), 0.0F, 1.0F);
    rainLevel_ = std::clamp(rainLevel_ + (raining ? kLevelStep : -kLevelStep), 0.0F, 1.0F);
}

void WorldWeather::set(std::int32_t clearTime, std::int32_t weatherTime, bool nowRaining, bool nowThundering) {
    clearWeatherTime = clearTime;
    rainTime = weatherTime;
    thunderTime = weatherTime;
    raining = nowRaining;
    thundering = nowThundering;
}

std::int32_t WorldWeather::randomDuration(std::pair<std::int32_t, std::int32_t> range, random::RandomSource& random) {
    // parity: `UniformInt.sample`, which is `Mth.randomBetweenInclusive`.
    return random.nextIntBetweenInclusive(range.first, range.second);
}

} // namespace basalt::server::runtime::world
