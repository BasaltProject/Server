#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `WorldBorderCommand`.
///
/// `damage` is stored and reported but nothing is hurt by it yet: damage outside the border belongs
/// to the per-tick player pass, not here.
class WorldBorderCommand final : public Command {
public:
    WorldBorderCommand();

    void build(const CommandNodeBuilder& root) const override;
};

} // namespace basalt::server::runtime::command::commands
