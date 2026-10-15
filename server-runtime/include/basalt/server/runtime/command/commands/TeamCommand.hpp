#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `TeamCommand`.
class TeamCommand final : public Command {
public:
    explicit TeamCommand(scoreboard::Teams& teams);

    void build(const CommandNodeBuilder& root) const override;

private:
    scoreboard::Teams* teams_;
};

} // namespace basalt::server::runtime::command::commands
