#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/plugins/HookEvents.hpp>

namespace basalt::server::runtime::plugins::hookEvents {

void entity(server::plugins::PluginEvent& event, const std::string_view world, const entity::ServerEntity& who) {
    event.set("entity_id", static_cast<std::int64_t>(who.entityId()));
    event.set("entity_type", who.type().toString());
    event.set("world", std::string{world});
    event.set("x", who.x);
    event.set("y", who.y);
    event.set("z", who.z);
}

void entity(server::plugins::PluginEvent& event, const std::string_view world, const std::int32_t id,
            const std::string_view type, const double x, const double y, const double z) {
    event.set("entity_id", static_cast<std::int64_t>(id));
    event.set("entity_type", std::string{type});
    event.set("world", std::string{world});
    event.set("x", x);
    event.set("y", y);
    event.set("z", z);
}

void position(server::plugins::PluginEvent& event, const std::string_view world, const std::int32_t x,
              const std::int32_t y, const std::int32_t z) {
    event.set("world", std::string{world});
    event.set("x", static_cast<std::int64_t>(x));
    event.set("y", static_cast<std::int64_t>(y));
    event.set("z", static_cast<std::int64_t>(z));
}

bool changeBlockAllowed(const EventSink& sink, const std::string_view world, const entity::ServerEntity& who,
                        const std::int32_t x, const std::int32_t y, const std::int32_t z, const std::int32_t fromState,
                        const std::int32_t toState, const std::string_view cause) {
    return sink.allowed(&server::plugins::pluginEventTypes::entityChangeBlock,
                        [&](server::plugins::PluginEvent& event) {
                            event.set("entity_id", static_cast<std::int64_t>(who.entityId()));
                            event.set("entity_type", who.type().toString());
                            position(event, world, x, y, z);
                            event.set("block", blockName(fromState));
                            event.set("to_block", blockName(toState));
                            event.set("cause", std::string{cause});
                        });
}

std::string blockName(const std::int32_t stateId) {
    const wire::common::Identifier* block = server::registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{"minecraft:air"} : block->toString();
}

} // namespace basalt::server::runtime::plugins::hookEvents
