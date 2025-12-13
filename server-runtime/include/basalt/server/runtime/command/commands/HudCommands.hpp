#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/config/CommandsConfig.hpp>

namespace basalt::server::runtime::command::commands {

/// The commands that write on the player's screen, and the one an operator writes in a file.

/// parity: `TitleCommand`, all six forms.
///
/// `clear` and `reset` differ the way vanilla makes them differ: clearing hides whatever is
/// showing, resetting also puts the fade timings back to their defaults.
class TitleCommand final : public Command {
public:
    TitleCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `ExperienceCommand`, `/xp` alias included. Points is the default when neither literal is
/// given.
class ExperienceCommand final : public Command {
public:
    ExperienceCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `SetSpawnCommand`.
///
/// The point is stored per player in `players.db`, so it outlives both the session and the world it
/// names: a world that is no longer loaded when they die sends them to the default world's spawn,
/// the same fallback a stored position gets.
class SpawnPointCommand final : public Command {
public:
    SpawnPointCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// A command that exists because `commands.toml` said so: a name, a permission, and lines to answer.
/// The limit is the point: anything with a condition or an argument in it is a plugin, because a
/// configuration file that grows those becomes a programming language nobody chose to design.
///
/// Permission level zero, because the named permission decides who may run it and a higher level
/// would additionally hide it from clients that were granted the permission.
class ConfiguredCommand final : public Command {
public:
    explicit ConfiguredCommand(config::CustomCommand definition);

    void build(const CommandNodeBuilder& root) const override;

private:
    config::CustomCommand definition_;
};

} // namespace basalt::server::runtime::command::commands
