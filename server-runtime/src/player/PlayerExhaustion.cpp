#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/plugins/PluginHookEventTypes.hpp>
#include <basalt/server/runtime/player/PlayerExhaustion.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/EventSink.hpp>
#include <basalt/server/runtime/registry/VanillaRegistryData.hpp>

#include <string>
#include <variant>

namespace basalt::server::runtime::player {

void causeExhaustion(PlayerSession& session, const float amount, const std::string_view reason) {
    if (session.invulnerable() || amount == 0.0F) {
        return;
    }
    float exhaustion = amount;
    if (session.events != nullptr) {
        const server::plugins::PluginEvent* answered = session.events->ask(
            &server::plugins::pluginEventTypes::playerExhaustion, [&](server::plugins::PluginEvent& event) {
                plugins::EventSink::player(event, session);
                event.set("reason", std::string{reason});
                event.set("exhaustion", static_cast<double>(amount));
            });
        if (answered != nullptr) {
            if (answered->cancelled()) {
                return;
            }
            if (const auto* edited = std::get_if<double>(&answered->get("exhaustion")); edited != nullptr) {
                exhaustion = static_cast<float>(*edited);
            }
        }
    }
    session.food.addExhaustion(exhaustion);
}

float damageExhaustion(const wire::common::Identifier& damageType) {
    for (const registry::vanillaRegistryData::Entry& entry : registry::vanillaRegistryData::damageTypes()) {
        if (entry.first == damageType.path()) {
            return entry.second.getFloat("exhaustion", 0.0F);
        }
    }
    return 0.0F;
}

} // namespace basalt::server::runtime::player
