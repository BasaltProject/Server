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

namespace basalt::server::runtime::chat {

/// parity: `Player.getDisplayName`.
[[nodiscard]] wire::text::Component displayName(const player::PlayerSession& player, const scoreboard::Teams* teams);

/// parity: `PlayerTeam.getFormattedDisplayName`.
[[nodiscard]] wire::text::Component formattedTeamName(const scoreboard::Team& team);

/// parity: `PlayerList.broadcastChatMessage`.
void broadcastChatMessage(const std::vector<player::PlayerSession*>& players, const PlayerChatMessage& message,
                          const BoundChatType& chatType, const wire::common::Logger& logger,
                          const wire::text::LangTable& lang);

/// parity: `MinecraftServer.logChatMessage`.
void logChatMessage(const PlayerChatMessage& message, const BoundChatType& chatType, const wire::common::Logger& logger,
                    const wire::text::LangTable& lang);

} // namespace basalt::server::runtime::chat
