#include <basalt/server/runtime/BuildInfo.hpp>
#include <basalt/server/runtime/chat/ChatBroadcast.hpp>
#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/MessageArguments.hpp>
#include <basalt/server/runtime/command/commands/BasicCommands.hpp>
#include <basalt/wire/multiversion/ProtocolVersions.hpp>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

using wire::text::Component;

/// Ranges rather than a list of fifty numbers: what an operator wants to read is the span.
[[nodiscard]] std::string describe(const std::set<std::int32_t>& protocols) {
    if (protocols.empty()) {
        return "none";
    }
    std::vector<wire::multiversion::ProtocolVersion> known;
    for (const wire::multiversion::ProtocolVersion& version : wire::multiversion::protocolVersions::known()) {
        if (protocols.contains(version.number)) {
            known.push_back(version);
        }
    }
    if (known.empty()) {
        std::string listed;
        for (const std::int32_t number : protocols) {
            if (!listed.empty()) {
                listed += ", ";
            }
            listed += std::to_string(number);
        }
        return listed;
    }
    const wire::multiversion::ProtocolVersion& oldest = known.front();
    const wire::multiversion::ProtocolVersion& newest = known.back();
    const std::string span =
        oldest == newest ? std::string{oldest.label} : std::string{oldest.label} + " to " + std::string{newest.label};
    return span + " (" + std::to_string(known.size()) + " protocol version" + (known.size() == 1 ? ")" : "s)");
}

} // namespace

SeedCommand::SeedCommand() : Command("seed", permissionLevel::kGamemasters) {}

void SeedCommand::build(const CommandNodeBuilder& root) const {
    root.executes([](const CommandContext& context) {
        const std::int64_t seed = context.environment().options().worldSeed;
        context.success(Component::translatable("commands.seed.success", {Component::text(std::to_string(seed))}));
        return Parsed<std::int32_t>{static_cast<std::int32_t>(seed)};
    });
}

StopCommand::StopCommand() : Command("stop", permissionLevel::kOwners) {}

void StopCommand::build(const CommandNodeBuilder& root) const {
    root.executes([](const CommandContext& context) {
        context.success(Component::translatable("commands.stop.stopping"), true);
        if (context.environment().options().stopServer) {
            context.environment().options().stopServer();
        }
        return Parsed<std::int32_t>{1};
    });
}

SayCommand::SayCommand() : Command("say", permissionLevel::kGamemasters) {}

void SayCommand::build(const CommandNodeBuilder& root) const {
    root.argument("message", messageArgument(), [](const CommandNodeBuilder& message) {
        message.executes([](const CommandContext& context) {
            const CommandEnvironment& environment = context.environment();
            chat::broadcastChatMessage(
                environment.players().snapshot(), messageArguments::resolveChatMessage(context, "message"),
                messageArguments::bind(chat::chatType::kSayCommand, context.source(), environment),
                environment.logger(), environment.lang());
            return Parsed<std::int32_t>{1};
        });
    });
}

EmoteCommand::EmoteCommand() : Command("me") {}

void EmoteCommand::build(const CommandNodeBuilder& root) const {
    root.argument("action", messageArgument(), [](const CommandNodeBuilder& action) {
        action.executes([](const CommandContext& context) {
            const CommandEnvironment& environment = context.environment();
            chat::broadcastChatMessage(
                environment.players().snapshot(), messageArguments::resolveChatMessage(context, "action"),
                messageArguments::bind(chat::chatType::kEmoteCommand, context.source(), environment),
                environment.logger(), environment.lang());
            return Parsed<std::int32_t>{1};
        });
    });
}

TellRawCommand::TellRawCommand() : Command("tellraw", permissionLevel::kGamemasters) {}

void TellRawCommand::build(const CommandNodeBuilder& root) const {
    root.argument("targets", playersArgument(false), [](const CommandNodeBuilder& targets) {
        targets.argument("message", componentArgument(), [](const CommandNodeBuilder& message) {
            message.executes([](const CommandContext& context) {
                const auto* players = context.get<std::vector<player::PlayerSession*>>("targets");
                const Component* text = context.get<Component>("message");
                if (players == nullptr || text == nullptr) {
                    return Parsed<std::int32_t>{0};
                }
                for (player::PlayerSession* target : *players) {
                    target->sendSystemMessage(*text);
                }
                return Parsed<std::int32_t>{static_cast<std::int32_t>(players->size())};
            });
        });
    });
}

KickCommand::KickCommand() : Command("kick", permissionLevel::kAdmins) {}

void KickCommand::build(const CommandNodeBuilder& root) const {
    const auto kick = [](const CommandContext& context, const Component& reason) {
        const auto* players = context.get<std::vector<player::PlayerSession*>>("targets");
        if (players == nullptr) {
            return Parsed<std::int32_t>{0};
        }
        for (player::PlayerSession* target : *players) {
            if (target->connection().disconnect) {
                target->connection().disconnect(reason);
            }
            context.success(Component::translatable("commands.kick.success", {Component::text(target->name()), reason}),
                            true);
        }
        return Parsed<std::int32_t>{static_cast<std::int32_t>(players->size())};
    };
    root.argument("targets", playersArgument(false), [kick](const CommandNodeBuilder& targets) {
        targets.executes([kick](const CommandContext& context) {
            return kick(context, Component::translatable("multiplayer.disconnect.kicked"));
        });
        targets.argument("reason", messageArgument(), [kick](const CommandNodeBuilder& reason) {
            reason.executes([kick](const CommandContext& context) {
                // parity: `MessageArgument.getMessage`, selectors and all.
                const std::string* text = context.get<std::string>("reason");
                return kick(context, messageArguments::resolveComponent(text == nullptr ? std::string{} : *text,
                                                                        context.source(), context.environment()));
            });
        });
    });
}

ListPlayersCommand::ListPlayersCommand() : Command("list") {}

void ListPlayersCommand::build(const CommandNodeBuilder& root) const {
    const auto list = [](const CommandContext& context, bool withIds) {
        const std::vector<player::PlayerSession*> players = context.environment().players().snapshot();
        std::vector<Component> named;
        named.reserve(players.size());
        for (const player::PlayerSession* player : players) {
            named.push_back(withIds ? Component::translatable(
                                          "commands.list.nameAndId",
                                          {Component::text(player->name()), Component::text(player->uuid().toString())})
                                    : Component::text(player->name()));
        }
        context.success(Component::translatable(
            "commands.list.players", {Component::text(std::to_string(players.size())),
                                      Component::text(std::to_string(context.environment().options().maxPlayers)),
                                      Component::join(Component::text(", "), named)}));
        return Parsed<std::int32_t>{static_cast<std::int32_t>(players.size())};
    };
    root.executes([list](const CommandContext& context) { return list(context, false); });
    root.literal("uuids", [list](const CommandNodeBuilder& uuids) {
        uuids.executes([list](const CommandContext& context) { return list(context, true); });
    });
}

VersionCommand::VersionCommand(AcceptedProtocols accepted)
    : Command("version", permissionLevel::kGamemasters), accepted_(std::move(accepted)) {}

void VersionCommand::build(const CommandNodeBuilder& root) const {
    root.executes([this](const CommandContext& context) {
        context.success(Component::text("Basalt " + std::string{buildInfo::kVersion} + " for Minecraft " +
                                        std::string{buildInfo::kMinecraftVersion} + " (protocol " +
                                        std::to_string(buildInfo::kProtocolVersion) + ")"));
        const std::set<std::int32_t> protocols =
            accepted_ ? accepted_() : std::set<std::int32_t>{buildInfo::kProtocolVersion};
        context.success(Component::text("  Clients accepted: " + describe(protocols), wire::text::kGray));
        return Parsed<std::int32_t>{1};
    });
}

} // namespace basalt::server::runtime::command::commands
