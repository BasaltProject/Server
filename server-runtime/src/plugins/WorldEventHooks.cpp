#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/plugins/WorldEventHooks.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

namespace basalt::server::runtime::plugins::worldEventHooks {
namespace {

using server::plugins::PluginEvent;
using namespace server::plugins::pluginEventTypes;

void here(PluginEvent& event, const world::LoadedWorld& world, const std::int32_t x, const std::int32_t y,
          const std::int32_t z) {
    event.set("world", world.name());
    event.set("x", static_cast<std::int64_t>(x));
    event.set("y", static_cast<std::int64_t>(y));
    event.set("z", static_cast<std::int64_t>(z));
}

[[nodiscard]] std::string itemOf(const server::world::loot::ItemStack* stack) {
    return stack == nullptr ? std::string{} : stack->item.toString();
}

} // namespace

void explosions(ExplosionSystem::Hooks& hooks, const EventSink& sink) {
    hooks.allowed = [&sink](const world::LoadedWorld& world, double x, double y, double z, float power,
                            std::string_view cause, std::int32_t sourceEntityId) {
        return sink.allowed(&blockExplode, [&](PluginEvent& event) {
            event.set("world", world.name());
            event.set("x", x);
            event.set("y", y);
            event.set("z", z);
            event.set("power", static_cast<double>(power));
            event.set("cause", std::string{cause});
            event.set("entity_id", static_cast<std::int64_t>(sourceEntityId));
        });
    };
}

void redstone(RedstoneTickSystem::Hooks& hooks, const EventSink& sink) {
    hooks.igniteAllowed = [&sink](const world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                  std::string_view cause) {
        return sink.allowed(&blockIgnite, [&](PluginEvent& event) {
            here(event, world, x, y, z);
            event.set("cause", std::string{cause});
            event.set("player_name", std::string{});
        });
    };
    hooks.dispenseAllowed = [&sink](const world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
        return sink.allowed(&blockDispense, [&](PluginEvent& event) {
            here(event, world, x, y, z);
            event.set("item", std::string{});
        });
    };
    hooks.allowHopperMove = [&sink](std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z) {
        return sink.allowed(&inventoryMoveItem, [&](PluginEvent& event) {
            event.set("world", std::string{worldName});
            event.set("x", static_cast<std::int64_t>(x));
            event.set("y", static_cast<std::int64_t>(y));
            event.set("z", static_cast<std::int64_t>(z));
            event.set("kind", std::string{"hopper"});
        });
    };
    hooks.allowSmelt = [&sink](std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
                               std::string_view cooker, const server::world::loot::ItemStack* input,
                               const server::world::loot::ItemStack* result) {
        return sink.allowed(&inventorySmelt, [&](PluginEvent& event) {
            event.set("world", std::string{worldName});
            event.set("x", static_cast<std::int64_t>(x));
            event.set("y", static_cast<std::int64_t>(y));
            event.set("z", static_cast<std::int64_t>(z));
            event.set("cooker", std::string{cooker});
            event.set("item", itemOf(input));
            event.set("result", itemOf(result));
        });
    };
}

void growth(RedstoneTickSystem::Hooks& hooks, const EventSink& sink) {
    hooks.mayChange = [&sink](std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
                              std::int32_t from, std::int32_t to, std::string_view cause) {
        const auto nameOf = [](const std::int32_t stateId) {
            const wire::common::Identifier* block = server::registry::blockStateDefinitions::blockOf(stateId);
            return block == nullptr ? std::string{"minecraft:air"} : block->toString();
        };
        const auto fill = [&](PluginEvent& event) {
            event.set("world", std::string{worldName});
            event.set("x", static_cast<std::int64_t>(x));
            event.set("y", static_cast<std::int64_t>(y));
            event.set("z", static_cast<std::int64_t>(z));
            event.set("block", nameOf(from));
            event.set("to_block", nameOf(to));
            event.set("cause", std::string{cause});
        };
        return cause == "fade" ? sink.allowed(&blockFade, fill) : sink.allowed(&blockGrow, fill);
    };
}

void environment(EnvironmentTickSystem::Hooks& hooks, const EventSink& sink) {
    hooks.portalAllowed = [&sink](const player::PlayerSession& who, const world::LoadedWorld&,
                                  const world::LoadedWorld& to, double x, double y, double z, std::string_view kind) {
        return sink.allowed(&playerPortal, [&](PluginEvent& event) {
            EventSink::player(event, who);
            event.set("to_world", to.name());
            event.set("x", x);
            event.set("y", y);
            event.set("z", z);
            event.set("kind", std::string{kind});
        });
    };
    hooks.weatherChangeAllowed = [&sink](const world::LoadedWorld& world, bool raining, bool thundering,
                                         bool wasRaining, bool wasThundering) {
        return sink.allowed(&worldWeatherChange, [&](PluginEvent& event) {
            event.set("world", world.name());
            event.set("raining", raining);
            event.set("thundering", thundering);
            event.set("from_raining", wasRaining);
            event.set("from_thundering", wasThundering);
        });
    };
    hooks.lightningAllowed = [&sink](const world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
        return sink.allowed(&worldLightning, [&](PluginEvent& event) { here(event, world, x, y, z); });
    };
    hooks.foodChanged = [&sink](const player::PlayerSession& who, std::int32_t now,
                                std::int32_t before) -> std::optional<std::int32_t> {
        const server::plugins::PluginEvent* event = sink.ask(&playerFoodChange, [&](PluginEvent& filled) {
            EventSink::player(filled, who);
            filled.set("food", static_cast<std::int64_t>(now));
            filled.set("from_food", static_cast<std::int64_t>(before));
        });
        if (event == nullptr) {
            return now;
        }
        if (event->cancelled()) {
            return std::nullopt;
        }
        const auto* edited = std::get_if<std::int64_t>(&event->get("food"));
        return edited == nullptr ? now : static_cast<std::int32_t>(*edited);
    };
}

void spawning(NaturalSpawningSystem::Hooks& hooks, const EventSink& sink) {
    hooks.spawnAllowed = [&sink](const world::LoadedWorld& world, const wire::common::Identifier& type, double x,
                                 double y, double z, std::string_view reason) {
        return sink.allowed(&entitySpawn, [&](PluginEvent& event) {
            event.set("entity_type", type.toString());
            event.set("world", world.name());
            event.set("x", x);
            event.set("y", y);
            event.set("z", z);
            event.set("reason", std::string{reason});
        });
    };
}

void pistons(BlockChangeBroadcaster::Hooks& hooks, const EventSink& sink) {
    hooks.pistonAllowed = [&sink](const world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                  bool extending, std::string_view direction) {
        return sink.allowed(&blockPiston, [&](PluginEvent& event) {
            here(event, world, x, y, z);
            event.set("extending", extending);
            event.set("direction", std::string{direction});
            event.set("moved", static_cast<std::int64_t>(0));
        });
    };
}

void mobs(MobActionSystem::Hooks& hooks, const EventSink& sink, WorldOf worldOf) {
    hooks.combustAllowed = [&sink, worldOf](const world::LoadedWorld& world, const entity::ServerEntity& mob,
                                            std::int32_t fireTicks, std::string_view cause) {
        return sink.allowed(&entityCombust, [&](PluginEvent& event) {
            event.set("entity_id", static_cast<std::int64_t>(mob.entityId()));
            event.set("entity_type", mob.type().toString());
            event.set("world", world.name());
            event.set("ticks", static_cast<std::int64_t>(fireTicks));
            event.set("cause", std::string{cause});
        });
    };
    hooks.breedAllowed = [&sink](const world::LoadedWorld& world, const entity::ServerEntity& animal,
                                 const entity::ServerEntity& partner) {
        return sink.allowed(&entityBreed, [&](PluginEvent& event) {
            event.set("entity_id", static_cast<std::int64_t>(animal.entityId()));
            event.set("entity_type", animal.type().toString());
            event.set("world", world.name());
            event.set("partner_id", static_cast<std::int64_t>(partner.entityId()));
        });
    };
    hooks.combat.damaged = [&sink, worldOf](const entity::ServerEntity& mob, float amount, std::string_view cause,
                                            const std::optional<std::string>& attackerName) -> std::optional<float> {
        const server::plugins::PluginEvent* event = sink.ask(&entityDamage, [&](PluginEvent& filled) {
            filled.set("entity_id", static_cast<std::int64_t>(mob.entityId()));
            filled.set("entity_type", mob.type().toString());
            filled.set("world", worldOf ? worldOf(mob) : std::string{});
            filled.set("cause", std::string{cause});
            filled.set("amount", static_cast<double>(amount));
            filled.set("attacker_name", attackerName.value_or(std::string{}));
        });
        if (event == nullptr) {
            return amount;
        }
        if (event->cancelled()) {
            return std::nullopt;
        }
        const auto* edited = std::get_if<double>(&event->get("amount"));
        return edited == nullptr ? amount : static_cast<float>(*edited);
    };
    hooks.combat.died = [&sink, worldOf](const entity::ServerEntity& mob, const std::optional<std::string>& killer) {
        sink.notify(&entityDeath, [&](PluginEvent& event) {
            event.set("entity_id", static_cast<std::int64_t>(mob.entityId()));
            event.set("entity_type", mob.type().toString());
            event.set("world", worldOf ? worldOf(mob) : std::string{});
            event.set("x", mob.x);
            event.set("y", mob.y);
            event.set("z", mob.z);
            event.set("killer_name", killer.value_or(std::string{}));
        });
    };
}

} // namespace basalt::server::runtime::plugins::worldEventHooks
