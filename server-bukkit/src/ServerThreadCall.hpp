#pragma once

#include "BridgeInternal.hpp"

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <utility>

namespace basalt::server::bukkit {
/// Runs `run` on the plugin thread and waits; once the server is stopping a call that never started is withdrawn.
template <typename Result, typename Run>
[[nodiscard]] Result callOnPluginThread(BukkitBridge& bridge, Result fallback, Run run) {
    if (bridge.plugins().onPluginThread()) {
        return run();
    }

    struct Shared {
        std::atomic<int> state{0};
        std::promise<Result> done;
    };

    constexpr int kRunning = 1;
    constexpr int kWithdrawn = 2;
    const auto shared = std::make_shared<Shared>();
    std::future<Result> result = shared->done.get_future();
    plugins::PluginHandle handle = bridge.services().tasks().after(0, [shared, run = std::move(run)]() mutable {
        int expected = 0;
        if (shared->state.compare_exchange_strong(expected, kRunning)) {
            shared->done.set_value(run());
        }
    });
    while (result.wait_for(std::chrono::milliseconds{50}) != std::future_status::ready) {
        int expected = 0;
        if (bridge.stopping() && shared->state.compare_exchange_strong(expected, kWithdrawn)) {
            handle.remove();
            return fallback;
        }
    }
    return result.get();
}
} // namespace basalt::server::bukkit
