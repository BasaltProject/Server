#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::world {
class RealLootTables;
}

namespace basalt::server::runtime::command::commands {

/// parity: 26.3 `ComputeCommand`: works a number provider out in a loot context, `default`, at a
/// `block` or about an `entity`, and prints and answers it. The target entity of the `entity` form
/// is not part of the context here: no provider this build models reads it.
class ComputeCommand final : public Command {
public:
    explicit ComputeCommand(world::RealLootTables* lootTables);

    void build(const CommandNodeBuilder& root) const override;

private:
    world::RealLootTables* lootTables_;
};

} // namespace basalt::server::runtime::command::commands
