#pragma once

#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/server/runtime/command/CommandDispatcher.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace basalt::server::runtime::plugins {

/// `/plugins`, which is where an operator finds out what is loaded and takes something in or out
/// without stopping the server.
///
/// Not a vanilla command, so it lives here rather than among them, and it is the one place the three
/// hot operations are reachable from: plug in, reload, unplug.
class PluginsCommand final : public command::Command {
public:
    /// `databasePath` is where the player database lives, so a purge can copy it before it deletes
    /// anything.
    explicit PluginsCommand(server::plugins::PluginHost& host,
                            std::function<std::optional<std::string>()> databasePath = {});

    void build(const command::CommandNodeBuilder& root) const override;

private:
    [[nodiscard]] std::int32_t listOrphans(const command::CommandContext& context) const;

    /// `/plugins purge <id>` says what would go; `/plugins purge <id> confirm` does it.
    ///
    /// Two commands rather than a flag, and the harmless one is the short one, so nobody deletes a
    /// thousand players' rows by mistyping. The database is copied first: the only thing worse than
    /// losing the data is losing it with nothing to go back to.
    [[nodiscard]] std::int32_t purge(const command::CommandContext& context, bool dryRun) const;

    /// Copies the database beside itself, named for what is about to happen to it.
    [[nodiscard]] std::optional<std::string> backup() const;

    server::plugins::PluginHost* host_;
    std::function<std::optional<std::string>()> databasePath_;
};

} // namespace basalt::server::runtime::plugins
