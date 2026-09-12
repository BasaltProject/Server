#pragma once

#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/text/Component.hpp>
#include <basalt/wire/text/LangTable.hpp>

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime::player {

class PlayerSession;
class PlayerRegistry;

/// What dying needs of the server, without needing the connection it happened on. The state, the
/// sentence and who hears it are the same everywhere; only the screen is the edition's own business
/// and is a callback. Apart from `CombatHandler` so a player with no `ClientSession` can still die.
struct DeathContext {
    PlayerRegistry* registry = nullptr;
    wire::common::Logger logger{"Server"};
    wire::text::LangTable lang;
    /// What a plugin is told, with the sentence already resolved to plain text.
    std::function<void(const PlayerSession& player, const std::string& deathMessage)> died;
};

/// Kills a player who is not already dead: the state, the statistics, the sentence, the log line,
/// and everyone told. Does nothing to somebody already down.
void killPlayer(PlayerSession& session, std::string_view deathMessageId,
                const std::optional<wire::text::Component>& killer, const DeathContext& context);

} // namespace basalt::server::runtime::player
