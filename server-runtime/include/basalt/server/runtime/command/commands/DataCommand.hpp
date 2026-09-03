#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/command/CommandStorage.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `DataCommands`, over the three holders this server has NBT for: command storage, a block
/// entity, and a player.
///
/// A player is a read-only holder: their NBT is a view built from live session state, so a write
/// would land on a copy and quietly do nothing.
class DataCommand final : public Command {
public:
    explicit DataCommand(CommandStorage& storage);

    void build(const CommandNodeBuilder& root) const override;

private:
    /// The `modify` verb, which is five operations over three source shapes and lives in a file of
    /// its own for the same reason `/scoreboard` does.
    void buildModify(const CommandNodeBuilder& root) const;

    CommandStorage* storage_;
};

} // namespace basalt::server::runtime::command::commands
