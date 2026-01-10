#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

#include <cstdint>

namespace basalt::server::runtime::command::commands {

/// The commands that act on a player's session: who they watch, who hears them, where they play.

/// parity: `SetPlayerIdleTimeoutCommand`.
///
/// The value is recorded and reported; nothing kicks an idle player yet, because idleness is
/// measured from movement and input the server does not track per player. Stated here rather than
/// accepted silently.
class SetIdleTimeoutCommand final : public Command {
public:
    explicit SetIdleTimeoutCommand(std::function<void(std::int32_t minutes)> setTimeoutMinutes);

    void build(const CommandNodeBuilder& root) const override;

private:
    std::function<void(std::int32_t)> setTimeoutMinutes_;
};

/// parity: `TeamMsgCommand`, whose `chat.type.team.text` key is what draws the team-coloured
/// brackets.
class TeamMsgCommand final : public Command {
public:
    explicit TeamMsgCommand(scoreboard::Teams& teams);

    void build(const CommandNodeBuilder& root) const override;

private:
    scoreboard::Teams* teams_;
};

/// `/restart`: winds this server down exactly as `/stop` does, then builds another one from the
/// configuration files as they stand, in the same process. Nothing carries over, which is the
/// point: everything `/reload` has to refuse because it was decided at startup is decided again.
///
/// Players are disconnected: their connection belongs to the instance being replaced.
class RestartCommand final : public Command {
public:
    explicit RestartCommand(std::function<void()> requestRestart);

    void build(const CommandNodeBuilder& root) const override;

private:
    std::function<void()> requestRestart_;
};

/// parity: `SpectateCommand`, which only works while the spectator is actually in spectator mode.
class SpectateCommand final : public Command {
public:
    SpectateCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `TransferCommand`, which hands a player to another server without them reconnecting by
/// hand.
class TransferCommand final : public Command {
public:
    TransferCommand();

    void build(const CommandNodeBuilder& root) const override;
};

} // namespace basalt::server::runtime::command::commands
