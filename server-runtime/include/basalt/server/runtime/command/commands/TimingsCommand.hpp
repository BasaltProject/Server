#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/timings/TimingsReport.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace basalt::server::runtime::command::commands {

/// What `/timings` needs from the server that it cannot reach on its own.
struct TimingsHooks {
    /// Base address of the report viewer. Empty keeps reports on disk and uploads nothing.
    std::string viewerUrl;
    /// A file with the CA to trust for the viewer, for one behind a private authority. Empty means the system's.
    std::string caFile;
    /// Hands a task to the tick thread, the only one allowed to touch players.
    std::function<void(std::function<void()>)> postToTick;
    /// Delivers a message to whoever asked: the player with that uuid, or the console when it is empty.
    std::function<void(const std::string& requester, const wire::text::Component&)> tell;
};

class TimingsCommand final : public Command {
public:
    TimingsCommand(wire::common::Timings& timings, wire::common::TickDurations& tickDurations,
                   const timings::TimingsReport& report, std::function<std::int64_t()> nowNanos,
                   TimingsHooks hooks = {});

    ~TimingsCommand() override;

    void build(const CommandNodeBuilder& root) const override;

private:
    void start(const CommandContext& context, std::int32_t timeoutSeconds) const;

    /// Builds the report on the tick thread, then uploads it from a worker so the server never waits on the network.
    void finish(const std::string& requester, bool stopMeasuring) const;

    [[nodiscard]] static std::string requesterOf(const CommandContext& context);

    wire::common::Timings* timings_;
    wire::common::TickDurations* tickDurations_;
    const timings::TimingsReport* report_;
    std::function<std::int64_t()> nowNanos_;
    TimingsHooks hooks_;

    // Mutable because `build` hands out const callbacks.
    mutable std::mutex workersMutex_;
    mutable std::vector<std::jthread> workers_;
    mutable std::jthread timer_;
    mutable std::string startedBy_;
};

} // namespace basalt::server::runtime::command::commands
