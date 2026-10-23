#pragma once

#include <basalt/server/runtime/plugins/EventSink.hpp>

namespace basalt::server::runtime::player {
class PlayerSession;
}

/// Events that hang off one player's own state rather than off a game system.
namespace basalt::server::runtime::plugins::playerHooks {

void install(player::PlayerSession& player, const EventSink& sink);

} // namespace basalt::server::runtime::plugins::playerHooks
