#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `OpCommand`, except for where membership lives.
///
/// Vanilla keeps operators in `ops.json`; Basalt keeps which group a player is in as a column in
/// `players.db`, because the server writes it and only the server writes it. `groups.toml` says
/// what the operator group is; this says who is in it.
class OpCommand final : public Command {
public:
    OpCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `DeOpCommands`. Puts them back in the default group rather than in no group at all.
class DeopCommand final : public Command {
public:
    DeopCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// `group <player> <group>`: the general form the other two are shortcuts for.
///
/// No vanilla counterpart, so no translation key. Without it the groups between `default` and
/// `operator` would be declarable in `groups.toml` and impossible to actually put anyone in.
class GroupCommand final : public Command {
public:
    GroupCommand();

    void build(const CommandNodeBuilder& root) const override;
};

} // namespace basalt::server::runtime::command::commands
