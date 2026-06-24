#pragma once

#include <basalt/server/plugins/PluginAccess.hpp>
#include <basalt/server/runtime/config/GroupsConfig.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/storage/PlayerStore.hpp>

#include <functional>
#include <vector>

namespace basalt::server::runtime::plugins {

/// The player database, groups and ban lists as the plugin API sees them.
class ServerAccess final : public server::plugins::PluginAccess {
public:
    struct Hooks {
        std::function<storage::PlayerStore*()> store;
        std::function<const config::GroupsConfig&()> groups;
        std::function<std::vector<player::PlayerSession*>()> sessions;
        std::function<bool()> whitelistEnforced;
        std::function<void(bool)> setWhitelistEnforced;
    };

    explicit ServerAccess(Hooks hooks) : hooks_(std::move(hooks)) {}

    [[nodiscard]] std::optional<server::plugins::PluginKnownPlayer> playerByUuid(std::string_view uuid) override;
    [[nodiscard]] std::optional<server::plugins::PluginKnownPlayer> playerByName(std::string_view name) override;
    [[nodiscard]] std::vector<server::plugins::PluginKnownPlayer> knownPlayers() override;
    [[nodiscard]] std::optional<server::plugins::PluginKnownPlayer> spawnPoint(std::string_view uuid) override;
    [[nodiscard]] std::int64_t statistic(std::string_view uuid, std::string_view key) override;
    bool setStatistic(std::string_view uuid, std::string_view key, std::int64_t value) override;
    [[nodiscard]] std::optional<std::array<std::string, 4>> lastDeath(std::string_view uuid) override;
    [[nodiscard]] bool isOperator(std::string_view uuid) override;
    bool setOperator(std::string_view uuid, bool value) override;
    [[nodiscard]] std::vector<server::plugins::PluginKnownPlayer> operators() override;
    [[nodiscard]] bool isWhitelisted(std::string_view uuid) override;
    void setWhitelisted(std::string_view uuid, std::string_view name, bool value) override;
    [[nodiscard]] std::vector<server::plugins::PluginKnownPlayer> whitelisted() override;
    [[nodiscard]] bool whitelistEnforced() override;
    void setWhitelistEnforced(bool value) override;
    [[nodiscard]] std::optional<server::plugins::PluginBan> banOf(std::string_view subject, bool ip) override;
    [[nodiscard]] std::vector<server::plugins::PluginBan> bans(bool ip) override;
    void ban(const server::plugins::PluginBan& record, bool ip) override;
    bool pardon(std::string_view subject, bool ip) override;

private:
    [[nodiscard]] storage::PlayerStore* open() const;
    [[nodiscard]] std::optional<server::plugins::PluginKnownPlayer> online(std::string_view uuid,
                                                                           std::string_view name) const;

    Hooks hooks_;
};

} // namespace basalt::server::runtime::plugins
