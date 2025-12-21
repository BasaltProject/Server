#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/command/CommandStorage.hpp>
#include <basalt/server/runtime/command/commands/BossBarCommand.hpp>
#include <basalt/server/runtime/command/commands/FunctionCommands.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `ExecuteCommand`, declared as the tree vanilla declares rather than as one greedy
/// string.
///
/// Each link redirects back onto `/execute` with a modifier, so the client parses and completes the
/// whole chain and every link keeps its own arguments. `run` redirects onto the dispatcher root,
/// which is what makes the rest of the line a command in its own right.
///
/// Declared but inert, each for want of the system underneath: `on` (no entity relations), `if
/// predicate`, `if items` and `if stopwatch`. They parse and fail rather than being missing from
/// the tree, so a line that uses one reads as false instead of as a syntax error.
/// What only a running server owns. Each is optional, and the clause that needs it simply answers
/// false or writes nowhere when it is absent.
///
/// At namespace scope rather than nested: a nested struct cannot supply the default member
/// initializers the enclosing class's own default argument needs.
struct ExecuteCommandHooks {
    /// Where `store ... bossbar` writes.
    BossBarRegistry* bossBars = nullptr;
    /// Where `store ... storage` writes and `if data` reads.
    const DataAccess* data = nullptr;
    /// What `if function` runs.
    FunctionRunner* functions = nullptr;
    /// Where `summon` gets an entity id.
    player::PlayerRegistry* players = nullptr;
};

class ExecuteCommand final : public Command {
public:
    using Hooks = ExecuteCommandHooks;

    ExecuteCommand(scoreboard::Scoreboard& scoreboard, Hooks hooks = {});

    void build(const CommandNodeBuilder& root) const override;

private:
    /// The clauses that only change the source: who, where, facing what, in which world.
    void buildContextClauses(const CommandNodeBuilder& root, CommandNode& execute) const;

    /// `if` and `unless`, which are the same tree read two ways.
    void buildConditions(const CommandNodeBuilder& builder, CommandNode& execute, bool positive) const;

    /// `store result` and `store success`, which are the same tree over two numbers.
    void buildStores(const CommandNodeBuilder& builder, CommandNode& execute, bool fromSuccess) const;

    scoreboard::Scoreboard* scoreboard_;
    Hooks hooks_;
};

} // namespace basalt::server::runtime::command::commands
