#include <basalt/server/runtime/BasaltServer.hpp>
#include <basalt/server/runtime/BuildInfo.hpp>
#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/command/CommandSource.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/text/MiniMessage.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <string>

namespace basalt::server::runtime {

void BasaltServer::runStartupCommands() {
    if (config_->tuning.startupCommands.empty() || consoleSource_ == nullptr) {
        return;
    }
    // Queued on the owner so they run on the tick thread, after the worlds are up.
    worldRegistry_->scheduler().global().submit([this] {
        command::CommandDispatcher* dispatcher = wired_.dispatcher.get();
        if (dispatcher == nullptr) {
            return;
        }
        for (const std::string& line : config_->tuning.startupCommands) {
            logger_.info([&line] { return "Running startup command: " + line; });
            static_cast<void>(dispatcher->execute(*consoleSource_, line.starts_with('/') ? line.substr(1) : line));
        }
    });
}

void BasaltServer::startQuery() {
    if (!config_->tuning.queryEnabled) {
        return;
    }
    query_ = net::QueryServer::start(
        static_cast<std::uint16_t>(config_->tuning.queryPort),
        [this] {
            net::QueryServer::Info info;
            const std::shared_ptr<const config::ConfigSnapshot> snapshot = live_->current();
            info.serverName = "A Basalt Server";
            if (const auto& motd = snapshot->serverList.defaultProfile().motd; motd.has_value() && !motd->empty()) {
                info.serverName = wire::text::toPlainText(wire::text::miniMessage::parse(motd->front()));
            }
            info.worldName = worldRegistry_->defaultWorld().name();
            info.version = std::string{buildInfo::kMinecraftVersion};
            info.hostIp = config_->javaBind;
            info.serverPort = config_->port;
            info.maxPlayers = config_->maxPlayers;
            for (const player::PlayerSession* player : playerRegistry_.snapshot()) {
                info.players.push_back(player->name());
            }
            return info;
        },
        logger_);
}

} // namespace basalt::server::runtime
