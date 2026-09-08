#pragma once

#include <basalt/wire/common/timings/TickDurations.hpp>

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <thread>

namespace basalt::server::runtime {

/// Ticks per second over the three windows, the way every server reports them.
struct TickRates {
    double oneMinute = 20.0;
    double fiveMinutes = 20.0;
    double fifteenMinutes = 20.0;

    friend bool operator==(const TickRates&, const TickRates&) = default;
};

/// What a caller hands over. A struct because all of it has a sensible default.
///
/// At namespace scope rather than nested: a nested struct cannot supply the default member
/// initializers the enclosing class's own default argument needs.
struct TickLoopOptions {
    /// 0 seconds means no periodic save at all; chunks still go out when they unload or on stop.
    std::int32_t saveIntervalSeconds = 300;
    /// parity: vanilla unloads every tick; a sweep that walks every chunk cannot, so it has its own
    /// clock.
    std::int32_t unloadIntervalSeconds = 30;
    /// The periodic save, on its own thread. On the tick thread it outlasted the catch-up limit, so
    /// its backlog was dropped rather than caught up and those ticks were simply lost.
    std::function<void()> onSaveInterval;
    /// Dropping the chunks nobody can reach, which is not the same job as saving them. Hanging both
    /// off the save interval meant memory only came back when the world was written.
    std::function<void()> onUnloadInterval;
    /// Runs every tick. Deliberately not a general extension point: it exists so something checks
    /// for a termination signal 20 times a second, which no other thread here can do.
    ///
    /// parity: `worldRuns` is false while `/tick freeze` holds and no step is owed. Vanilla keeps
    /// serving the console while frozen and only stops the world, so the callback still runs and
    /// decides for itself what a frozen turn is worth doing.
    std::function<void(bool worldRuns)> onTick;
};

/// A 20 TPS heartbeat on its own thread, which is what drives every world's turn.
///
/// It deliberately touches no sockets. Writing to a client that has stopped keeping up parks
/// whichever thread is doing the writing until that one client drains, and doing that from this
/// shared thread stalled saving, chunk unloading and every other player at once the moment a single
/// client fell behind. Anything that has to reach a client belongs on that client's own thread.
class TickLoop {
public:
    static constexpr std::int64_t kTickMillis = 50;
    static constexpr std::int64_t kTicksPerSecond = 20;

    /// parity: the bounds `/tick rate` accepts.
    static constexpr float kMinTickRate = 1.0F;
    static constexpr float kMaxTickRate = 10000.0F;

    /// parity: `MinecraftServer.computeNextAutosaveInterval`, five minutes at the rate asked for and
    /// never fewer than a hundred ticks. Linear compresses a region as one stream, so saving one
    /// chunk rewrites all 1024 of its neighbours.
    static constexpr std::int32_t kDefaultSaveIntervalSeconds = 300;
    /// parity: vanilla unloads every tick; a sweep that walks every chunk cannot, so it has its own
    /// clock.
    static constexpr std::int32_t kDefaultUnloadIntervalSeconds = 30;

    using Options = TickLoopOptions;

    explicit TickLoop(Options options = {});

    ~TickLoop();

    TickLoop(const TickLoop&) = delete;
    TickLoop& operator=(const TickLoop&) = delete;
    TickLoop(TickLoop&&) = delete;
    TickLoop& operator=(TickLoop&&) = delete;

    /// Every tick's duration, for the distribution `/timings` reports.
    [[nodiscard]] wire::common::TickDurations& durations() noexcept { return durations_; }

    [[nodiscard]] const wire::common::TickDurations& durations() const noexcept { return durations_; }

    /// How many ticks a second the loop is actually managing, over one, five and fifteen minutes.
    [[nodiscard]] TickRates tickRates() const;

    [[nodiscard]] float tickRate() const noexcept { return targetRate_.load(std::memory_order_relaxed); }

    [[nodiscard]] bool isFrozen() const noexcept { return frozen_.load(std::memory_order_relaxed); }

    [[nodiscard]] bool isSprinting() const noexcept { return sprintTicksLeft_.load(std::memory_order_relaxed) > 0; }

    /// Ticks run since the loop started, which is what `/debug stop` reports over its window.
    [[nodiscard]] std::int64_t ticksRun() const noexcept { return ticksCounted_.load(std::memory_order_relaxed); }

    void setTickRate(float rate);

    void setFrozen(bool value);

    /// Runs `ticks` ticks while frozen. Answers false when the loop is not frozen, as vanilla does.
    bool step(std::int32_t ticks);

    /// Runs `ticks` ticks as fast as they will go, ignoring the rate.
    void sprint(std::int32_t ticks);

    bool stopSprinting();

    void start();

    /// Stops ticking and waits for a sweep in flight: the caller's next move is to save once itself.
    void stop();

private:
    /// How many ticks until the next autosave.
    ///
    /// parity: `MinecraftServer.computeNextAutosaveInterval`, which counts the interval in ticks at
    /// the rate being asked for, so slowing the tick rate does not stretch five minutes into an
    /// hour, and never lets it fall under a hundred ticks.
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

    /// `/tick` state: the rate asked for, whether the loop is frozen, and how many ticks a frozen
    /// loop still owes. parity: `ServerTickRateManager`.
    ///
    /// Frozen means the loop keeps running at its own pace but does no work, so connections and
    /// chunk streaming carry on: that is what vanilla's freeze does too, and it is why a frozen
    /// server still answers a `/tick unfreeze`.
    std::atomic<float> targetRate_{static_cast<float>(kTicksPerSecond)};
    std::atomic<bool> frozen_{false};
    std::atomic<std::int32_t> stepsOwed_{0};
    std::atomic<std::int64_t> ticksCounted_{0};
    std::atomic<std::int32_t> sprintTicksLeft_{0};

    std::thread ticker_;
    std::thread sweeper_;
};

} // namespace basalt::server::runtime
