#pragma once

#include <basalt/wire/common/timings/TickDurations.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <thread>
#include <vector>

namespace basalt::server::runtime {

struct TickRates {
    double oneMinute = 20.0;
    double fiveMinutes = 20.0;
    double fifteenMinutes = 20.0;

    friend bool operator==(const TickRates&, const TickRates&) = default;
};

struct TickLoopOptions {
    std::int32_t saveIntervalSeconds = 300;
    /// parity: vanilla unloads every tick; a sweep that walks every chunk cannot, so it has its own.
    std::int32_t unloadIntervalSeconds = 30;
    std::function<void()> onSaveInterval;
    std::function<void()> onUnloadInterval;
    /// parity: `pause-when-empty-seconds`; 0 or unset never pauses.
    std::function<std::int32_t()> pauseWhenEmptySeconds;
    std::function<bool()> hasPlayers;
    /// parity: `worldRuns` is false while `/tick freeze` holds and no step is owed.
    std::function<void(bool worldRuns)> onTick;
};

class TickLoop {
public:
    static constexpr std::int64_t kTickMillis = 50;
    static constexpr std::int64_t kTicksPerSecond = 20;

    static constexpr float kMinTickRate = 1.0F;
    static constexpr float kMaxTickRate = 10000.0F;

    static constexpr std::int32_t kDefaultSaveIntervalSeconds = 300;
    /// parity: vanilla unloads every tick; a sweep that walks every chunk cannot, so it has its own.
    static constexpr std::int32_t kDefaultUnloadIntervalSeconds = 30;

    using Options = TickLoopOptions;

    explicit TickLoop(Options options = {});

    ~TickLoop();

    TickLoop(const TickLoop&) = delete;
    TickLoop& operator=(const TickLoop&) = delete;
    TickLoop(TickLoop&&) = delete;
    TickLoop& operator=(TickLoop&&) = delete;

    [[nodiscard]] wire::common::TickDurations& durations() noexcept { return durations_; }

    [[nodiscard]] const wire::common::TickDurations& durations() const noexcept { return durations_; }

    [[nodiscard]] TickRates tickRates() const;

    [[nodiscard]] float tickRate() const noexcept { return targetRate_.load(std::memory_order_relaxed); }

    [[nodiscard]] bool isFrozen() const noexcept { return frozen_.load(std::memory_order_relaxed); }

    [[nodiscard]] bool isPaused() const noexcept { return paused_.load(std::memory_order_relaxed); }

    /// parity: `MinecraftServer.tickTimesNanos`, the last hundred tick durations in a ring.
    [[nodiscard]] std::vector<std::int64_t> recentTickNanos() const;

    [[nodiscard]] bool isSprinting() const noexcept { return sprintTicksLeft_.load(std::memory_order_relaxed) > 0; }

    [[nodiscard]] std::int64_t ticksRun() const noexcept { return ticksCounted_.load(std::memory_order_relaxed); }

    void setTickRate(float rate);

    void setFrozen(bool value);

    /// Runs `ticks` ticks while frozen. Answers false when the loop is not frozen, as vanilla does.
    bool step(std::int32_t ticks);

    void sprint(std::int32_t ticks);

    [[nodiscard]] std::int32_t stepsOwed() const noexcept { return stepsOwed_.load(std::memory_order_relaxed); }

    /// Answers false when no step was owed.
    bool stopStepping();

    bool stopSprinting();

    void start();

    void stop();

private:
    [[nodiscard]] std::int64_t nextAutosaveInterval() const;

    void runTicks();

    void runSweeper();

    Options options_;
    std::int64_t unloadIntervalTicks_;
    wire::common::TickDurations durations_;

    std::atomic<bool> running_{false};
    std::atomic<double> oneMinute_{20.0};
    std::atomic<double> fiveMinutes_{20.0};
    std::atomic<double> fifteenMinutes_{20.0};

    // A flag, not a queue: a sweep that outlasts an interval owes one more sweep, not several.
    std::atomic<bool> sweepRequested_{false};
    std::atomic<bool> unloadRequested_{false};
    std::atomic<bool> sweeping_{false};

    std::atomic<float> targetRate_{static_cast<float>(kTicksPerSecond)};
    std::atomic<bool> frozen_{false};
    std::atomic<std::int32_t> stepsOwed_{0};
    std::atomic<std::int64_t> ticksCounted_{0};
    std::atomic<std::int32_t> sprintTicksLeft_{0};
    std::atomic<bool> paused_{false};
    std::array<std::atomic<std::int64_t>, 100> recentNanos_{};

    std::thread ticker_;
    std::thread sweeper_;
};

} // namespace basalt::server::runtime
