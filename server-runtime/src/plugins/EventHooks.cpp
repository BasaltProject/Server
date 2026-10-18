#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/plugins/EventHooks.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cmath>
#include <string>

namespace basalt::server::runtime::plugins::eventHooks {
namespace {

using server::plugins::PluginEvent;
using server::plugins::pluginEventTypes::blockBreak;
using server::plugins::pluginEventTypes::blockDamage;
using server::plugins::pluginEventTypes::blockPlace;
using server::plugins::pluginEventTypes::entityDamage;
using server::plugins::pluginEventTypes::entityHeal;
using server::plugins::pluginEventTypes::entityMount;
using server::plugins::pluginEventTypes::entityShoot;
using server::plugins::pluginEventTypes::entityTame;
using server::plugins::pluginEventTypes::inventoryClick;
using server::plugins::pluginEventTypes::inventoryOpen;
using server::plugins::pluginEventTypes::inventoryTrade;
using server::plugins::pluginEventTypes::itemSpawn;
using server::plugins::pluginEventTypes::playerBed;
using server::plugins::pluginEventTypes::playerBucket;
using server::plugins::pluginEventTypes::playerDeath;
using server::plugins::pluginEventTypes::playerEffect;
using server::plugins::pluginEventTypes::playerGamemodeChange;
using server::plugins::pluginEventTypes::playerInteract;
using server::plugins::pluginEventTypes::playerInteractEntity;
using server::plugins::pluginEventTypes::playerItemConsume;
using server::plugins::pluginEventTypes::playerItemDamage;
using server::plugins::pluginEventTypes::playerItemDrop;
using server::plugins::pluginEventTypes::playerLevelChange;
using server::plugins::pluginEventTypes::playerPickItem;
using server::plugins::pluginEventTypes::playerRespawn;
using server::plugins::pluginEventTypes::playerSignChange;
using server::plugins::pluginEventTypes::playerTeleport;
using server::plugins::pluginEventTypes::playerWorldChange;

constexpr std::string_view kPlayerType = "minecraft:player";

[[nodiscard]] std::string blockNameOf(const std::int32_t stateId) {
    const std::optional<server::registry::BlockState> state = server::registry::blocks::byStateId(stateId);
    return state.has_value() ? state->block.toString() : std::string{"minecraft:air"};
}

[[nodiscard]] std::string blockNameAt(ClientSession& owner, const std::int32_t x, const std::int32_t y,
                                      const std::int32_t z) {
    if (owner.currentWorld() == nullptr) {
        return "minecraft:air";
    }
    return blockNameOf(owner.world().blockState(x, y, z).value_or(0));
}

[[nodiscard]] std::string_view gamemodeName(const wire::protocol::play::Gamemode mode) {
    static constexpr std::array<std::string_view, 4> kNames{"survival", "creative", "adventure", "spectator"};
    const auto index = static_cast<std::size_t>(mode);
    return index < kNames.size() ? kNames[index] : kNames[0];
}

void position(PluginEvent& event, const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    event.set("x", static_cast<std::int64_t>(x));
    event.set("y", static_cast<std::int64_t>(y));
    event.set("z", static_cast<std::int64_t>(z));
}

void playerAsEntity(PluginEvent& event, const player::PlayerSession& who) {
    event.set("entity_id", static_cast<std::int64_t>(who.entityId()));
    event.set("entity_type", std::string{kPlayerType});
    event.set("world", who.worldName);
}

} // namespace

BlockInteractionHandler::Hooks blockInteraction(ClientSession& owner, const EventSink& sink) {
    BlockInteractionHandler::Hooks hooks;
    hooks.breakAllowed = [&sink](const player::PlayerSession& who, std::int32_t x, std::int32_t y, std::int32_t z,
                                 std::int32_t stateId) {
        return sink.allowed(&blockBreak, [&](PluginEvent& event) {
            EventSink::player(event, who);
            position(event, x, y, z);
            event.set("block", blockNameOf(stateId));
        });
    };
    hooks.placeAllowed = [&sink](const player::PlayerSession& who, std::int32_t x, std::int32_t y, std::int32_t z,
                                 const std::string& item) {
        return sink.allowed(&blockPlace, [&](PluginEvent& event) {
            EventSink::player(event, who);
            position(event, x, y, z);
            event.set("block", item);
        });
    };
    hooks.interactAllowed = [&owner, &sink](const player::PlayerSession& who, std::int32_t x, std::int32_t y,
                                            std::int32_t z, const std::string& item) {
        return sink.allowed(&playerInteract, [&](PluginEvent& event) {
            EventSink::player(event, who);
            event.set("action", std::string{"right_click_block"});
            position(event, x, y, z);
            event.set("block", blockNameAt(owner, x, y, z));
            event.set("item", item);
            event.set("hand", std::string{"main_hand"});
        });
    };
    hooks.dropAllowed = [&sink](const player::PlayerSession& who, const std::string& item, std::int32_t count) {
        return sink.allowed(&playerItemDrop, [&](PluginEvent& event) {
            EventSink::player(event, who);
            event.set("item", item);
            event.set("count", static_cast<std::int64_t>(count));
        });
    };
    hooks.digAllowed = [&owner, &sink](const player::PlayerSession& who, std::int32_t x, std::int32_t y,
                                       std::int32_t z) {
        return sink.allowed(&blockDamage, [&](PluginEvent& event) {
            EventSink::player(event, who);
            position(event, x, y, z);
            event.set("block", blockNameAt(owner, x, y, z));
        });
    };
    hooks.itemDamageAllowed = [&sink](const player::PlayerSession& who, const std::string& item, std::int32_t amount,
                                      bool broken) {
        return sink.allowed(&playerItemDamage, [&](PluginEvent& event) {
            EventSink::player(event, who);
            event.set("item", item);
            event.set("damage", static_cast<std::int64_t>(amount));
            event.set("broken", broken);
        });
    };
    hooks.signChange = [&sink](const player::PlayerSession& who, std::int32_t x, std::int32_t y, std::int32_t z,
                               std::vector<std::string>& lines) {
        constexpr std::array<std::string_view, 4> kFields{"line1", "line2", "line3", "line4"};
        const server::plugins::PluginEvent* event = sink.ask(&playerSignChange, [&](PluginEvent& filled) {
            EventSink::player(filled, who);
            position(filled, x, y, z);
            for (std::size_t line = 0; line < kFields.size(); ++line) {
                filled.set(kFields[line], line < lines.size() ? lines[line] : std::string{});
            }
        });
        if (event == nullptr) {
            return true;
        }
        if (event->cancelled()) {
            return false;
        }
        for (std::size_t line = 0; line < kFields.size() && line < lines.size(); ++line) {
            if (const auto* edited = std::get_if<std::string>(&event->get(kFields[line])); edited != nullptr) {
                lines[line] = *edited;
            }
        }
        return true;
    };
    hooks.bucketAllowed = [&sink](const player::PlayerSession& who, std::int32_t x, std::int32_t y, std::int32_t z,
                                  const std::string& item, bool filling) {
        return sink.allowed(&playerBucket, [&](PluginEvent& event) {
            EventSink::player(event, who);
            position(event, x, y, z);
            event.set("item", item);
            event.set("filling", filling);
        });
    };
    hooks.bedAllowed = [&sink](const player::PlayerSession& who, std::int32_t x, std::int32_t y, std::int32_t z) {
        return sink.allowed(&playerBed, [&](PluginEvent& event) {
            EventSink::player(event, who);
            position(event, x, y, z);
            event.set("entering", true);
        });
    };
    hooks.itemSpawnAllowed = [&sink](std::string_view worldName, double x, double y, double z, const std::string& item,
                                     std::int32_t count, std::string_view cause) {
        return sink.allowed(&itemSpawn, [&](PluginEvent& event) {
            event.set("world", std::string{worldName});
            event.set("x", x);
            event.set("y", y);
            event.set("z", z);
            event.set("item", item);
            event.set("count", static_cast<std::int64_t>(count));
            event.set("cause", std::string{cause});
        });
    };
    return hooks;
}

PlayerLifecycle::Hooks lifecycle(ClientSession& owner, const EventSink& sink) {
    static_cast<void>(owner);
    PlayerLifecycle::Hooks hooks;
    hooks.respawned = [&sink](const player::PlayerSession& who, const world::LoadedWorld&, double x, double y,
                              double z) {
        sink.notify(&playerRespawn, [&](PluginEvent& event) {
            EventSink::player(event, who);
            event.set("x", x);
            event.set("y", y);
            event.set("z", z);
        });
    };
    hooks.gamemodeAllowed = [&sink](const player::PlayerSession& who, wire::protocol::play::Gamemode mode,
                                    wire::protocol::play::Gamemode previous) {
        return sink.allowed(&playerGamemodeChange, [&](PluginEvent& event) {
            EventSink::player(event, who);
            event.set("gamemode", std::string{gamemodeName(mode)});
            event.set("from_gamemode", std::string{gamemodeName(previous)});
        });
    };
    hooks.worldChanged = [&sink](const player::PlayerSession& who, const world::LoadedWorld&,
                                 const std::optional<std::string>& from) {
        sink.notify(&playerWorldChange, [&](PluginEvent& event) {
            EventSink::player(event, who);
            event.set("from_world", from.value_or(std::string{}));
        });
    };
    hooks.teleportAllowed = [&sink](const player::PlayerSession* who, double toX, double toY, double toZ,
                                    std::string_view cause) {
        if (who == nullptr) {
            return true;
        }
        return sink.allowed(&playerTeleport, [&](PluginEvent& event) {
            EventSink::player(event, *who);
            event.set("from_x", who->x);
            event.set("from_y", who->y);
            event.set("from_z", who->z);
            event.set("x", toX);
            event.set("y", toY);
            event.set("z", toZ);
            event.set("cause", std::string{cause});
        });
    };
    return hooks;
}

CombatHandler::Hooks combat(ClientSession& owner, const EventSink& sink) {
    CombatHandler::Hooks hooks;
    hooks.damaged = [&owner, &sink](const player::PlayerSession& who, float amount, std::string_view cause,
                                    const std::optional<std::int32_t>& sourceEntityId) -> std::optional<float> {
        const server::plugins::PluginEvent* event = sink.ask(&entityDamage, [&](PluginEvent& filled) {
            playerAsEntity(filled, who);
            filled.set("cause", std::string{cause});
            filled.set("amount", static_cast<double>(amount));
            std::string attacker;
            if (sourceEntityId.has_value() && owner.currentWorld() != nullptr) {
                if (const entity::ServerEntity* source = owner.world().entity(*sourceEntityId); source != nullptr) {
                    attacker = source->type().toString();
                }
            }
            filled.set("attacker_name", attacker);
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
    hooks.healed = [&sink](const player::PlayerSession& who, float amount) -> std::optional<float> {
        const server::plugins::PluginEvent* event = sink.ask(&entityHeal, [&](PluginEvent& filled) {
            playerAsEntity(filled, who);
            filled.set("amount", static_cast<double>(amount));
            filled.set("cause", std::string{"regeneration"});
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
    hooks.died = [&sink](const player::PlayerSession& who, const std::string& deathMessage) {
        sink.notify(&playerDeath, [&](PluginEvent& event) {
            EventSink::player(event, who);
            event.set("x", who.x);
            event.set("y", who.y);
            event.set("z", who.z);
            event.set("cause", deathMessage);
        });
    };
    hooks.attackAllowed = [&owner, &sink](const player::PlayerSession& who, std::int32_t targetEntityId) {
        return sink.allowed(&playerInteractEntity, [&](PluginEvent& event) {
            EventSink::player(event, who);
            event.set("entity_id", static_cast<std::int64_t>(targetEntityId));
            std::string type;
            if (owner.currentWorld() != nullptr) {
                if (const entity::ServerEntity* target = owner.world().entity(targetEntityId); target != nullptr) {
                    type = target->type().toString();
                }
            }
            event.set("entity_type", type.empty() ? std::string{kPlayerType} : type);
            event.set("action", std::string{"attack"});
            event.set("hand", std::string{"main_hand"});
        });
    };
    hooks.mountAllowed = [&sink](const player::PlayerSession& who, const entity::ServerEntity& vehicle) {
        return sink.allowed(&entityMount, [&](PluginEvent& event) {
            EventSink::player(event, who);
            event.set("entity_id", static_cast<std::int64_t>(vehicle.entityId()));
            event.set("entity_type", vehicle.type().toString());
            event.set("mounting", true);
        });
    };
    hooks.tameAllowed = [&sink](const player::PlayerSession& who, const entity::ServerEntity& animal) {
        return sink.allowed(&entityTame, [&](PluginEvent& event) {
            EventSink::player(event, who);
            event.set("entity_id", static_cast<std::int64_t>(animal.entityId()));
            event.set("entity_type", animal.type().toString());
        });
    };
    return hooks;
}

ItemUseTimers::Hooks itemUse(ClientSession& owner, const EventSink& sink) {
    static_cast<void>(owner);
    ItemUseTimers::Hooks hooks;
    hooks.consumeAllowed = [&sink](const player::PlayerSession& who, const wire::common::Identifier& item) {
        return sink.allowed(&playerItemConsume, [&](PluginEvent& event) {
            EventSink::player(event, who);
            event.set("item", item.toString());
        });
    };
    hooks.shootAllowed = [&sink](const player::PlayerSession& who, const wire::common::Identifier& projectile,
                                 float power) {
        return sink.allowed(&entityShoot, [&](PluginEvent& event) {
            playerAsEntity(event, who);
            event.set("projectile", projectile.toString());
            event.set("power", static_cast<double>(power));
        });
    };
    return hooks;
}

UiStateSync::Hooks uiState(ClientSession& owner, const EventSink& sink) {
    static_cast<void>(owner);
    UiStateSync::Hooks hooks;
    hooks.effectAllowed = [&sink](const player::PlayerSession& who, const wire::common::Identifier& effect,
                                  std::int32_t amplifier, std::int32_t durationTicks) {
        return sink.allowed(&playerEffect, [&](PluginEvent& event) {
            EventSink::player(event, who);
            event.set("effect", effect.toString());
            event.set("amplifier", static_cast<std::int64_t>(amplifier));
            event.set("duration", static_cast<std::int64_t>(durationTicks));
        });
    };
    hooks.levelChanged = [&sink](const player::PlayerSession& who, std::int32_t level, std::int32_t previous) {
        sink.notify(&playerLevelChange, [&](PluginEvent& event) {
            EventSink::player(event, who);
            event.set("level", static_cast<std::int64_t>(level));
            event.set("from_level", static_cast<std::int64_t>(previous));
        });
    };
    return hooks;
}

ContainerMenuController::Hooks containerMenu(ClientSession& owner, const EventSink& sink) {
    ContainerMenuController::Hooks hooks;
    hooks.openAllowed = [&owner, &sink](std::string_view kind,
                                        const std::optional<wire::protocol::play::BlockPos>& pos) {
        const player::PlayerSession* who = owner.playerSession();
        if (who == nullptr) {
            return true;
        }
        return sink.allowed(&inventoryOpen, [&](PluginEvent& event) {
            EventSink::player(event, *who);
            event.set("kind", std::string{kind});
            position(event, pos.has_value() ? pos->x : 0, pos.has_value() ? pos->y : 0, pos.has_value() ? pos->z : 0);
        });
    };
    hooks.clickAllowed = [&owner, &sink](std::int32_t slot, std::int32_t button, std::string_view mode,
                                         const std::optional<std::string>& item) {
        const player::PlayerSession* who = owner.playerSession();
        if (who == nullptr) {
            return true;
        }
        return sink.allowed(&inventoryClick, [&](PluginEvent& event) {
            EventSink::player(event, *who);
            event.set("slot", static_cast<std::int64_t>(slot));
            event.set("button", static_cast<std::int64_t>(button));
            event.set("mode", std::string{mode});
            event.set("item", item.value_or(std::string{}));
        });
    };
    hooks.pickAllowed = [&sink](const player::PlayerSession& who, std::int32_t x, std::int32_t y, std::int32_t z,
                                const std::string& item) {
        return sink.allowed(&playerPickItem, [&](PluginEvent& event) {
            EventSink::player(event, who);
            position(event, x, y, z);
            event.set("item", item);
        });
    };
    return hooks;
}

VillagerTradeController::Hooks villagerTrade(ClientSession& owner, const EventSink& sink) {
    VillagerTradeController::Hooks hooks;
    hooks.tradeAllowed = [&owner, &sink](std::int32_t containerId, std::int32_t index,
                                         const std::optional<std::string>& resultItem) {
        const player::PlayerSession* who = owner.playerSession();
        if (who == nullptr) {
            return true;
        }
        return sink.allowed(&inventoryTrade, [&](PluginEvent& event) {
            EventSink::player(event, *who);
            event.set("entity_id", static_cast<std::int64_t>(containerId));
            event.set("index", static_cast<std::int64_t>(index));
            event.set("result", resultItem.value_or(std::string{}));
        });
    };
    return hooks;
}

} // namespace basalt::server::runtime::plugins::eventHooks
