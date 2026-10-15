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

    bool tick(bool advanceTime);

    void setTotalTicks(std::int64_t ticks);

    /// parity: `ServerClockManager.addTicks`.
    void addTicks(std::int32_t ticks);

    /// Zero rate when paused, so the client stops interpolating instead of drifting.
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

/// parity: `ServerLevel.advanceWeatherCycle`.
class WorldWeather {
public:
    /// parity: `ServerLevel.RAIN_DELAY`.
    static constexpr std::pair<std::int32_t, std::int32_t> kRainDelay{12000, 180000};
    static constexpr std::pair<std::int32_t, std::int32_t> kRainDuration{12000, 24000};
    static constexpr std::pair<std::int32_t, std::int32_t> kThunderDelay{12000, 180000};
    static constexpr std::pair<std::int32_t, std::int32_t> kThunderDuration{3600, 15600};

    bool raining = false;
    std::int32_t rainTime = 0;
    bool thundering = false;
    std::int32_t thunderTime = 0;
    std::int32_t clearWeatherTime = 0;

    /// parity: `ServerLevel.advanceWeatherCycle`.
    void tick(random::RandomSource& random, bool advanceCycle);

    /// parity: `MinecraftServer.setWeatherParameters`.
    void set(std::int32_t clearTime, std::int32_t weatherTime, bool raining, bool thundering);

    [[nodiscard]] static std::int32_t randomDuration(std::pair<std::int32_t, std::int32_t> range,
                                                     random::RandomSource& random);

    /// parity: `Level.getRainLevel(1.0F)`.
    [[nodiscard]] float rainLevel() const noexcept { return rainLevel_; }

    [[nodiscard]] float thunderLevel() const noexcept { return thunderLevel_; }

    /// parity: `Level.isRaining`.
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
