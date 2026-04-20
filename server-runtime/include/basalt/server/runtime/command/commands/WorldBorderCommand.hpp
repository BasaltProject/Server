#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `WorldBorderCommand`.
class WorldBorderCommand final : public Command {
public:
    WorldBorderCommand();

    void build(const CommandNodeBuilder& root) const override;
};

} // namespace basalt::server::runtime::command::commands
