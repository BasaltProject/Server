#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `ScoreboardCommand`, both halves: `objectives` and `players`.
///
/// Objectives are created with a criterion but nothing increments them yet, so in practice every
/// objective behaves as `dummy` and `/trigger` is the one that moves on its own.
class ScoreboardCommand final : public Command {
public:
    explicit ScoreboardCommand(scoreboard::Scoreboard& scoreboard);

    void build(const CommandNodeBuilder& root) const override;

private:
    /// The `objectives` half, which is the objectives themselves and where they are drawn.
    void buildObjectives(const CommandNodeBuilder& root) const;

    /// The `players` half, which is the scores under them.
    void buildPlayers(const CommandNodeBuilder& root) const;

    scoreboard::Scoreboard* scoreboard_;
};

} // namespace basalt::server::runtime::command::commands
