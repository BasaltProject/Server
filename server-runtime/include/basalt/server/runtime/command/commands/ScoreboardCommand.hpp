#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `ScoreboardCommand`, both halves: `objectives` and `players`.
class ScoreboardCommand final : public Command {
public:
    explicit ScoreboardCommand(scoreboard::Scoreboard& scoreboard);

    void build(const CommandNodeBuilder& root) const override;

private:
    void buildObjectives(const CommandNodeBuilder& root) const;

    void buildPlayers(const CommandNodeBuilder& root) const;

    scoreboard::Scoreboard* scoreboard_;
};

} // namespace basalt::server::runtime::command::commands
