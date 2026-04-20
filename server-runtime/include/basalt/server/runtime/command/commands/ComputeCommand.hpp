#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::world {
class RealLootTables;
}

namespace basalt::server::runtime::command::commands {

/// parity: 26.3 `ComputeCommand`.
class ComputeCommand final : public Command {
public:
    explicit ComputeCommand(world::RealLootTables* lootTables);

    void build(const CommandNodeBuilder& root) const override;

private:
    world::RealLootTables* lootTables_;
};

} // namespace basalt::server::runtime::command::commands
