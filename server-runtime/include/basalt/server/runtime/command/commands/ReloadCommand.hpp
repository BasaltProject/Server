#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/command/commands/DataPackCommand.hpp>
#include <basalt/server/runtime/config/LiveConfigs.hpp>

#include <cstdint>

namespace basalt::server::runtime::command::commands {

/// parity: `ReloadCommand`: finds new data packs and reloads them.
class ReloadCommand final : public Command {
public:
    ReloadCommand(config::LiveConfigs& live, std::function<void()> onReloaded, DataPackControl packs = {},
                  config::ConfigDirectory directory = config::ConfigDirectory{});

    void build(const CommandNodeBuilder& root) const override;

private:
    config::LiveConfigs* live_;
    std::function<void()> onReloaded_;
    DataPackControl packs_;
    config::ConfigDirectory directory_;
};

} // namespace basalt::server::runtime::command::commands
