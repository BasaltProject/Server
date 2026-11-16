#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/config/CommandsConfig.hpp>

namespace basalt::server::runtime::command::commands {

class CustomMessageCommand final : public Command {
public:
    explicit CustomMessageCommand(const config::CustomCommand& declared);

    void build(const CommandNodeBuilder& root) const override;

private:
    std::vector<std::string> lines_;
};

} // namespace basalt::server::runtime::command::commands
