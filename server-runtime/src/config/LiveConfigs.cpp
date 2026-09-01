#include <basalt/server/runtime/config/LiveConfigs.hpp>

#include <utility>

namespace basalt::server::runtime::config {

std::expected<ConfigSnapshot, std::string> LiveConfigs::read(const ConfigDirectory& directory,
                                                             const wire::common::Logger& logger) {
    std::expected<GroupsConfig, std::string> groups = GroupsConfig::load(directory, logger);
    if (!groups.has_value()) {
        return std::unexpected(groups.error());
    }
    std::expected<ServerListConfig, std::string> serverList = ServerListConfig::load(directory, std::nullopt, logger);
    if (!serverList.has_value()) {
        return std::unexpected(serverList.error());
    }
    std::expected<CommandsConfig, std::string> commands = CommandsConfig::load(directory, logger);
    if (!commands.has_value()) {
        return std::unexpected(commands.error());
    }
    std::expected<AnimationsConfig, std::string> animations = AnimationsConfig::load(directory, logger);
    if (!animations.has_value()) {
        return std::unexpected(animations.error());
    }
    std::expected<TabListConfig, std::string> tabList = TabListConfig::load(directory, logger);
    if (!tabList.has_value()) {
        return std::unexpected(tabList.error());
    }
    std::expected<ScoreboardConfig, std::string> scoreboard = ScoreboardConfig::load(directory, logger);
    if (!scoreboard.has_value()) {
        return std::unexpected(scoreboard.error());
    }
    std::expected<BossBarConfig, std::string> bossBar = BossBarConfig::load(directory, logger);
    if (!bossBar.has_value()) {
        return std::unexpected(bossBar.error());
    }
    return ConfigSnapshot{.groups = std::move(*groups),
                          .serverList = std::move(*serverList),
                          .commands = std::move(*commands),
                          .animations = std::move(*animations),
                          .ui = UiConfigs{.tabList = std::move(*tabList),
                                          .scoreboard = std::move(*scoreboard),
                                          .bossBar = std::move(*bossBar)}};
}

} // namespace basalt::server::runtime::config
