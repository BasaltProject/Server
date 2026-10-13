#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// The commands that act on a player: their mode, their life, and what the server tells them.

/// parity: `GameModeCommand`.
class GameModeCommand final : public Command {
public:
    GameModeCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `DefaultGameModeCommands`, minus its "move everyone who is on the old default" pass,
/// which vanilla only does for players whose mode was never set individually and which needs a flag
/// on the player that does not exist yet.
class DefaultGameModeCommand final : public Command {
public:
    DefaultGameModeCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `KillCommand`. Vanilla's `Entity.kill` is unblockable damage, so creative mode does not
/// save you.
class KillCommand final : public Command {
public:
    KillCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `HelpCommand`. Prints brigadier's own smart usage, one line per command, slash included.
///
/// The argument form parses the whole greedy line against the tree and describes the node it
/// reached, the input echoed before each usage line, so `/help give @s dirt` continues past
/// `give` exactly like vanilla.
class HelpCommand final : public Command {
public:
    HelpCommand();

    void build(const CommandNodeBuilder& root) const override;
};

} // namespace basalt::server::runtime::command::commands
