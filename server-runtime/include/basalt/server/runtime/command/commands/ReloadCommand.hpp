#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/config/LiveConfigs.hpp>

#include <cstdint>

namespace basalt::server::runtime::command::commands {

/// `/reload`: re-reads the configuration files that are read on every use. Bukkit's version is
/// distrusted because it reloads code while the server holds references to what it threw away.
///
/// Here the line is a property of the file: what is already a bound socket, a thread pool or a loaded
/// world is refused by name, and every file is read before any is applied so nothing is half-done.
class ReloadCommand final : public Command {
public:
    /// `reloadFunctions` re-reads the datapack functions vanilla's own `/reload` does, and answers
    /// how many were loaded.
    ReloadCommand(config::LiveConfigs& live, std::function<void()> onReloaded,
                  std::function<std::int32_t()> reloadFunctions = {},
                  config::ConfigDirectory directory = config::ConfigDirectory{});

    void build(const CommandNodeBuilder& root) const override;

private:
    config::LiveConfigs* live_;
    std::function<void()> onReloaded_;
    std::function<std::int32_t()> reloadFunctions_;
    config::ConfigDirectory directory_;
};

} // namespace basalt::server::runtime::command::commands
