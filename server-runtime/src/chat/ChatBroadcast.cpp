#include <basalt/server/runtime/chat/ChatBroadcast.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/scoreboard/TeamNameTags.hpp>
#include <basalt/server/runtime/scoreboard/Teams.hpp>
#include <basalt/wire/text/TranslationResolver.hpp>

#include <memory>
#include <string>

namespace basalt::server::runtime::chat {
namespace {

using wire::text::Component;

[[nodiscard]] Component withInsertionAndHover(const Component& component, const std::string& insertion,
                                              wire::text::HoverEvent hover) {
    wire::text::Style style = component.style();
    style.insertion = insertion;
    return component.withStyle(style).hoverEvent(std::move(hover));
}

} // namespace

Component displayName(const player::PlayerSession& player, const scoreboard::Teams* teams) {
    const scoreboard::Team* team = teams == nullptr ? nullptr : teams->teamOf(player.name());
    Component name = Component::text(player.name());
    if (team != nullptr) {
        // parity: `PlayerTeam.getFormattedName`.
        name = Component::empty().append(team->prefix).append(name).append(team->suffix);
        if (team->color.has_value()) {
            name = name.color(scoreboard::textColorOf(*team->color));
        }
    }
    const wire::text::HoverEvent hover{
        wire::text::HoverEvent::ShowEntity{.entityType = "minecraft:player",
                                           .uuid = player.uuid().toString(),
                                           .name = std::make_shared<const Component>(Component::text(player.name()))}};
    return withInsertionAndHover(name, player.name(), hover)
        .clickEvent(wire::text::ClickEvent::suggestCommand("/tell " + player.name() + " "));
}

Component formattedTeamName(const scoreboard::Team& team) {
    const Component inner = withInsertionAndHover(team.displayName, team.name,
                                                  wire::text::HoverEvent::showText(Component::text(team.name)));
    Component bracketed = Component::translatable("chat.square_brackets", {inner});
    if (team.color.has_value()) {
        bracketed = bracketed.color(scoreboard::textColorOf(*team.color));
    }
    return bracketed;
}

void logChatMessage(const PlayerChatMessage& message, const BoundChatType& chatType, const wire::common::Logger& logger,
                    const wire::text::LangTable& lang) {
    // parity: `verifyChatTrusted`, a signature that has not yet expired.
    const bool trusted = message.hasSignature() && !message.hasExpiredServer(nowMillis());
    logger.info([&] {
        const std::string line = wire::text::resolvePlainText(chatType.decorate(message.decoratedContent()), lang);
        return trusted ? line : "[Not Secure] " + line;
    });
}

void broadcastChatMessage(const std::vector<player::PlayerSession*>& players, const PlayerChatMessage& message,
                          const BoundChatType& chatType, const wire::common::Logger& logger,
                          const wire::text::LangTable& lang) {
    logChatMessage(message, chatType, logger, lang);
    for (const player::PlayerSession* player : players) {
        player->sendChatMessage(message, chatType);
    }
}

} // namespace basalt::server::runtime::chat
