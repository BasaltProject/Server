#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `SetBlockCommand`.
///
/// `destroy` drops nothing yet: block drops go through the loot path, which needs the block entity
/// and tool context a command does not have. It clears the block like vanilla does and says nothing
/// else, which is the same visible result minus the item entity.
class SetBlockCommand final : public Command {
public:
    SetBlockCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `FillCommand`, including its five modes and the `replace <filter>` form.
///
/// The cap is the `max_block_modifications` game rule, read off the world the fill acts on.
class FillCommand final : public Command {
public:
    FillCommand();

    void build(const CommandNodeBuilder& root) const override;
};

} // namespace basalt::server::runtime::command::commands
