#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/commands/SessionCommands.hpp>
#include <basalt/wire/protocol/play/BorderPackets.hpp>
#include <basalt/wire/text/TextColor.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

using wire::text::Component;

/// parity: `TransferCommand`'s own default, which is the port a client assumes too.
constexpr std::int32_t kDefaultPort = 25565;

void sendTo(const player::PlayerSession& player, std::int32_t packetId,
            const std::function<void(wire::io::ByteBuffer&)>& write) {
    if (player.connection().sendPlayPacket) {
        player.connection().sendPlayPacket(packetId, write);
    }
}

[[nodiscard]] Parsed<std::int32_t> spectate(const CommandContext& context, player::PlayerSession* target,
                                            player::PlayerSession& spectator) {
    if (target != nullptr && target->uuid() == spectator.uuid()) {
        return std::unexpected{CommandSyntaxError{.component = Component::translatable("commands.spectate.self")}};
    }
    if (spectator.gamemode != wire::protocol::play::Gamemode::Spectator) {
        return std::unexpected{
            CommandSyntaxError{.component = Component::translatable("commands.spectate.not_spectator",
                                                                    {Component::text(spectator.name())})}};
    }
    // Their own entity id puts the camera back, which is how vanilla stops spectating.
    if (spectator.connection().setCamera) {
        spectator.connection().setCamera(target != nullptr ? target->entityId() : spectator.entityId());
    }
    context.success(target != nullptr ? Component::translatable("commands.spectate.success.started",
                                                                {Component::text(target->name())})
                                      : Component::translatable("commands.spectate.success.stopped"));
    return 1;
}

/// The one player a `single = true` selector matched, or null when it matched nobody.
[[nodiscard]] player::PlayerSession* only(const CommandContext& context, std::string_view name) {
    const auto* matched = context.get<std::vector<player::PlayerSession*>>(name);
    return matched == nullptr || matched->size() != 1 ? nullptr : matched->front();
}

} // namespace

SetIdleTimeoutCommand::SetIdleTimeoutCommand(std::function<void(std::int32_t)> setTimeoutMinutes)
    : Command("setidletimeout", permissionLevel::kAdmins), setTimeoutMinutes_(std::move(setTimeoutMinutes)) {}

void SetIdleTimeoutCommand::build(const CommandNodeBuilder& root) const {
    root.argument("minutes", integerArgument(0), [this](const CommandNodeBuilder& minutes) {
        minutes.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
            const auto* value = context.get<std::int32_t>("minutes");
            if (value == nullptr) {
                return 0;
            }
            if (setTimeoutMinutes_) {
                setTimeoutMinutes_(*value);
            }
            context.success(*value > 0 ? Component::translatable("commands.setidletimeout.success",
                                                                 {Component::text(std::to_string(*value))})
                                       : Component::translatable("commands.setidletimeout.success.disabled"),
                            true);
            return *value;
        });
    });
}

TeamMsgCommand::TeamMsgCommand(scoreboard::Teams& teams)
    : Command("teammsg", permissionLevel::kAll, {"tm"}), teams_(&teams) {}

void TeamMsgCommand::build(const CommandNodeBuilder& root) const {
    root.argument("message", messageArgument(), [this](const CommandNodeBuilder& message) {
        message.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
            const Parsed<player::PlayerSession*> player = context.requirePlayer();
            if (!player.has_value()) {
                return std::unexpected{player.error()};
            }
            const scoreboard::Team* team = teams_->teamOf((*player)->name());
            if (team == nullptr) {
                return std::unexpected{
                    CommandSyntaxError{.component = Component::translatable("commands.teammsg.failed.noteam")}};
            }
            const auto* text = context.get<std::string>("message");
            const Component body = Component::text(text == nullptr ? std::string{} : *text);
            const Component line = Component::translatable(
                "chat.type.team.text", {team->displayName, Component::text((*player)->name()), body});
            for (player::PlayerSession* member : context.environment().players().snapshot()) {
                if (std::ranges::find(team->members, member->name()) != team->members.end()) {
                    member->sendSystemMessage(line);
                }
            }
            context.environment().logger().info([&] {
                return "[Team " + team->name + "] <" + (*player)->name() + "> " +
                       (text == nullptr ? std::string{} : *text);
            });
            return static_cast<std::int32_t>(team->members.size());
        });
    });
}

RestartCommand::RestartCommand(std::function<void()> requestRestart)
    : Command("restart", permissionLevel::kAdmins), requestRestart_(std::move(requestRestart)) {}

void RestartCommand::build(const CommandNodeBuilder& root) const {
    root.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
        const std::size_t online = context.environment().players().snapshot().size();
        context.success(Component::text(online == 0 ? std::string{"Restarting."}
                                                    : "Restarting: " + std::to_string(online) +
                                                          " player(s) will be disconnected and can reconnect in a "
                                                          "moment."),
                        true);
        context.environment().broadcast(
            Component::text("The server is restarting. You can reconnect in a moment.", wire::text::kYellow));
        // After the messages, since the first thing a restart does is close the socket they would
        // have gone out on.
        if (requestRestart_) {
            requestRestart_();
        }
        return 1;
    });
}

SpectateCommand::SpectateCommand() : Command("spectate", permissionLevel::kGamemasters) {}

void SpectateCommand::build(const CommandNodeBuilder& root) const {
    root.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
        const Parsed<player::PlayerSession*> self = context.requirePlayer();
        if (!self.has_value()) {
            return std::unexpected{self.error()};
        }
        return spectate(context, nullptr, **self);
    });
    root.argument("target", playersArgument(true), [](const CommandNodeBuilder& target) {
        target.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
            const Parsed<player::PlayerSession*> self = context.requirePlayer();
            if (!self.has_value()) {
                return std::unexpected{self.error()};
            }
            player::PlayerSession* watched = only(context, "target");
            return watched == nullptr ? 0 : spectate(context, watched, **self);
        });
        target.argument("player", playersArgument(true), [](const CommandNodeBuilder& watcher) {
            watcher.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                player::PlayerSession* watched = only(context, "target");
                player::PlayerSession* spectator = only(context, "player");
                return watched == nullptr || spectator == nullptr ? 0 : spectate(context, watched, *spectator);
            });
        });
    });
}

TransferCommand::TransferCommand() : Command("transfer", permissionLevel::kAdmins) {}

namespace {

[[nodiscard]] Parsed<std::int32_t> transfer(const CommandContext& context, std::int32_t port,
                                            const std::vector<player::PlayerSession*>& players) {
    if (players.empty()) {
        return std::unexpected{
            CommandSyntaxError{.component = Component::translatable("commands.transfer.error.no_players")}};
    }
    const auto* host = context.get<std::string>("hostname");
    const wire::protocol::play::TransferPacket packet{.host = host == nullptr ? std::string{} : *host, .port = port};
    for (const player::PlayerSession* player : players) {
        if (player->connection().transferTo) {
            player->connection().transferTo(packet.host, port);
            continue;
        }
        sendTo(*player, wire::protocol::play::TransferPacket::kId,
               [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    }
    const Component where = Component::text(packet.host);
    const Component which = Component::text(std::to_string(port));
    context.success(players.size() == 1
                        ? Component::translatable("commands.transfer.success.single",
                                                  {Component::text(players.front()->name()), where, which})
                        : Component::translatable("commands.transfer.success.multiple",
                                                  {Component::text(std::to_string(players.size())), where, which}),
                    true);
    return static_cast<std::int32_t>(players.size());
}

} // namespace

void TransferCommand::build(const CommandNodeBuilder& root) const {
    root.argument("hostname", wordArgument(), [](const CommandNodeBuilder& hostname) {
        hostname.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
            const Parsed<player::PlayerSession*> self = context.requirePlayer();
            if (!self.has_value()) {
                return std::unexpected{self.error()};
            }
            return transfer(context, kDefaultPort, {*self});
        });
        hostname.argument("port", integerArgument(1, 65535), [](const CommandNodeBuilder& port) {
            port.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                const Parsed<player::PlayerSession*> self = context.requirePlayer();
                if (!self.has_value()) {
                    return std::unexpected{self.error()};
                }
                const auto* number = context.get<std::int32_t>("port");
                return transfer(context, number == nullptr ? kDefaultPort : *number, {*self});
            });
            port.argument("players", playersArgument(false), [](const CommandNodeBuilder& players) {
                players.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                    const auto* matched = context.get<std::vector<player::PlayerSession*>>("players");
                    const auto* number = context.get<std::int32_t>("port");
                    return transfer(context, number == nullptr ? kDefaultPort : *number,
                                    matched == nullptr ? std::vector<player::PlayerSession*>{} : *matched);
                });
            });
        });
    });
}

} // namespace basalt::server::runtime::command::commands
