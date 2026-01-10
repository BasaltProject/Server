#include <basalt/server/runtime/sched/Owner.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace basalt::server::runtime::sched {
namespace {

/// A small number per thread rather than a hash of the thread id: nothing collides with the zero
/// that means "nobody has claimed this".
[[nodiscard]] std::uint64_t currentThreadId() noexcept {
    static std::atomic<std::uint64_t> next{1};
    static thread_local const std::uint64_t id = next.fetch_add(1);
    return id;
}

std::atomic<bool> strictOwnership{false};

} // namespace

const FailureHandler& rethrowOnFailure() {
    static const FailureHandler handler = [](const std::exception&) { throw; };
    return handler;
}

bool Owner::isCurrent() const noexcept { return boundThread_.load() == currentThreadId(); }

bool Owner::isClaimed() const noexcept { return boundThread_.load() != 0; }

std::size_t Owner::pending() const {
    const std::lock_guard<std::mutex> lock{mutex_};
    return incoming_.size() + atEnd_.size() + delayed_.size();
}

void Owner::bindToCurrentThread() noexcept { boundThread_.store(currentThreadId()); }

TaskHandle Owner::submit(std::function<void()> task) {
    TaskHandle handle;
    if (isClosed()) {
        // Nobody is going to run it, and saying so is better than holding it for ever.
        handle.cancel();
        return handle;
    }
    if (isCurrent() || !isClaimed()) {
        task();
        return handle;
    }
    const std::lock_guard<std::mutex> lock{mutex_};
    incoming_.push_back(ScheduledTask{.handle = handle, .task = std::move(task), .dueInTicks = 0});
    return handle;
}

TaskHandle Owner::schedule(std::int32_t delayTicks, std::function<void()> task) {
    TaskHandle handle;
    if (isClosed()) {
        handle.cancel();
        return handle;
    }
    const std::lock_guard<std::mutex> lock{mutex_};
    // A negative delay is not a task from the past: the clamp and the `<= 0` below say the same
    // thing here, and it stays so that `pending` and a merge see a number that means what it says.
    delayed_.push_back(ScheduledTask{.handle = handle, .task = std::move(task), .dueInTicks = std::max(delayTicks, 0)});
    return handle;
}

TaskHandle Owner::scheduleAtEnd(std::function<void()> task) {
    TaskHandle handle;
    if (isClosed()) {
        handle.cancel();
        return handle;
    }
    const std::lock_guard<std::mutex> lock{mutex_};
    atEnd_.push_back(ScheduledTask{.handle = handle, .task = std::move(task), .dueInTicks = 0});
    return handle;
}

std::size_t Owner::runPending(const FailureHandler& onFailure) { return drain(incoming_, running_, onFailure); }

std::size_t Owner::runEndOfTick(const FailureHandler& onFailure) { return drain(atEnd_, atEndRunning_, onFailure); }

void Owner::advanceTick() {
    const std::lock_guard<std::mutex> lock{mutex_};
    // Nothing waiting is the common case by far, and this is what saves scanning for it.
    if (delayed_.empty()) {
        return;
    }
    std::size_t index = 0;
    while (index < delayed_.size()) {
        ScheduledTask& task = delayed_[index];
        if (task.handle.isCancelled()) {
            delayed_.erase(delayed_.begin() + static_cast<std::ptrdiff_t>(index));
        } else if (task.dueInTicks <= 0) {
            incoming_.push_back(std::move(task));
            delayed_.erase(delayed_.begin() + static_cast<std::ptrdiff_t>(index));
        } else {
            --task.dueInTicks;
            ++index;
        }
    }
}

void Owner::close() {
    closed_.store(true);
    const std::lock_guard<std::mutex> lock{mutex_};
    for (const ScheduledTask& task : incoming_) {
        task.handle.cancel();
    }
    for (const ScheduledTask& task : atEnd_) {
        task.handle.cancel();
    }
    for (const ScheduledTask& task : delayed_) {
        task.handle.cancel();
    }
    incoming_.clear();
    atEnd_.clear();
    delayed_.clear();
}

void Owner::drainInto(Owner& other) {
    std::vector<ScheduledTask> moved;
    {
        const std::lock_guard<std::mutex> lock{mutex_};
        moved.reserve(incoming_.size() + atEnd_.size() + delayed_.size());
        for (ScheduledTask& task : incoming_) {
            moved.push_back(std::move(task));
        }
        for (ScheduledTask& task : atEnd_) {
            moved.push_back(std::move(task));
        }
        for (ScheduledTask& task : delayed_) {
            moved.push_back(std::move(task));
        }
        incoming_.clear();
        atEnd_.clear();
        delayed_.clear();
    }
    if (moved.empty()) {
        return;
    }
    other.adopt(std::move(moved));
}

void Owner::adopt(std::vector<ScheduledTask> tasks) {
    const std::lock_guard<std::mutex> lock{mutex_};
    for (ScheduledTask& task : tasks) {
        if (task.handle.isCancelled()) {
            continue;
        }
        if (task.dueInTicks > 0) {
            delayed_.push_back(std::move(task));
        } else {
            incoming_.push_back(std::move(task));
        }
    }
}

void Owner::check(std::string_view what) const {
    if (!ownership::isStrict() || !isClaimed() || isCurrent()) {
        return;
    }
    throw std::logic_error{std::string{what} + " belongs to '" + name_ + "' and was touched from another thread"};
}

std::size_t Owner::drain(std::deque<ScheduledTask>& queue, std::deque<ScheduledTask>& spare,
                         const FailureHandler& onFailure) {
    // Only the owning thread runs the queue. Two threads draining the same owner swap the two
    // buffers past each other and end up iterating one that the other is filling, which is a
    // corrupted queue and a packet handled twice. It is a bug in the caller, so it says so.
    if (isClaimed() && !isCurrent()) {
        throw std::logic_error{"'" + name_ + "' was drained from a thread that does not own it"};
    }
    {
        const std::lock_guard<std::mutex> lock{mutex_};
        queue.swap(spare);
    }
    // An empty turn is the common case for most owners, and this is what keeps it free.
    if (spare.empty()) {
        return 0;
    }
    std::size_t ran = 0;
    for (const ScheduledTask& task : spare) {
        if (task.handle.isCancelled()) {
            continue;
        }
        // One task that throws costs itself and nothing else: the rest of this turn belongs to
        // whoever else asked, and dropping them would lose a player's packets for a tick.
        try {
            task.task();
            ++ran;
        } catch (const std::exception& failure) {
            onFailure(failure);
        }
    }
    spare.clear();
    return ran;
}

bool ownership::isStrict() noexcept { return strictOwnership.load(); }

void ownership::setStrict(bool strict) noexcept { strictOwnership.store(strict); }

} // namespace basalt::server::runtime::sched
