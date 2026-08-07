#include <basalt/server/runtime/TickLoop.hpp>
#include <basalt/server/runtime/sched/Owner.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <thread>

namespace basalt::server::runtime {
namespace {

constexpr float kMillisPerSecond = 1000.0F;
constexpr std::int64_t kCatchUpLimit = 20;
constexpr std::int64_t kSweepPollMillis = 5;
constexpr std::int64_t kStopDrainMillis = 30'000;
constexpr int kWindowSeconds = 900;
constexpr std::int64_t kMinAutosaveTicks = 100;

using Clock = std::chrono::steady_clock;

[[nodiscard]] std::int64_t millisSince(Clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();
}

[[nodiscard]] std::int64_t nanosSince(Clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
}

void sleepMillis(std::int64_t millis) { std::this_thread::sleep_for(std::chrono::milliseconds{millis}); }

// Buckets count how many ticks happened, which notices a loop that stalled completely.
class TickMeter {
public:
    TickRates tick() {
        const std::int64_t second = std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - start_).count();
        if (second != currentSecond_) {
            // Fill whole seconds where nothing ticked, or a stall would read as a gap rather than a zero.
            const std::int64_t gaps = std::min<std::int64_t>(second - currentSecond_, kWindowSeconds);
            for (std::int64_t filled = 0; filled < gaps; filled++) {
                seconds_.push_back(filled == 0 ? currentCount_ : 0);
            }
            while (seconds_.size() > static_cast<std::size_t>(kWindowSeconds)) {
                seconds_.pop_front();
            }
            currentSecond_ = second;
            currentCount_ = 0;
        }
        currentCount_++;
        return TickRates{
            .oneMinute = rateOver(60), .fiveMinutes = rateOver(300), .fifteenMinutes = rateOver(kWindowSeconds)};
    }

private:
    [[nodiscard]] double rateOver(int window) const {
        if (seconds_.empty()) {
            return static_cast<double>(TickLoop::kTicksPerSecond);
        }
        const std::size_t counted = std::min(seconds_.size(), static_cast<std::size_t>(window));
        std::int64_t total = 0;
        for (std::size_t offset = 0; offset < counted; offset++) {
            total += seconds_[seconds_.size() - counted + offset];
        }
        const double rate = static_cast<double>(total) / static_cast<double>(counted);
        return std::min(rate, static_cast<double>(TickLoop::kTicksPerSecond));
    }

    Clock::time_point start_ = Clock::now();
    std::deque<std::int32_t> seconds_;
    std::int64_t currentSecond_ = 0;
    std::int32_t currentCount_ = 0;
};

} // namespace

TickLoop::TickLoop(Options options)
    : options_(std::move(options)),
      unloadIntervalTicks_(static_cast<std::int64_t>(options_.unloadIntervalSeconds) * kTicksPerSecond) {}

TickLoop::~TickLoop() { stop(); }

TickRates TickLoop::tickRates() const {
    return TickRates{.oneMinute = oneMinute_.load(std::memory_order_relaxed),
                     .fiveMinutes = fiveMinutes_.load(std::memory_order_relaxed),
                     .fifteenMinutes = fifteenMinutes_.load(std::memory_order_relaxed)};
}

void TickLoop::setTickRate(float rate) {
    targetRate_.store(std::clamp(rate, kMinTickRate, kMaxTickRate), std::memory_order_relaxed);
}

void TickLoop::setFrozen(bool value) {
    frozen_.store(value, std::memory_order_relaxed);
    if (!value) {
        stepsOwed_.store(0, std::memory_order_relaxed);
    }
}

bool TickLoop::step(std::int32_t ticks) {
    if (!frozen_.load(std::memory_order_relaxed)) {
        return false;
    }
    stepsOwed_.store(ticks, std::memory_order_relaxed);
    return true;
}

bool TickLoop::stopStepping() { return stepsOwed_.exchange(0, std::memory_order_relaxed) > 0; }

void TickLoop::sprint(std::int32_t ticks) { sprintTicksLeft_.store(ticks, std::memory_order_relaxed); }

bool TickLoop::stopSprinting() {
    if (sprintTicksLeft_.load(std::memory_order_relaxed) <= 0) {
        return false;
    }
    sprintTicksLeft_.store(0, std::memory_order_relaxed);
    return true;
}

std::vector<std::int64_t> TickLoop::recentTickNanos() const {
    std::vector<std::int64_t> out;
    out.reserve(recentNanos_.size());
    for (const std::atomic<std::int64_t>& nanos : recentNanos_) {
        out.push_back(nanos.load(std::memory_order_relaxed));
    }
    return out;
}

std::int64_t TickLoop::nextAutosaveInterval() const {
    return std::max(kMinAutosaveTicks, static_cast<std::int64_t>(targetRate_.load(std::memory_order_relaxed) *
                                                                 static_cast<float>(options_.saveIntervalSeconds)));
}

void TickLoop::start() {
    running_.store(true, std::memory_order_relaxed);
    sched::Owner::tickThreadRunning(true);
    sweeper_ = std::thread{[this] { runSweeper(); }};
    ticker_ = std::thread{[this] { runTicks(); }};
}

void TickLoop::runTicks() {
    TickMeter meter;
    std::int64_t tick = 0;
    std::int64_t ticksUntilSave = nextAutosaveInterval();
    const std::int64_t playerSaveTicks =
        static_cast<std::int64_t>(options_.playerSaveIntervalSeconds) * kTicksPerSecond;
    std::int64_t ticksUntilPlayerSave = playerSaveTicks;
    const Clock::time_point clock = Clock::now();
    std::int64_t nextTickMillis = 0;
    std::int64_t emptyTicks = 0;
    while (running_.load(std::memory_order_relaxed)) {
        const Clock::time_point started = Clock::now();
        const bool sprinting = sprintTicksLeft_.load(std::memory_order_relaxed) > 0;
        const bool stepping = stepsOwed_.load(std::memory_order_relaxed) > 0;
        bool skip = frozen_.load(std::memory_order_relaxed) && !stepping;
        // parity: `MinecraftServer.tickServer` counts empty ticks and only serves connections once past the limit.
        const std::int64_t pauseAfter =
            options_.pauseWhenEmptySeconds ? static_cast<std::int64_t>(options_.pauseWhenEmptySeconds()) * 20 : 0;
        if (pauseAfter > 0) {
            emptyTicks = options_.hasPlayers && !options_.hasPlayers() && !sprinting ? emptyTicks + 1 : 0;
            if (emptyTicks == pauseAfter) {
                sweepRequested_.store(true, std::memory_order_relaxed);
            }
            skip = skip || emptyTicks >= pauseAfter;
        } else {
            emptyTicks = 0;
        }
        paused_.store(pauseAfter > 0 && emptyTicks >= pauseAfter, std::memory_order_relaxed);
        if (stepping) {
            stepsOwed_.fetch_sub(1, std::memory_order_relaxed);
        }
        if (sprinting) {
            sprintTicksLeft_.fetch_sub(1, std::memory_order_relaxed);
        }
        tick++;
        ticksCounted_.store(tick, std::memory_order_relaxed);
        if (options_.onTick) {
            options_.onTick(!skip);
        }
        if (options_.saveIntervalSeconds > 0 && --ticksUntilSave <= 0) {
            // parity: `MinecraftServer.autoSave`.
            ticksUntilSave = nextAutosaveInterval();
            sweepRequested_.store(true, std::memory_order_relaxed);
        }
        if (playerSaveTicks > 0 && --ticksUntilPlayerSave <= 0) {
            ticksUntilPlayerSave = playerSaveTicks;
            playersRequested_.store(true, std::memory_order_relaxed);
        }
        // Unloading is not gated on the save interval: a server that never saves periodically must still drop chunks.
        if (unloadIntervalTicks_ > 0 && tick % unloadIntervalTicks_ == 0) {
            unloadRequested_.store(true, std::memory_order_relaxed);
        }

        if (!skip) {
            recentNanos_[static_cast<std::size_t>(tick % 100)].store(nanosSince(started), std::memory_order_relaxed);
        }
        durations_.record(nanosSince(clock), nanosSince(started));
        const TickRates rates = meter.tick();
        oneMinute_.store(rates.oneMinute, std::memory_order_relaxed);
        fiveMinutes_.store(rates.fiveMinutes, std::memory_order_relaxed);
        fifteenMinutes_.store(rates.fifteenMinutes, std::memory_order_relaxed);

        // Slept towards an absolute deadline: sleep overshoot alone held an idle server at 17 TPS.
        if (sprinting) {
            nextTickMillis = millisSince(clock);
            continue;
        }
        nextTickMillis += std::max<std::int64_t>(
            1, static_cast<std::int64_t>(kMillisPerSecond / targetRate_.load(std::memory_order_relaxed)));
        const std::int64_t remaining = nextTickMillis - millisSince(clock);
        if (remaining > 0) {
            sleepMillis(remaining);
        } else if (remaining < -kTickMillis * kCatchUpLimit) {
            // Too far behind: skip the backlog rather than spin through it, which turns a stall into a spiral.
            nextTickMillis = millisSince(clock);
        }
    }
}

void TickLoop::runSweeper() {
    while (running_.load(std::memory_order_relaxed)) {
        bool saving = true;
        bool unloading = true;
        bool players = true;
        saving = sweepRequested_.compare_exchange_strong(saving, false, std::memory_order_relaxed);
        unloading = unloadRequested_.compare_exchange_strong(unloading, false, std::memory_order_relaxed);
        players = playersRequested_.compare_exchange_strong(players, false, std::memory_order_relaxed);
        if (!saving && !unloading && !players) {
            sleepMillis(kSweepPollMillis);
            continue;
        }
        sweeping_.store(true, std::memory_order_relaxed);
        if (saving && options_.onSaveInterval) {
            options_.onSaveInterval();
        } else if (players && options_.onPlayerSaveInterval) {
            options_.onPlayerSaveInterval();
        }
        if (unloading && options_.onUnloadInterval) {
            options_.onUnloadInterval();
        }
        sweeping_.store(false, std::memory_order_relaxed);
    }
}

void TickLoop::stop() {
    running_.store(false, std::memory_order_relaxed);
    std::int64_t waited = 0;
    while (sweeping_.load(std::memory_order_relaxed) && waited < kStopDrainMillis) {
        sleepMillis(kSweepPollMillis);
        waited += kSweepPollMillis;
    }
    // A stop from inside the tick would make the ticker join itself, so the destructor does the join.
    if (ticker_.joinable() && ticker_.get_id() != std::this_thread::get_id()) {
        ticker_.join();
    }
    if (sweeper_.joinable()) {
        sweeper_.join();
    }
    sched::Owner::tickThreadRunning(false);
}

} // namespace basalt::server::runtime
