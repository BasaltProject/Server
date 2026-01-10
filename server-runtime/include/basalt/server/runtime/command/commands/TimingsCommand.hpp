#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/timings/TimingsReport.hpp>

#include <cstdint>

namespace basalt::server::runtime::command::commands {

/// `/timings`: what the server is spending its time on, modelled on spark minus its sampling
/// profiler, which needs other threads' call stacks.
///
/// What is left is the half most people read anyway: tick duration distribution and directly
/// measured sections. The distribution is the point, since a server pinned at 20 TPS with one tick
/// in fifty taking 80ms stutters while its TPS reads perfect.
///
/// Off until `/timings start`: a clock read per section is small but not free.
class TimingsCommand final : public Command {
public:
    TimingsCommand(wire::common::Timings& timings, wire::common::TickDurations& tickDurations,
                   const timings::TimingsReport& report, std::function<std::int64_t()> nowNanos);

    void build(const CommandNodeBuilder& root) const override;

private:
    wire::common::Timings* timings_;
    wire::common::TickDurations* tickDurations_;
    const timings::TimingsReport* report_;
    std::function<std::int64_t()> nowNanos_;
};

} // namespace basalt::server::runtime::command::commands
