#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `GameRuleCommand`.
class GameRuleCommand final : public Command {
public:
    GameRuleCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `TriggerCommand`.
class TriggerCommand final : public Command {
public:
    explicit TriggerCommand(scoreboard::Scoreboard& scoreboard);

    void build(const CommandNodeBuilder& root) const override;

private:
    scoreboard::Scoreboard* scoreboard_;
};

} // namespace basalt::server::runtime::command::commands
