#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/command/CommandStorage.hpp>
#include <basalt/server/runtime/command/commands/BossBarCommand.hpp>
#include <basalt/server/runtime/command/commands/FunctionCommands.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `ExecuteCommand`.
struct ExecuteCommandHooks {
    BossBarRegistry* bossBars = nullptr;
    const DataAccess* data = nullptr;
    FunctionRunner* functions = nullptr;
    player::PlayerRegistry* players = nullptr;
};

class ExecuteCommand final : public Command {
public:
    using Hooks = ExecuteCommandHooks;

    ExecuteCommand(scoreboard::Scoreboard& scoreboard, Hooks hooks = {});

    void build(const CommandNodeBuilder& root) const override;

private:
    void buildContextClauses(const CommandNodeBuilder& root, CommandNode& execute) const;

    void buildConditions(const CommandNodeBuilder& builder, CommandNode& execute, bool positive) const;

    void buildStores(const CommandNodeBuilder& builder, CommandNode& execute, bool fromSuccess) const;

    scoreboard::Scoreboard* scoreboard_;
    Hooks hooks_;
};

} // namespace basalt::server::runtime::command::commands
