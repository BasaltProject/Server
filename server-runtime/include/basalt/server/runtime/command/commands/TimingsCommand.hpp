#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/timings/TimingsReport.hpp>

#include <cstdint>

namespace basalt::server::runtime::command::commands {

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
