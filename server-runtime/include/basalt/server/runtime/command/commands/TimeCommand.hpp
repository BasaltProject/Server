#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `TimeCommand` as 26.2 rewrote it: clocks, not "day time".
class TimeCommand final : public Command {
public:
    TimeCommand();

    void build(const CommandNodeBuilder& root) const override;
};

} // namespace basalt::server::runtime::command::commands
