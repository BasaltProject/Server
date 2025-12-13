#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `BanPlayerCommands`. A banned player is disconnected on the spot, as vanilla does.
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

/// parity: `PardonCommand` and `PardonIpCommand`, which refuse when there was nothing to lift.
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
///
/// Whether the whitelist is enforced is `server.toml`'s `whitelist`, so `on` and `off` report that
/// the file decides rather than silently disagreeing with it: an operator who turns it on here and
/// restarts would otherwise find it off again.
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
