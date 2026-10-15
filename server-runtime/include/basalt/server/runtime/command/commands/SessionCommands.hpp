#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

#include <cstdint>

namespace basalt::server::runtime::command::commands {

/// parity: `SetPlayerIdleTimeoutCommand`.
class SetIdleTimeoutCommand final : public Command {
public:
    explicit SetIdleTimeoutCommand(std::function<void(std::int32_t minutes)> setTimeoutMinutes);

    void build(const CommandNodeBuilder& root) const override;

private:
    std::function<void(std::int32_t)> setTimeoutMinutes_;
};

/// parity: `TeamMsgCommand`.
class TeamMsgCommand final : public Command {
public:
    explicit TeamMsgCommand(scoreboard::Teams& teams);

    void build(const CommandNodeBuilder& root) const override;

private:
    scoreboard::Teams* teams_;
};

class RestartCommand final : public Command {
public:
    explicit RestartCommand(std::function<void()> requestRestart);

    void build(const CommandNodeBuilder& root) const override;

private:
    std::function<void()> requestRestart_;
};

/// parity: `SpectateCommand`.
class SpectateCommand final : public Command {
public:
    SpectateCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `TransferCommand`.
class TransferCommand final : public Command {
public:
    TransferCommand();

    void build(const CommandNodeBuilder& root) const override;
};

} // namespace basalt::server::runtime::command::commands
