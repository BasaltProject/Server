#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `GameRuleCommand`. One literal per rule, each optionally taking a value, exactly as
/// vanilla builds the tree, so the client autocompletes rule names.
///
/// Every rule is declared twice, bare and under its `minecraft:` id, which is what vanilla does and
/// what makes `/gamerule minecraft:keep_inventory` work.
class GameRuleCommand final : public Command {
public:
    GameRuleCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `TriggerCommand`, the one command an ordinary player may run against the scoreboard.
///
/// It only works on an objective of criterion `trigger` that `/scoreboard players enable` has armed
/// for that player, and running it disarms it again: that is the whole mechanism datapacks use to
/// offer a menu to players who have no permissions at all.
class TriggerCommand final : public Command {
public:
    explicit TriggerCommand(scoreboard::Scoreboard& scoreboard);

    void build(const CommandNodeBuilder& root) const override;

private:
    scoreboard::Scoreboard* scoreboard_;
};

} // namespace basalt::server::runtime::command::commands
