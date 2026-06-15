#pragma once

#include <basalt/server/plugins/PluginBossBars.hpp>
#include <basalt/server/runtime/command/commands/BossBarCommand.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace basalt::server::runtime::plugins {

class ServerBossBars final : public server::plugins::PluginBossBars {
public:
    struct Hooks {
        std::function<std::vector<player::PlayerSession*>()> sessions;
        command::commands::BossBarRegistry* registry = nullptr;
    };

    explicit ServerBossBars(Hooks hooks) : hooks_(std::move(hooks)) {}

    std::optional<std::string> create(const server::plugins::PluginBossBar& bar) override;
    [[nodiscard]] std::optional<server::plugins::PluginBossBar> get(std::string_view id) override;
    [[nodiscard]] std::vector<std::string> kept() override;
    bool update(const server::plugins::PluginBossBar& bar) override;
    bool remove(std::string_view id) override;
    bool addPlayer(std::string_view id, std::string_view uuid) override;
    bool removePlayer(std::string_view id, std::string_view uuid) override;
    void clearPlayers(std::string_view id) override;
    [[nodiscard]] std::vector<std::string> players(std::string_view id) override;

private:
    struct Transient {
        wire::common::Uuid uuid = wire::common::Uuid::random();
        server::plugins::PluginBossBar bar;
        std::set<std::string> players;
    };

    [[nodiscard]] std::vector<player::PlayerSession*> watching(std::string_view id) const;
    void sendTo(const player::PlayerSession& player, const wire::common::Uuid& barId,
                const wire::protocol::play::BossEventPacket::Operation& operation) const;
    [[nodiscard]] wire::common::Uuid uuidOf(std::string_view id) const;

    Hooks hooks_;
    std::map<std::string, Transient, std::less<>> transient_;
};

} // namespace basalt::server::runtime::plugins
