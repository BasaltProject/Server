#pragma once

#include <basalt/server/runtime/sched/Owner.hpp>

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>

namespace basalt::server::runtime::sched {

/// Runs `work` on the owner's thread and waits, or at once when that is this thread or no tick thread runs.
template <typename R>
R onOwner(sched::Owner& owner, std::function<R()> work) {
    if (owner.isCurrent() || !owner.isClaimed() || !sched::Owner::tickThreadRunning()) {
        return work();
    }

    struct Hop {
        std::atomic<bool> taken{false};
        std::promise<R> promise;
    };

    const auto hop = std::make_shared<Hop>();
    const auto held = std::make_shared<std::function<R()>>(std::move(work));
    std::future<R> answer = hop->promise.get_future();
    owner.submit([hop, held] {
        if (hop->taken.exchange(true)) {
            return;
        }
        try {
            hop->promise.set_value((*held)());
        } catch (...) {
            hop->promise.set_exception(std::current_exception());
        }
    });
    while (answer.wait_for(std::chrono::milliseconds{50}) != std::future_status::ready) {
        // The tick thread stopped with the task still queued: do it here, the worlds no longer change.
        if (!sched::Owner::tickThreadRunning() && !hop->taken.exchange(true)) {
            return (*held)();
        }
    }
    return answer.get();
}

} // namespace basalt::server::runtime::sched
