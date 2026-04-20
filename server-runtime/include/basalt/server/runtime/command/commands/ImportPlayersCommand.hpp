#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

#include <cstdint>
#include <functional>

namespace basalt::server::runtime::command::commands {

class ImportPlayersCommand final : public Command {
public:
    explicit ImportPlayersCommand(std::function<std::int64_t()> nowEpochSeconds);

    void build(const CommandNodeBuilder& root) const override;

private:
    std::function<std::int64_t()> nowEpochSeconds_;
};

} // namespace basalt::server::runtime::command::commands
