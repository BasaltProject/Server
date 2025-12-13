#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

#include <cstdint>
#include <functional>
#include <set>

namespace basalt::server::runtime::command::commands {

/// The commands whose whole body is one message and no state of their own.
///
/// Vanilla gives each of these a class under `net.minecraft.server.commands`, and so does this
/// build; they share a file because a header per two-line command is ceremony without a reader.
/// Each still says which vanilla command it reimplements and asks for the same permission level.

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

/// parity: `SayCommand`, whose output is the `say_command` chat type, rendered by
/// `chat.type.announcement`.
class SayCommand final : public Command {
public:
    SayCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `EmoteCommands`, the `emote_command` chat type (`chat.type.emote`). Available to
/// everyone.
class EmoteCommand final : public Command {
public:
    EmoteCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `TellRawCommand`. The message is a component, so styles and events survive to the
/// client.
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

/// parity: `ListPlayersCommand`. Available to everyone, as it is in vanilla.
class ListPlayersCommand final : public Command {
public:
    ListPlayersCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `VersionCommand` in shape only. Vanilla prints its data pack version and feature flags,
/// which do not exist here; this prints what this build actually is, which is the useful answer.
class VersionCommand final : public Command {
public:
    /// Every protocol this server lets in right now, the native one included.
    using AcceptedProtocols = std::function<std::set<std::int32_t>()>;

    explicit VersionCommand(AcceptedProtocols accepted = {});

    void build(const CommandNodeBuilder& root) const override;

private:
    AcceptedProtocols accepted_;
};

} // namespace basalt::server::runtime::command::commands
