#pragma once

#include <basalt/server/plugins/PluginEvents.hpp>
#include <basalt/server/plugins/PluginHookEventTypes.hpp>
#include <basalt/server/runtime/plugins/EventSink.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace basalt::server::runtime::entity {
class ServerEntity;
}

/// Field fillers shared by the events that sit deep in the game systems.
namespace basalt::server::runtime::plugins::hookEvents {

void entity(server::plugins::PluginEvent& event, std::string_view world, const entity::ServerEntity& who);

/// The same fields for something that is not a `ServerEntity`, like a player.
void entity(server::plugins::PluginEvent& event, std::string_view world, std::int32_t id, std::string_view type,
            double x, double y, double z);

void position(server::plugins::PluginEvent& event, std::string_view world, std::int32_t x, std::int32_t y,
              std::int32_t z);

/// Asks the plugins whether `who` may turn the block at these coordinates from one state into another.
[[nodiscard]] bool changeBlockAllowed(const EventSink& sink, std::string_view world, const entity::ServerEntity& who,
                                      std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t fromState,
                                      std::int32_t toState, std::string_view cause = "other");

/// The id of the block a state belongs to, `minecraft:air` for an unknown one.
[[nodiscard]] std::string blockName(std::int32_t stateId);

} // namespace basalt::server::runtime::plugins::hookEvents
