#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `GameModeCommand`.
class GameModeCommand final : public Command {
public:
    GameModeCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `DefaultGameModeCommands`.
class DefaultGameModeCommand final : public Command {
public:
    DefaultGameModeCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `KillCommand`.
class KillCommand final : public Command {
public:
    KillCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `HelpCommand`.
class HelpCommand final : public Command {
public:
    HelpCommand();

    void build(const CommandNodeBuilder& root) const override;
};

} // namespace basalt::server::runtime::command::commands
