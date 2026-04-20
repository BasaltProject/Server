#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `EffectCommand`.
class EffectCommand final : public Command {
public:
    EffectCommand();

    void build(const CommandNodeBuilder& root) const override;
};

class WorldCommand final : public Command {
public:
    WorldCommand();

    void build(const CommandNodeBuilder& root) const override;
};

} // namespace basalt::server::runtime::command::commands
