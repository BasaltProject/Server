#pragma once

#include <basalt/server/runtime/TickLoop.hpp>
#include <basalt/server/runtime/command/CommandDispatcher.hpp>

#include <cstdint>

namespace basalt::server::runtime::command::commands {

/// parity: `TickCommand`.
class TickCommand final : public Command {
public:
    TickCommand(TickLoop& tickLoop, wire::common::TickDurations& durations, std::function<std::int64_t()> nowNanos);

    void build(const CommandNodeBuilder& root) const override;

private:
    TickLoop* tickLoop_;
    wire::common::TickDurations* durations_;
    std::function<std::int64_t()> nowNanos_;
};

} // namespace basalt::server::runtime::command::commands
