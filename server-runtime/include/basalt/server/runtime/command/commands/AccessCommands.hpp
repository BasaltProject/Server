#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

#include <cstdint>

namespace basalt::server::runtime::command::commands {

/// parity: `BanPlayerCommands`.
class BanCommand final : public Command {
public:
    explicit BanCommand(std::function<std::int64_t()> nowEpochSeconds);

    void build(const CommandNodeBuilder& root) const override;

private:
    std::function<std::int64_t()> nowEpochSeconds_;
};

/// parity: `BanIpCommands`, including its "this is a player, ban their address" form.
class BanIpCommand final : public Command {
public:
    explicit BanIpCommand(std::function<std::int64_t()> nowEpochSeconds);

    void build(const CommandNodeBuilder& root) const override;

private:
    std::function<std::int64_t()> nowEpochSeconds_;
};

/// parity: `PardonCommand` and `PardonIpCommand`.
class PardonCommand final : public Command {
public:
    explicit PardonCommand(bool ip);

    void build(const CommandNodeBuilder& root) const override;

private:
    bool ip_;
};

/// parity: `BanListCommands`.
class BanListCommand final : public Command {
public:
    BanListCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `WhitelistCommand`.
class WhitelistCommand final : public Command {
public:
    WhitelistCommand(std::function<bool()> enforced, std::function<void(bool)> setEnforced,
                     std::function<std::int64_t()> nowEpochSeconds);

    void build(const CommandNodeBuilder& root) const override;

private:
    std::function<bool()> enforced_;
    std::function<void(bool)> setEnforced_;
    std::function<std::int64_t()> nowEpochSeconds_;
};

} // namespace basalt::server::runtime::command::commands
