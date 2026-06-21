#include <basalt/server/plugins/PluginHookEventTypes.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/PlayerHookEvents.hpp>

namespace basalt::server::runtime::plugins::playerHooks {
namespace {

using server::plugins::PluginEvent;
using namespace server::plugins::pluginEventTypes;

[[nodiscard]] bool isClockStatistic(const std::string_view key) {
    return key.ends_with("play_time") || key.ends_with("total_world_time") || key.ends_with("time_since_death") ||
           key.ends_with("time_since_rest") || key.ends_with("sneak_time");
}

} // namespace

void install(player::PlayerSession& player, const EventSink& sink) {
    player.events = &sink;
    player.stats.setGate([&player, &sink](const std::string_view key, const std::int64_t amount) {
        if (isClockStatistic(key) || !sink.listening(&playerStatistic)) {
            return true;
        }
        return sink.allowed(&playerStatistic, [&](PluginEvent& event) {
            const std::int64_t previous = player.stats.get(key);
            EventSink::player(event, player);
            event.set("statistic", std::string{key});
            event.set("previous", previous);
            event.set("value", previous + amount);
        });
    });
}

} // namespace basalt::server::runtime::plugins::playerHooks
