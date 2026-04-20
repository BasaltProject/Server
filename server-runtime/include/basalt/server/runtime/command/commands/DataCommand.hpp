#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/command/CommandStorage.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `DataCommands`.
class DataCommand final : public Command {
public:
    explicit DataCommand(CommandStorage& storage);

    void build(const CommandNodeBuilder& root) const override;

private:
    void buildModify(const CommandNodeBuilder& root) const;

    CommandStorage* storage_;
};

} // namespace basalt::server::runtime::command::commands
