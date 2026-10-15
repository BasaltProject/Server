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

/// What a caller keeps of a task it handed to an owner: a way to take it back.
///
/// A value, not a reference: copies of one handle are the same handle, so cancelling through any of
/// them cancels the task itself.
class TaskHandle {
public:
    TaskHandle() : cancelled_(std::make_shared<std::atomic<bool>>(false)) {}

    [[nodiscard]] bool isCancelled() const noexcept { return cancelled_->load(); }

    /// Takes the task back. Safe from any thread and at any time: a task already running is not
    /// interrupted, and one that has already run does not come back.
    void cancel() const noexcept { cancelled_->store(true); }

private:
    std::shared_ptr<std::atomic<bool>> cancelled_;
};

/// What a task that threw costs: itself, and nothing else on the turn.
using FailureHandler = std::function<void(const std::exception&)>;

/// Rethrows, which is what a caller that passes nothing gets.
[[nodiscard]] const FailureHandler& rethrowOnFailure();

/// One thread and the state it owns.
///
/// parity of model with `RegionizedServer`: everything mutable belongs to exactly one owner, and
/// whoever is not that owner asks by `submit` instead of touching it.
///
/// A task may read and write what this owner owns and hand work to another owner. What it must not
/// do is wait for another owner's answer: that is a deadlock, and with one region the other owner
/// is itself. What a task adds is run on the next turn, which is what keeps a turn bounded.
class Owner {
public:
    /// `name` is what it owns, for the message a strict-mode failure prints.
    explicit Owner(std::string name) : name_(std::move(name)) {}

    Owner(const Owner&) = delete;
    Owner& operator=(const Owner&) = delete;
    Owner(Owner&&) = delete;
    Owner& operator=(Owner&&) = delete;
    ~Owner() = default;

    [[nodiscard]] const std::string& name() const noexcept { return name_; }

    /// Whether the calling thread is the one that owns this.
    [[nodiscard]] bool isCurrent() const noexcept;

    /// Whether anybody has claimed this yet.
    ///
    /// Until somebody does there is nothing to enforce and nobody to hand a task to: a world built
    /// by a test, or one that exists before the first tick, is whoever is holding it. Enforcement
    /// starts the moment a thread claims it, which on a running server is the first tick.
    [[nodiscard]] bool isClaimed() const noexcept;

    /// Whether this owner has gone: its world was closed, or its region merged into another.
    [[nodiscard]] bool isClosed() const noexcept { return closed_.load(); }

    /// How many tasks are waiting, for the timings and the tests.
    [[nodiscard]] std::size_t pending() const;

    /// Claims this owner for the calling thread. The tick thread does it on its first turn.
    void bindToCurrentThread() noexcept;

    /// Runs `task` on the owning thread. Straight away when the caller already is that thread, so a
    /// chain of gameplay does not queue behind itself; otherwise it waits its turn.
    TaskHandle submit(std::function<void()> task);

    /// Runs `task` once `delayTicks` turns have passed, counting this one. Zero is the next turn.
    ///
    /// parity of intent with `ServerLevel.scheduleTick`: a delay is in ticks because everything
    /// else here is, and a task that asks for a delay while running is due that many turns from
    /// now.
    TaskHandle schedule(std::int32_t delayTicks, std::function<void()> task);

    /// Runs `task` at the end of this turn rather than at the start of the next.
    ///
    /// For what has to see everything the turn did: sending what changed, or writing out a snapshot
    /// that would otherwise be a tick stale.
    TaskHandle scheduleAtEnd(std::function<void()> task);

    /// Runs `task` where `submit` would, but only when the turn asks for console input instead of
    /// at its start. parity: `DedicatedServer.handleConsoleInputs`, which the jar runs after the
    /// levels and before `playerList.tick`, so a console command sees the world this tick moved.
    TaskHandle submitConsole(std::function<void()> task);

    /// What the console typed since the last turn, in order. The other half of `submitConsole`.
    std::size_t runConsole(const FailureHandler& onFailure = rethrowOnFailure());

    /// Runs what is waiting, on the owning thread. Answers how many ran.
    ///
    /// Takes the whole queue in one go and runs it outside the lock: a task that submits another
    /// (a block change that schedules a tick) must not deadlock, and what it adds belongs to the
    /// next turn rather than to this one, which is what keeps a tick bounded. The two queues are
    /// swapped rather than allocated, so a turn costs no garbage of its own.
    std::size_t runPending(const FailureHandler& onFailure = rethrowOnFailure());

    /// The other half of a turn: what `scheduleAtEnd` left, run after the turn's own work.
    std::size_t runEndOfTick(const FailureHandler& onFailure = rethrowOnFailure());

    /// Moves every delayed task one turn closer and queues whatever came due.
    ///
    /// Called once a turn by whoever ticks this owner, before `runPending`, so a task scheduled for
    /// zero ticks runs on the very next turn.
    void advanceTick();

    /// Nothing else runs here: the world was closed, or this region merged into another.
    ///
    /// What was waiting is cancelled rather than run, and the handles say so. A caller that has to
    /// know its work survived a merge asks its handle instead of assuming.
    void close();

    /// Hands everything still waiting here to `other`, in order, and leaves this one empty.
    ///
    /// What a merge does with the work already queued: it moves rather than being dropped, because
    /// the caller asked for it against a piece of the world that still exists, only under a new
    /// owner. A split cannot do the same, since a task does not say which side it belongs to, so
    /// the survivor keeps it (see `RegionScheduler`).
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

    /// Where new work lands.
    std::deque<ScheduledTask> incoming_;

    /// What asked to run at the end of the turn rather than at its start.
    std::deque<ScheduledTask> atEnd_;

    /// What the console typed, held for the turn's console point instead of its start.
    std::deque<ScheduledTask> console_;

    /// What is waiting for a number of ticks to pass. Scanned once a turn, so a list is enough.
    std::vector<ScheduledTask> delayed_;

    std::atomic<std::uint64_t> boundThread_{0};
    std::atomic<bool> closed_{false};
};

/// Whether ownership is enforced or only documented.
///
/// Off in a release build, on in the tests and behind a flag for whoever is developing: with one
/// region an illegal access usually works by luck, and luck is what stops working the day regions
/// are turned on. Checking it with a single region is how that shows up now rather than then.
namespace ownership {

[[nodiscard]] bool isStrict() noexcept;

void setStrict(bool strict) noexcept;

} // namespace ownership

} // namespace basalt::server::runtime::sched
