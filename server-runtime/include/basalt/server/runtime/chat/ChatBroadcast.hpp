#pragma once

#include <basalt/server/runtime/chat/ChatMessage.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/text/Component.hpp>
#include <basalt/wire/text/LangTable.hpp>

#include <vector>

namespace basalt::server::runtime::player {
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::scoreboard {
class Teams;
struct Team;
} // namespace basalt::server::runtime::scoreboard

/// Where a chat line goes once it has been accepted.
namespace basalt::server::runtime::chat {

/// parity: `Player.getDisplayName`: the name inside its team's prefix, suffix and colour, clicking
/// to `/tell` them, hovering to show the player, and inserting the name on shift-click.
[[nodiscard]] wire::text::Component displayName(const player::PlayerSession& player, const scoreboard::Teams* teams);

/// parity: `PlayerTeam.getFormattedDisplayName`, the display name in square brackets and colour.
[[nodiscard]] wire::text::Component formattedTeamName(const scoreboard::Team& team);

/// parity: `PlayerList.broadcastChatMessage`: logged, marked "Not Secure" when it carries no live
/// signature, and sent to everyone.
void broadcastChatMessage(const std::vector<player::PlayerSession*>& players, const PlayerChatMessage& message,
                          const BoundChatType& chatType, const wire::common::Logger& logger,
                          const wire::text::LangTable& lang);

/// parity: `MinecraftServer.logChatMessage`.
void logChatMessage(const PlayerChatMessage& message, const BoundChatType& chatType, const wire::common::Logger& logger,
                    const wire::text::LangTable& lang);

} // namespace basalt::server::runtime::chat
