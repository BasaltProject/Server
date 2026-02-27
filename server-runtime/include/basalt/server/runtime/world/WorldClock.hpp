#pragma once

#include <basalt/server/random/RandomSource.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

namespace basalt::server::runtime::world {

/// parity: `ServerClockManager.ClockInstance`.
///
/// 26.2 replaced "day time plus game time" with clocks that carry a rate: a tick adds `rate` to a
/// fractional accumulator and only whole units move `totalTicks`, which is what makes
/// `/time rate 0.5` run half-speed days rather than skipping every other tick.
class WorldClock {
public:
    /// parity: the `day` timeline's period, which is what a marker's ticks are taken modulo.
    static constexpr std::int32_t kDayLength = 24000;

    WorldClock() = default;

    WorldClock(std::int64_t totalTicks, float rate, bool paused)
        : totalTicks_(totalTicks), rate_(rate), paused_(paused) {}

    [[nodiscard]] std::int64_t totalTicks() const noexcept { return totalTicks_; }

    [[nodiscard]] float rate() const noexcept { return rate_; }

    void setRate(float rate) noexcept { rate_ = rate; }

    [[nodiscard]] bool paused() const noexcept { return paused_; }

    void setPaused(bool paused) noexcept { paused_ = paused; }

    [[nodiscard]] float partialTick() const noexcept { return partialTick_; }

    /// Advances one tick, answering whether `totalTicks` moved so a caller can skip a pointless
    /// sync.
    bool tick(bool advanceTime);

    void setTotalTicks(std::int64_t ticks);

    /// parity: `ServerClockManager.addTicks`, which clamps at zero rather than going negative.
    void addTicks(std::int32_t ticks);

    /// What the client is told: zero rate when paused, so it stops interpolating instead of
    /// drifting.
    [[nodiscard]] float networkRate(bool advanceTime) const noexcept;

    /// parity: the four `show_in_commands` markers of the `minecraft:day` timeline.
    [[nodiscard]] static std::span<const std::pair<std::string_view, std::int32_t>> timeMarkers();

    [[nodiscard]] static std::optional<std::int32_t> markerTicks(std::string_view name);

    /// parity: `ClockTimeMarker.resolveTimeToMoveTo` for a marker with a period.
    [[nodiscard]] static std::int64_t timeOfNextMarker(std::int64_t from, std::int32_t markerTicks);

private:
    std::int64_t totalTicks_ = 0;
    float rate_ = 1.0F;
    bool paused_ = false;
    float partialTick_ = 0.0F;
};

/// Rain and thunder for one world.
///
/// parity: `ServerLevel.advanceWeatherCycle`, counters included. The three countdowns are what make
/// the cycle work: `clearWeatherTime` forces clear weather while it runs (that is what
/// `/weather clear <duration>` sets), and outside it `rainTime` and `thunderTime` each count down
/// to a *flip* rather than to a state, which is why a fresh world with every counter at zero starts
/// by drawing a delay instead of raining on its first tick.
class WorldWeather {
public:
    /// parity: `ServerLevel.RAIN_DELAY`, `RAIN_DURATION`, `THUNDER_DELAY`, `THUNDER_DURATION`, as
    /// inclusive ranges.
    static constexpr std::pair<std::int32_t, std::int32_t> kRainDelay{12000, 180000};
    static constexpr std::pair<std::int32_t, std::int32_t> kRainDuration{12000, 24000};
    static constexpr std::pair<std::int32_t, std::int32_t> kThunderDelay{12000, 180000};
    static constexpr std::pair<std::int32_t, std::int32_t> kThunderDuration{3600, 15600};

    bool raining = false;
    std::int32_t rainTime = 0;
    bool thundering = false;
    std::int32_t thunderTime = 0;
    std::int32_t clearWeatherTime = 0;

    /// parity: `ServerLevel.advanceWeatherCycle`. The counters only move with the `advanceWeather`
    /// game rule; the two levels ramp either way, because they chase the flags and not the clock.
    void tick(random::RandomSource& random, bool advanceCycle);

    /// parity: `MinecraftServer.setWeatherParameters`, which `/weather` calls with these four.
    void set(std::int32_t clearTime, std::int32_t weatherTime, bool raining, bool thundering);

    /// A duration for a `/weather` that was given none, from the same ranges vanilla samples.
    [[nodiscard]] static std::int32_t randomDuration(std::pair<std::int32_t, std::int32_t> range,
                                                     random::RandomSource& random);

    /// parity: `Level.getRainLevel(1.0F)`, which on the server is the level itself: the `o` twin
    /// only exists to interpolate a partial tick, and the server always asks for a whole one.
    [[nodiscard]] float rainLevel() const noexcept { return rainLevel_; }

    [[nodiscard]] float thunderLevel() const noexcept { return thunderLevel_; }

    /// parity: `Level.isRaining` and `Level.isThundering` ask the **level**, not the flag, so a
    /// `/weather rain` takes 21 ticks to count and a `/weather thunder` 91.
    [[nodiscard]] bool isRaining() const noexcept { return rainLevel_ > kRainingAt; }

    [[nodiscard]] bool isThundering() const noexcept { return thunderLevel_ > kThunderingAt; }

    /// parity: `Level.setRainLevel` and `setThunderLevel`, which clamp and skip the slope.
    void setRainLevel(float level) noexcept { rainLevel_ = std::clamp(level, 0.0F, 1.0F); }

    void setThunderLevel(float level) noexcept { thunderLevel_ = std::clamp(level, 0.0F, 1.0F); }

private:
    /// parity: neither level is saved. A world that loads raining starts them at zero and climbs.
    static constexpr float kLevelStep = 0.01F;
    static constexpr float kRainingAt = 0.2F;
    static constexpr float kThunderingAt = 0.9F;

    float rainLevel_ = 0.0F;
    float thunderLevel_ = 0.0F;
};

} // namespace basalt::server::runtime::world
