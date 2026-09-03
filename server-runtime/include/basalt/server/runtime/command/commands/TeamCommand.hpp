#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `TeamCommand`. `deathMessageVisibility` is stored and travels with the team; who is
/// actually shown a death message belongs with the death message work, not here.
class TeamCommand final : public Command {
public:
    explicit TeamCommand(scoreboard::Teams& teams);

    void build(const CommandNodeBuilder& root) const override;

private:
    scoreboard::Teams* teams_;
};

} // namespace basalt::server::runtime::command::commands
