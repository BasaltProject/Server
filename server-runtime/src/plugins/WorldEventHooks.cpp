#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/EventHooks.hpp>
#include <basalt/server/runtime/plugins/PluginItemConversion.hpp>
#include <basalt/server/runtime/plugins/WorldEventHooks.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <algorithm>
#include <array>

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
    hooks.burnAllowed = [&sink](const world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                std::int32_t stateId, std::int32_t fromX, std::int32_t fromY, std::int32_t fromZ) {
        return sink.allowed(&blockBurn, [&](PluginEvent& event) {
            here(event, world, x, y, z);
            const wire::common::Identifier* block = server::registry::blockStateDefinitions::blockOf(stateId);
            event.set("block", block == nullptr ? std::string{"minecraft:air"} : block->toString());
            event.set("igniter_x", static_cast<std::int64_t>(fromX));
            event.set("igniter_y", static_cast<std::int64_t>(fromY));
            event.set("igniter_z", static_cast<std::int64_t>(fromZ));
        });
    };
    hooks.flowAllowed = [&sink](const world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                std::int32_t toX, std::int32_t toY, std::int32_t toZ) {
        return sink.allowed(&blockFromTo, [&](PluginEvent& event) {
            here(event, world, x, y, z);
            const wire::common::Identifier* block =
                server::registry::blockStateDefinitions::blockOf(world.state().blockState(x, y, z).value_or(0));
            event.set("block", block == nullptr ? std::string{"minecraft:air"} : block->toString());
            event.set("to_x", static_cast<std::int64_t>(toX));
            event.set("to_y", static_cast<std::int64_t>(toY));
            event.set("to_z", static_cast<std::int64_t>(toZ));
            event.set("face", std::string{toY < y   ? "down"
                                          : toX > x ? "east"
                                          : toX < x ? "west"
                                          : toZ > z ? "south"
                                                    : "north"});
        });
    };
    hooks.structureAllowed = [&sink](const world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                     std::int32_t stateId) {
        return sink.allowed(&worldStructureGrow, [&](PluginEvent& event) {
            here(event, world, x, y, z);
            const wire::common::Identifier* block = server::registry::blockStateDefinitions::blockOf(stateId);
            event.set("species", block == nullptr ? std::string{"minecraft:oak_sapling"} : block->toString());
            event.set("player_name", std::string{});
        });
    };
    hooks.dispenseAllowed = [&sink](const world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                    const std::string& item, std::string_view face) {
        return sink.allowed(&blockDispense, [&](PluginEvent& event) {
            here(event, world, x, y, z);
            event.set("item", item);
            event.set("face", std::string{face});
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
                               std::string_view cooker, const server::world::loot::ItemStack& input,
                               server::world::loot::ItemStack& result, std::string_view recipe) {
        const auto describe = [](const server::world::loot::ItemStack& stack) {
            return snbtOf(storage::StoredItemStack{
                .id = stack.item.toString(), .count = stack.count, .components = stack.components});
        };
        const server::plugins::PluginEvent* answered = sink.ask(&inventorySmelt, [&](PluginEvent& event) {
            event.set("world", std::string{worldName});
            event.set("x", static_cast<std::int64_t>(x));
            event.set("y", static_cast<std::int64_t>(y));
            event.set("z", static_cast<std::int64_t>(z));
            event.set("cooker", std::string{cooker});
            event.set("item", input.item.toString());
            event.set("result", result.item.toString());
            event.set("source", describe(input));
            event.set("output", describe(result));
            event.set("recipe", std::string{recipe});
        });
        if (answered == nullptr) {
            return true;
        }
        if (answered->cancelled()) {
            return false;
        }
        if (const auto* text = std::get_if<std::string>(&answered->get("output")); text != nullptr) {
            if (const std::optional<storage::StoredItemStack> chosen = stackFromSnbt(*text); chosen.has_value()) {
                const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
                    wire::common::Identifier::parse(chosen->id);
                if (id.has_value()) {
                    result = server::world::loot::ItemStack{
                        .item = *id, .count = chosen->count, .components = chosen->components};
                }
            }
        }
        return true;
    };
    hooks.furnaceBurn = [&sink](std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
                                std::string_view cooker, const server::world::loot::ItemStack& fuel,
                                std::int32_t& burnTicks) {
        const server::plugins::PluginEvent* answered = sink.ask(&furnaceBurn, [&](PluginEvent& event) {
            event.set("world", std::string{worldName});
            event.set("x", static_cast<std::int64_t>(x));
            event.set("y", static_cast<std::int64_t>(y));
            event.set("z", static_cast<std::int64_t>(z));
            event.set("cooker", std::string{cooker});
            event.set("fuel", snbtOf(storage::StoredItemStack{
                                  .id = fuel.item.toString(), .count = fuel.count, .components = fuel.components}));
            event.set("burn_time", static_cast<std::int64_t>(burnTicks));
        });
        if (answered == nullptr) {
            return true;
        }
        if (answered->cancelled()) {
            return false;
        }
        if (const auto* value = std::get_if<std::int64_t>(&answered->get("burn_time")); value != nullptr) {
            burnTicks = static_cast<std::int32_t>(*value);
        }
        return true;
    };
    hooks.furnaceStarted = [&sink](std::string_view worldName, std::int32_t x, std::int32_t y, std::int32_t z,
                                   std::string_view cooker, const server::world::loot::ItemStack& source,
                                   std::int32_t total, std::string_view recipe) {
        sink.notify(&furnaceStart, [&](PluginEvent& event) {
            event.set("world", std::string{worldName});
            event.set("x", static_cast<std::int64_t>(x));
            event.set("y", static_cast<std::int64_t>(y));
            event.set("z", static_cast<std::int64_t>(z));
            event.set("cooker", std::string{cooker});
            event.set("source",
                      snbtOf(storage::StoredItemStack{
                          .id = source.item.toString(), .count = source.count, .components = source.components}));
            event.set("total", static_cast<std::int64_t>(total));
            event.set("recipe", std::string{recipe});
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
    hooks.armorWatch = [&sink](player::PlayerSession& who) {
        if (!sink.listening(&playerArmorChange)) {
            return;
        }
        constexpr std::array<std::string_view, 4> kSlots{"head", "chest", "legs", "feet"};
        for (std::int32_t index = 0; index < 4; ++index) {
            const auto& stack = who.inventory.at(36 + index);
            const std::string now = stack.has_value() ? stack->id : std::string{};
            if (who.armorKnown && now != who.armorSeen[static_cast<std::size_t>(index)]) {
                sink.notify(&playerArmorChange, [&](PluginEvent& event) {
                    EventSink::player(event, who);
                    event.set("slot", std::string{kSlots[static_cast<std::size_t>(index)]});
                    event.set("old_item", who.armorSeen[static_cast<std::size_t>(index)]);
                    event.set("new_item", now);
                });
            }
            who.armorSeen[static_cast<std::size_t>(index)] = now;
        }
        who.armorKnown = true;
    };
    hooks.healed = [&sink](const player::PlayerSession& who, float amount, std::string_view cause, bool fast) {
        return plugins::eventHooks::raiseHeal(sink, who, amount, cause, fast);
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
    hooks.spawned = [&sink](const world::LoadedWorld& world, const entity::ServerEntity& added,
                            std::string_view reason) {
        sink.notify(&entityAdd, [&](PluginEvent& event) {
            event.set("entity_id", static_cast<std::int64_t>(added.entityId()));
            event.set("entity_type", added.type().toString());
            event.set("world", world.name());
            event.set("x", added.x);
            event.set("y", added.y);
            event.set("z", added.z);
            event.set("reason", std::string{reason});
        });
    };
}

void pistons(BlockChangeBroadcaster::Hooks& hooks, const EventSink& sink) {
    hooks.physicsAllowed = [&sink](const world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                   std::optional<std::int32_t> source) {
        const auto nameOf = [](std::int32_t stateId) {
            const wire::common::Identifier* block = server::registry::blockStateDefinitions::blockOf(stateId);
            return block == nullptr ? std::string{"minecraft:air"} : block->toString();
        };
        return sink.allowed(&blockPhysics, [&](PluginEvent& event) {
            here(event, world, x, y, z);
            event.set("block", nameOf(world.state().blockState(x, y, z).value_or(0)));
            event.set("source_block", source.has_value() ? nameOf(*source) : std::string{});
        });
    };
    hooks.physicsWatched = [&sink] { return sink.listening(&blockPhysics); };
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

void vehicles(VehicleSystem::Hooks& hooks, const EventSink& sink) {
    hooks.damageAllowed = [&sink](const world::LoadedWorld& world, const entity::ServerEntity& vehicle,
                                  const VehicleSystem::Hit& hit) -> std::optional<float> {
        const server::plugins::PluginEvent* event = sink.ask(&vehicleDamage, [&](PluginEvent& filled) {
            filled.set("entity_id", static_cast<std::int64_t>(vehicle.entityId()));
            filled.set("entity_type", vehicle.type().toString());
            filled.set("world", world.name());
            filled.set("amount", static_cast<double>(hit.amount));
            filled.set("attacker_id", static_cast<std::int64_t>(hit.causeId.value_or(-1)));
        });
        if (event == nullptr) {
            return hit.amount;
        }
        if (event->cancelled()) {
            return std::nullopt;
        }
        const auto* edited = std::get_if<double>(&event->get("amount"));
        return edited == nullptr ? hit.amount : static_cast<float>(*edited);
    };
    hooks.destroyAllowed = [&sink](const world::LoadedWorld& world, const entity::ServerEntity& vehicle,
                                   const VehicleSystem::Hit& hit) {
        return sink.allowed(&vehicleDestroy, [&](PluginEvent& filled) {
            filled.set("entity_id", static_cast<std::int64_t>(vehicle.entityId()));
            filled.set("entity_type", vehicle.type().toString());
            filled.set("world", world.name());
            filled.set("attacker_id", static_cast<std::int64_t>(hit.causeId.value_or(-1)));
        });
    };
}

void mobs(MobActionSystem::Hooks& hooks, const EventSink& sink, WorldOf worldOf) {
    hooks.targetChanged = [&sink](const world::LoadedWorld& world, const entity::ServerEntity& mob,
                                  const std::optional<std::int32_t>& previous, std::optional<std::int32_t>& next) {
        const server::plugins::PluginEvent* event = sink.ask(&entityTarget, [&](PluginEvent& filled) {
            filled.set("entity_id", static_cast<std::int64_t>(mob.entityId()));
            filled.set("entity_type", mob.type().toString());
            filled.set("world", world.name());
            filled.set("target_id", static_cast<std::int64_t>(next.value_or(-1)));
            filled.set("previous_id", static_cast<std::int64_t>(previous.value_or(-1)));
            filled.set("reason", std::string{next.has_value() ? "closest" : "forget"});
        });
        if (event == nullptr) {
            return true;
        }
        if (event->cancelled()) {
            return false;
        }
        const auto* edited = std::get_if<std::int64_t>(&event->get("target_id"));
        if (edited != nullptr) {
            next = *edited < 0 ? std::nullopt : std::optional{static_cast<std::int32_t>(*edited)};
        }
        return true;
    };
    hooks.pluginGoal = [&sink](const world::LoadedWorld& world, const entity::ServerEntity& mob, std::string_view key,
                               entity::ai::PluginGoalPhase phase) {
        constexpr std::string_view kPhases[] = {"can_use", "can_continue", "start", "stop", "tick"};
        const server::plugins::PluginEvent* event = sink.ask(&entityGoal, [&](PluginEvent& filled) {
            filled.set("entity_id", static_cast<std::int64_t>(mob.entityId()));
            filled.set("entity_type", mob.type().toString());
            filled.set("world", world.name());
            filled.set("key", std::string{key});
            filled.set("phase", std::string{kPhases[static_cast<std::size_t>(phase)]});
            filled.set("result", false);
        });
        const bool* answer = event == nullptr ? nullptr : std::get_if<bool>(&event->get("result"));
        return answer != nullptr && *answer;
    };
    hooks.deathDrops = [&sink, worldOf](const world::LoadedWorld& world, const entity::ServerEntity& mob,
                                        std::vector<server::world::loot::ItemStack>& drops, std::int32_t& experience) {
        std::string lines;
        for (const server::world::loot::ItemStack& stack : drops) {
            lines += (lines.empty() ? "" : "\n") + stack.item.toString() + "|" + std::to_string(stack.count);
        }
        const server::plugins::PluginEvent* event = sink.ask(&entityDrops, [&](PluginEvent& filled) {
            filled.set("entity_id", static_cast<std::int64_t>(mob.entityId()));
            filled.set("entity_type", mob.type().toString());
            filled.set("world", world.name());
            filled.set("x", mob.x);
            filled.set("y", mob.y);
            filled.set("z", mob.z);
            filled.set("drops", lines);
            filled.set("exp", static_cast<std::int64_t>(experience));
            filled.set("killer_id", static_cast<std::int64_t>(mob.lastBlame.causingId.value_or(-1)));
        });
        if (event == nullptr) {
            return;
        }
        if (const auto* paid = std::get_if<std::int64_t>(&event->get("exp")); paid != nullptr) {
            experience = static_cast<std::int32_t>(std::max<std::int64_t>(0, *paid));
        }
        const auto* edited = std::get_if<std::string>(&event->get("drops"));
        if (edited == nullptr || *edited == lines) {
            return;
        }
        std::vector<server::world::loot::ItemStack> next;
        std::vector<bool> used(drops.size(), false);
        for (std::size_t from = 0; from <= edited->size();) {
            const std::size_t end = std::min(edited->find('\n', from), edited->size());
            const std::string line = edited->substr(from, end - from);
            from = end + 1;
            const std::size_t bar = line.find('|');
            if (bar == std::string::npos) {
                continue;
            }
            const auto item = wire::common::Identifier::parse(line.substr(0, bar));
            const std::int32_t count = std::atoi(line.c_str() + bar + 1);
            if (!item.has_value() || count <= 0) {
                continue;
            }
            bool kept = false;
            for (std::size_t at = 0; at < drops.size() && !kept; ++at) {
                if (!used[at] && drops[at].item == *item && drops[at].count == count) {
                    used[at] = true;
                    next.push_back(drops[at]);
                    kept = true;
                }
            }
            if (!kept) {
                next.push_back(server::world::loot::ItemStack{.item = *item, .count = count, .components = {}});
            }
        }
        drops = std::move(next);
    };
    hooks.effectAllowed = [&sink](const world::LoadedWorld& world, const entity::ServerEntity& mob,
                                  const world::ActiveEffect& effect, std::string_view action) {
        return sink.allowed(&entityPotionEffect, [&](PluginEvent& event) {
            event.set("entity_id", static_cast<std::int64_t>(mob.entityId()));
            event.set("entity_type", mob.type().toString());
            event.set("world", world.name());
            event.set("effect", effect.effect.toString());
            event.set("amplifier", static_cast<std::int64_t>(effect.amplifier));
            event.set("duration", static_cast<std::int64_t>(effect.remainingTicks));
            event.set("action", std::string{action});
            event.set("cause", std::string{"unknown"});
        });
    };
    hooks.combustAllowed = [&sink, worldOf](const world::LoadedWorld& world, const entity::ServerEntity& mob,
                                            std::int32_t& fireTicks, std::string_view cause) {
        const server::plugins::PluginEvent* event = sink.ask(&entityCombust, [&](PluginEvent& filled) {
            filled.set("entity_id", static_cast<std::int64_t>(mob.entityId()));
            filled.set("entity_type", mob.type().toString());
            filled.set("world", world.name());
            filled.set("ticks", static_cast<std::int64_t>(fireTicks));
            filled.set("cause", std::string{cause});
        });
        if (event == nullptr) {
            return true;
        }
        if (event->cancelled()) {
            return false;
        }
        if (const auto* edited = std::get_if<std::int64_t>(&event->get("ticks")); edited != nullptr) {
            fireTicks = static_cast<std::int32_t>(std::max<std::int64_t>(0, *edited));
        }
        return true;
    };
    hooks.tameAllowed = [&sink](const player::PlayerSession& who, const entity::ServerEntity& animal) {
        return sink.allowed(&entityTame, [&](PluginEvent& event) {
            EventSink::player(event, who);
            event.set("entity_id", static_cast<std::int64_t>(animal.entityId()));
            event.set("entity_type", animal.type().toString());
        });
    };
    hooks.breedAllowed = [&sink](const world::LoadedWorld& world, const entity::ServerEntity& animal,
                                 const entity::ServerEntity& partner, const std::string& breeder,
                                 std::int32_t& experience) {
        const server::plugins::PluginEvent* event = sink.ask(&entityBreed, [&](PluginEvent& filled) {
            filled.set("entity_id", static_cast<std::int64_t>(animal.entityId()));
            filled.set("entity_type", animal.type().toString());
            filled.set("world", world.name());
            filled.set("partner_id", static_cast<std::int64_t>(partner.entityId()));
            filled.set("breeder", breeder);
            filled.set("experience", static_cast<std::int64_t>(experience));
        });
        if (event == nullptr) {
            return true;
        }
        if (event->cancelled()) {
            return false;
        }
        if (const auto* edited = std::get_if<std::int64_t>(&event->get("experience")); edited != nullptr) {
            experience = static_cast<std::int32_t>(*edited);
        }
        return true;
    };
    hooks.combat.damaged = [&sink, worldOf](const entity::ServerEntity& mob, float amount, std::string_view cause,
                                            const std::optional<std::string>& attackerName,
                                            const std::optional<std::int32_t>& attackerId,
                                            const std::optional<std::int32_t>& directId) -> std::optional<float> {
        const server::plugins::PluginEvent* event = sink.ask(&entityDamage, [&](PluginEvent& filled) {
            filled.set("entity_id", static_cast<std::int64_t>(mob.entityId()));
            filled.set("entity_type", mob.type().toString());
            filled.set("world", worldOf ? worldOf(mob) : std::string{});
            filled.set("cause", std::string{cause});
            filled.set("amount", static_cast<double>(amount));
            filled.set("attacker_name", attackerName.value_or(std::string{}));
            filled.set("attacker_id", static_cast<std::int64_t>(attackerId.value_or(-1)));
            filled.set("direct_id", static_cast<std::int64_t>(directId.value_or(attackerId.value_or(-1))));
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
    hooks.combat.died = [&sink, worldOf](const entity::ServerEntity& mob, const std::optional<std::string>& killer,
                                         const std::optional<std::int32_t>& killerId) {
        sink.notify(&entityDeath, [&](PluginEvent& event) {
            event.set("entity_id", static_cast<std::int64_t>(mob.entityId()));
            event.set("entity_type", mob.type().toString());
            event.set("world", worldOf ? worldOf(mob) : std::string{});
            event.set("x", mob.x);
            event.set("y", mob.y);
            event.set("z", mob.z);
            event.set("killer_name", killer.value_or(std::string{}));
            event.set("killer_id", static_cast<std::int64_t>(killerId.value_or(-1)));
        });
    };
}

} // namespace basalt::server::runtime::plugins::worldEventHooks
