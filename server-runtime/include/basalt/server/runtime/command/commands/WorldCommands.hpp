#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// The commands that act on the world around the player rather than on the player.

/// parity: `MsgCommand`, with vanilla's own two keys: the sender sees
/// `commands.message.display.outgoing`, each recipient `commands.message.display.incoming`, and the
/// client italicises both.
class MsgCommand final : public Command {
public:
    MsgCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `WeatherCommand`. A form given no duration draws one from the same range the ordinary
/// weather cycle uses.
class WeatherCommand final : public Command {
public:
    WeatherCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `DifficultyCommand`, including its refusal when the world is already on that difficulty.
///
/// The value is this world's, not the server's, because that is where vanilla keeps it, and it
/// outlives a restart in `level.nbt` rather than in `worlds.toml`, which the operator owns.
class DifficultyCommand final : public Command {
public:
    DifficultyCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `SetWorldSpawnCommand`. The new spawn goes into `level.nbt`, so it outlives a restart.
class SetWorldSpawnCommand final : public Command {
public:
    SetWorldSpawnCommand();

    void build(const CommandNodeBuilder& root) const override;
};

} // namespace basalt::server::runtime::command::commands
