#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::sched {

class TaskHandle {
public:
    TaskHandle() : cancelled_(std::make_shared<std::atomic<bool>>(false)) {}

    [[nodiscard]] bool isCancelled() const noexcept { return cancelled_->load(); }

    void cancel() const noexcept { cancelled_->store(true); }

private:
    std::shared_ptr<std::atomic<bool>> cancelled_;
};

using FailureHandler = std::function<void(const std::exception&)>;

[[nodiscard]] const FailureHandler& rethrowOnFailure();

class Owner {
public:
    explicit Owner(std::string name) : name_(std::move(name)) {}

    Owner(const Owner&) = delete;
    Owner& operator=(const Owner&) = delete;
    Owner(Owner&&) = delete;
    Owner& operator=(Owner&&) = delete;
    ~Owner() = default;

    [[nodiscard]] const std::string& name() const noexcept { return name_; }

    [[nodiscard]] bool isCurrent() const noexcept;

    [[nodiscard]] bool isClaimed() const noexcept;

    /// Whether a tick thread is draining owners at all: a task handed to one when none is would never run.
    static void tickThreadRunning(bool running) noexcept;

    [[nodiscard]] static bool tickThreadRunning() noexcept;

    [[nodiscard]] bool isClosed() const noexcept { return closed_.load(); }

    [[nodiscard]] std::size_t pending() const;

    /// Claims this owner for the calling thread. The tick thread does it on its first turn.
    void bindToCurrentThread() noexcept;

    TaskHandle submit(std::function<void()> task);

    TaskHandle schedule(std::int32_t delayTicks, std::function<void()> task);

    TaskHandle scheduleAtEnd(std::function<void()> task);

    /// at its start. parity: `DedicatedServer.handleConsoleInputs`, which the jar runs after the
    TaskHandle submitConsole(std::function<void()> task);

    std::size_t runConsole(const FailureHandler& onFailure = rethrowOnFailure());

    std::size_t runPending(const FailureHandler& onFailure = rethrowOnFailure());

    std::size_t runEndOfTick(const FailureHandler& onFailure = rethrowOnFailure());

    void advanceTick();

    void close();

    void drainInto(Owner& other);

    /// Throws when the caller is not the owner and strict mode is on. See `ownership`.
    void check(std::string_view what) const;

private:
    struct ScheduledTask {
        TaskHandle handle;
        std::function<void()> task;
        std::int32_t dueInTicks = 0;
    };

    void adopt(std::vector<ScheduledTask> tasks);
    std::size_t drain(std::deque<ScheduledTask>& queue, const FailureHandler& onFailure);

    std::string name_;
    mutable std::mutex mutex_;

    std::deque<ScheduledTask> incoming_;

    std::deque<ScheduledTask> atEnd_;

    std::deque<ScheduledTask> console_;

    std::vector<ScheduledTask> delayed_;

    std::atomic<std::uint64_t> boundThread_{0};
    std::atomic<bool> closed_{false};
};

namespace ownership {

[[nodiscard]] bool isStrict() noexcept;

void setStrict(bool strict) noexcept;

} // namespace ownership

} // namespace basalt::server::runtime::sched
