#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `CloneCommands`, minus its block entity copying.
class CloneCommand final : public Command {
public:
    CloneCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `FillBiomeCommand`.
class FillBiomeCommand final : public Command {
public:
    FillBiomeCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `ForceLoadCommand`.
class ForceLoadCommand final : public Command {
public:
    ForceLoadCommand();

    void build(const CommandNodeBuilder& root) const override;
};

} // namespace basalt::server::runtime::command::commands
