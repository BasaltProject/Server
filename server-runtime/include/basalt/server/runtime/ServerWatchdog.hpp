#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <thread>

namespace basalt::server::runtime {

/// parity: `ServerWatchdog`, which gives up on a server whose tick has not finished within `max-tick-time`.
class ServerWatchdog {
public:
    ServerWatchdog(std::chrono::milliseconds limit, std::function<std::int64_t()> ticksRun,
                   std::function<void(double stalledSeconds)> onStall,
                   std::chrono::milliseconds poll = std::chrono::milliseconds{250});

    ServerWatchdog(const ServerWatchdog&) = delete;
    ServerWatchdog& operator=(const ServerWatchdog&) = delete;

private:
    std::jthread thread_;
};

} // namespace basalt::server::runtime
