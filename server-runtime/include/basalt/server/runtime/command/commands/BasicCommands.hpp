#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

#include <cstdint>
#include <functional>
#include <set>

namespace basalt::server::runtime::command::commands {

/// parity: `SeedCommand`, which on a dedicated server needs game-master level.
class SeedCommand final : public Command {
public:
    SeedCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `StopCommand`.
class StopCommand final : public Command {
public:
    StopCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `SayCommand`.
class SayCommand final : public Command {
public:
    SayCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `EmoteCommands`, the `emote_command` chat type (`chat.type.emote`).
class EmoteCommand final : public Command {
public:
    EmoteCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `TellRawCommand`.
class TellRawCommand final : public Command {
public:
    TellRawCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `KickCommand`, whose default reason is `multiplayer.disconnect.kicked`.
class KickCommand final : public Command {
public:
    KickCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `ListPlayersCommand`.
class ListPlayersCommand final : public Command {
public:
    ListPlayersCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `VersionCommand` in shape only.
class VersionCommand final : public Command {
public:
    using AcceptedProtocols = std::function<std::set<std::int32_t>()>;

    explicit VersionCommand(AcceptedProtocols accepted = {});

    void build(const CommandNodeBuilder& root) const override;

private:
    AcceptedProtocols accepted_;
};

} // namespace basalt::server::runtime::command::commands
