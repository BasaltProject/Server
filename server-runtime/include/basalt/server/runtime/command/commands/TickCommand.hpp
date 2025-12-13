#pragma once

#include <basalt/server/runtime/TickLoop.hpp>
#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `TickCommand`. `freeze` stops the world without stopping the server: connections, chunk
/// streaming and commands all keep working, which is what makes `/tick unfreeze` reachable.
class TickCommand final : public Command {
public:
    /// `nowNanos` is the same monotonic clock the tick loop records with, so a window means the same
    /// thing.
    TickCommand(TickLoop& tickLoop, wire::common::TickDurations& durations, std::function<std::int64_t()> nowNanos);

    void build(const CommandNodeBuilder& root) const override;

private:
    TickLoop* tickLoop_;
    wire::common::TickDurations* durations_;
    std::function<std::int64_t()> nowNanos_;
};

} // namespace basalt::server::runtime::command::commands
