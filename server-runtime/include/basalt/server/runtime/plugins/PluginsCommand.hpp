#pragma once

#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/server/runtime/bukkit/JavaPluginScan.hpp>
#include <basalt/server/runtime/command/CommandDispatcher.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::plugins {

class PluginsCommand final : public command::Command {
public:
    explicit PluginsCommand(server::plugins::PluginHost& host,
                            std::function<std::optional<std::string>()> databasePath = {},
                            std::function<std::vector<bukkit::JavaPluginJar>()> javaPlugins = {});

    void build(const command::CommandNodeBuilder& root) const override;

private:
    [[nodiscard]] std::int32_t listOrphans(const command::CommandContext& context) const;

    [[nodiscard]] std::int32_t purge(const command::CommandContext& context, bool dryRun) const;

    [[nodiscard]] std::optional<std::string> backup() const;

    server::plugins::PluginHost* host_;
    std::function<std::optional<std::string>()> databasePath_;
    std::function<std::vector<bukkit::JavaPluginJar>()> javaPlugins_;
};

} // namespace basalt::server::runtime::plugins
