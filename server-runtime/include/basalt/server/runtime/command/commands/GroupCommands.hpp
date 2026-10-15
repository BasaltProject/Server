#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `OpCommand`, except for where membership lives.
class OpCommand final : public Command {
public:
    OpCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `DeOpCommands`.
class DeopCommand final : public Command {
public:
    DeopCommand();

    void build(const CommandNodeBuilder& root) const override;
};

class GroupCommand final : public Command {
public:
    GroupCommand();

    void build(const CommandNodeBuilder& root) const override;
};

} // namespace basalt::server::runtime::command::commands
