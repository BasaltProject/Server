#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `SetBlockCommand`.
class SetBlockCommand final : public Command {
public:
    SetBlockCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `FillCommand`, including its five modes and the `replace <filter>` form.
class FillCommand final : public Command {
public:
    FillCommand();

    void build(const CommandNodeBuilder& root) const override;
};

} // namespace basalt::server::runtime::command::commands
