#include <basalt/server/plugins/PluginHookEventTypes.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/player/PlayerCooldowns.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/EventSink.hpp>

#include <string>

namespace basalt::server::runtime::player {

bool onCooldownFor(PlayerSession& session, const wire::common::Identifier& item, const std::int64_t gameTime) {
    const auto found = session.itemCooldowns.find(item.path());
    if (found == session.itemCooldowns.end()) {
        return false;
    }
    if (found->second > gameTime) {
        return true;
    }
    session.itemCooldowns.erase(found);
    return false;
}

void startCooldownFor(PlayerSession& session, const wire::common::Identifier& item, const std::int64_t gameTime) {
    startCooldownFor(session, item, gameTime, server::registry::itemStacking::cooldownTicks(item));
}

void startCooldownFor(PlayerSession& session, const wire::common::Identifier& item, const std::int64_t gameTime,
                      const std::int32_t ticks) {
    if (ticks <= 0) {
        return;
    }
    std::int32_t length = ticks;
    if (session.events != nullptr) {
        const server::plugins::PluginEvent* answered = session.events->ask(
            &server::plugins::pluginEventTypes::playerItemCooldown, [&](server::plugins::PluginEvent& event) {
                plugins::EventSink::player(event, session);
                event.set("item", item.toString());
                event.set("group", item.toString());
                event.set("ticks", static_cast<std::int64_t>(ticks));
            });
        if (answered != nullptr) {
            if (answered->cancelled()) {
                return;
            }
            if (const auto* edited = std::get_if<std::int64_t>(&answered->get("ticks")); edited != nullptr) {
                length = static_cast<std::int32_t>(*edited);
            }
        }
        if (length <= 0) {
            return;
        }
    }
    session.itemCooldowns[std::string{item.path()}] = gameTime + length;
    if (session.connection().startItemCooldown) {
        session.connection().startItemCooldown(item, length);
    }
}

} // namespace basalt::server::runtime::player
