#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/command/commands/DataPackCommand.hpp>
#include <basalt/server/runtime/config/LiveConfigs.hpp>

#include <cstdint>

namespace basalt::server::runtime::command::commands {

/// parity: `ReloadCommand`: finds new data packs and reloads them. It also re-reads the configuration
/// files that are read on every use; what is already a bound socket, a thread pool or a loaded world
/// is not, and every file is read before any is applied so nothing is half-done.
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
