#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `TagCommand`.
class TagCommand final : public Command {
public:
    TagCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `SpreadPlayersCommand`.
class SpreadPlayersCommand final : public Command {
public:
    explicit SpreadPlayersCommand(std::function<double()> nextDouble);

    void build(const CommandNodeBuilder& root) const override;

private:
    std::function<double()> nextDouble_;
};

} // namespace basalt::server::runtime::command::commands
