#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/EventSink.hpp>

namespace basalt::server::runtime::plugins {

void EventSink::player(server::plugins::PluginEvent& event, const player::PlayerSession& who) {
    event.set("player_name", who.name());
    event.set("player_uuid", who.uuid().toString());
    event.set("world", who.worldName);
}

} // namespace basalt::server::runtime::plugins
