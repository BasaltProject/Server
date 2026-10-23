#pragma once

#include <basalt/server/plugins/PluginEvents.hpp>

#include <functional>
#include <string_view>

namespace basalt::server::runtime::player {
class PlayerSession;
}

namespace basalt::server::runtime::plugins {

/// Where the game announces what happens to the plugins that listen: cheap while nobody does.
class EventSink {
public:
    using TypeOf = const server::plugins::PluginEventType& (*)();

    explicit EventSink(std::function<server::plugins::PluginEventBus*()> bus) : bus_(std::move(bus)) {}

    [[nodiscard]] bool listening(TypeOf type) const {
        const server::plugins::PluginEventBus* bus = bus_ ? bus_() : nullptr;
        return bus != nullptr && bus->listening(type());
    }

    /// Fires `type` and answers false only when a listener cancelled it. `fill` sets the fields by name.
    template <typename Fill>
    [[nodiscard]] bool allowed(TypeOf type, Fill&& fill) const {
        const server::plugins::PluginEvent* event = server::plugins::emit(bus_ ? bus_() : nullptr, &type(), fill);
        return event == nullptr || !event->cancelled();
    }

    /// Fires `type`; the handlers cannot undo what already happened.
    template <typename Fill>
    void notify(TypeOf type, Fill&& fill) const {
        static_cast<void>(server::plugins::emit(bus_ ? bus_() : nullptr, &type(), fill));
    }

    /// Fires `type` and hands back what a listener changed, or null while nobody listens.
    template <typename Fill>
    [[nodiscard]] const server::plugins::PluginEvent* ask(TypeOf type, Fill&& fill) const {
        return server::plugins::emit(bus_ ? bus_() : nullptr, &type(), fill);
    }

    /// The `player_name`, `player_uuid` and `world` every player event starts with.
    static void player(server::plugins::PluginEvent& event, const player::PlayerSession& who);

private:
    std::function<server::plugins::PluginEventBus*()> bus_;
};

} // namespace basalt::server::runtime::plugins
