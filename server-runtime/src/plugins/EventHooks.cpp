#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/plugins/EventHooks.hpp>
#include <basalt/server/runtime/plugins/PluginItemConversion.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cmath>
#include <string>

namespace basalt::server::runtime::plugins::eventHooks {
namespace {

using server::plugins::PluginEvent;
using server::plugins::pluginEventTypes::blockBreak;
using server::plugins::pluginEventTypes::blockDamage;
using server::plugins::pluginEventTypes::blockDrops;
using server::plugins::pluginEventTypes::blockFertilize;
using server::plugins::pluginEventTypes::blockPlace;
using server::plugins::pluginEventTypes::entityDamage;
using server::plugins::pluginEventTypes::entityHeal;
using server::plugins::pluginEventTypes::entityMount;
using server::plugins::pluginEventTypes::entityResurrect;
using server::plugins::pluginEventTypes::entityShoot;
using server::plugins::pluginEventTypes::entityTame;
using server::plugins::pluginEventTypes::hangingBreak;
using server::plugins::pluginEventTypes::hangingPlace;
using server::plugins::pluginEventTypes::inventoryChanged;
using server::plugins::pluginEventTypes::inventoryClick;
using server::plugins::pluginEventTypes::inventoryClose;
using server::plugins::pluginEventTypes::inventoryDrag;
using server::plugins::pluginEventTypes::inventoryEnchant;
using server::plugins::pluginEventTypes::inventoryEnchantPrepare;
using server::plugins::pluginEventTypes::inventoryOpen;
using server::plugins::pluginEventTypes::inventoryPrepare;
using server::plugins::pluginEventTypes::inventoryTake;
using server::plugins::pluginEventTypes::inventoryTrade;
using server::plugins::pluginEventTypes::itemSpawn;
using server::plugins::pluginEventTypes::playerBed;
using server::plugins::pluginEventTypes::playerBucket;
using server::plugins::pluginEventTypes::playerDeath;
using server::plugins::pluginEventTypes::playerEffect;
using server::plugins::pluginEventTypes::playerFish;
using server::plugins::pluginEventTypes::playerGamemodeChange;
using server::plugins::pluginEventTypes::playerInteract;
using server::plugins::pluginEventTypes::playerInteractAir;
using server::plugins::pluginEventTypes::playerInteractEntity;
using server::plugins::pluginEventTypes::playerItemConsume;
using server::plugins::pluginEventTypes::playerItemDamage;
using server::plugins::pluginEventTypes::playerItemDrop;
using server::plugins::pluginEventTypes::playerLevelChange;
using server::plugins::pluginEventTypes::playerPickItem;
using server::plugins::pluginEventTypes::playerQuit;
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
                                 std::int32_t stateId, bool& dropItems) {
        const server::plugins::PluginEvent* event = sink.ask(&blockBreak, [&](PluginEvent& filled) {
            EventSink::player(filled, who);
            position(filled, x, y, z);
            filled.set("block", blockNameOf(stateId));
            filled.set("drop_items", true);
        });
        if (event == nullptr) {
            return true;
        }
        if (const auto* drops = std::get_if<bool>(&event->get("drop_items")); drops != nullptr) {
            dropItems = *drops;
        }
        return !event->cancelled();
    };
    hooks.blockDrops = [&sink](const player::PlayerSession* who, const std::string& world, std::int32_t x,
                               std::int32_t y, std::int32_t z, std::int32_t stateId,
                               std::vector<server::world::loot::ItemStack>& drops) {
        if (who == nullptr || !sink.listening(&blockDrops)) {
            return;
        }
        std::string lines;
        for (const server::world::loot::ItemStack& stack : drops) {
            lines += (lines.empty() ? "" : "\n") + stack.item.toString() + "|" + std::to_string(stack.count);
        }
        const server::plugins::PluginEvent* event = sink.ask(&blockDrops, [&](PluginEvent& filled) {
            EventSink::player(filled, *who);
            position(filled, x, y, z);
            filled.set("world", world);
            filled.set("block", blockNameOf(stateId));
            filled.set("drops", lines);
        });
        if (event == nullptr) {
            return;
        }
        if (event->cancelled()) {
            drops.clear();
            return;
        }
        const auto* edited = std::get_if<std::string>(&event->get("drops"));
        if (edited == nullptr || *edited == lines) {
            return;
        }
        std::vector<server::world::loot::ItemStack> next;
        for (std::size_t from = 0; from < edited->size();) {
            const std::size_t end = std::min(edited->find('\n', from), edited->size());
            const std::string line = edited->substr(from, end - from);
            from = end + 1;
            const std::size_t bar = line.find('|');
            if (bar == std::string::npos) {
                continue;
            }
            const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
                wire::common::Identifier::parse(line.substr(0, bar));
            const std::int32_t count = std::atoi(line.c_str() + bar + 1);
            if (id.has_value() && count > 0) {
                next.push_back(server::world::loot::ItemStack{.item = *id, .count = count});
            }
        }
        drops = std::move(next);
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
                                            std::int32_t z, const std::string& item, std::string_view face,
                                            bool mainHand) {
        return sink.allowed(&playerInteract, [&](PluginEvent& event) {
            EventSink::player(event, who);
            event.set("action", std::string{"right_click_block"});
            position(event, x, y, z);
            event.set("block", blockNameAt(owner, x, y, z));
            event.set("item", item);
            event.set("hand", std::string{mainHand ? "main_hand" : "off_hand"});
            event.set("face", std::string{face});
        });
    };
    hooks.airUseAllowed = [&sink](const player::PlayerSession& who, const std::string& item, bool mainHand) {
        return sink.allowed(&playerInteractAir, [&](PluginEvent& event) {
            EventSink::player(event, who);
            event.set("item", item);
            event.set("hand", std::string{mainHand ? "main_hand" : "off_hand"});
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
    hooks.fishing = [&sink](const player::PlayerSession& who, std::string_view state, std::int32_t hookId,
                            std::int32_t& exp) {
        const server::plugins::PluginEvent* event = sink.ask(&playerFish, [&](PluginEvent& filled) {
            EventSink::player(filled, who);
            filled.set("state", std::string{state});
            filled.set("hook_id", static_cast<std::int64_t>(hookId));
            filled.set("caught_id", static_cast<std::int64_t>(-1));
            filled.set("exp", static_cast<std::int64_t>(exp));
        });
        if (event == nullptr) {
            return true;
        }
        if (const auto* paid = std::get_if<std::int64_t>(&event->get("exp")); paid != nullptr) {
            exp = static_cast<std::int32_t>(std::max<std::int64_t>(0, *paid));
        }
        return !event->cancelled();
    };
    hooks.hangingAllowed = [&sink](const player::PlayerSession& who, std::string_view type, std::int32_t x,
                                   std::int32_t y, std::int32_t z, std::string_view face) {
        return sink.allowed(&hangingPlace, [&](PluginEvent& event) {
            EventSink::player(event, who);
            position(event, x, y, z);
            event.set("entity_type", "minecraft:" + std::string{type});
            event.set("face", std::string{face});
        });
    };
    hooks.fertilizeAllowed = [&sink](const player::PlayerSession& who, std::int32_t x, std::int32_t y, std::int32_t z,
                                     std::int32_t stateId) {
        return sink.allowed(&blockFertilize, [&](PluginEvent& event) {
            EventSink::player(event, who);
            position(event, x, y, z);
            event.set("block", blockNameOf(stateId));
        });
    };
    hooks.bedLeft = [&sink](const player::PlayerSession& who, std::int32_t x, std::int32_t y, std::int32_t z) {
        sink.notify(&playerBed, [&](PluginEvent& event) {
            EventSink::player(event, who);
            position(event, x, y, z);
            event.set("entering", false);
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
    PlayerLifecycle::Hooks hooks;
    hooks.respawned = [&sink](const player::PlayerSession& who, const world::LoadedWorld&, double& x, double& y,
                              double& z) {
        const server::plugins::PluginEvent* event = sink.ask(&playerRespawn, [&](PluginEvent& filled) {
            EventSink::player(filled, who);
            filled.set("x", x);
            filled.set("y", y);
            filled.set("z", z);
        });
        if (event == nullptr) {
            return;
        }
        if (const auto* moved = std::get_if<double>(&event->get("x")); moved != nullptr) {
            x = *moved;
        }
        if (const auto* moved = std::get_if<double>(&event->get("y")); moved != nullptr) {
            y = *moved;
        }
        if (const auto* moved = std::get_if<double>(&event->get("z")); moved != nullptr) {
            z = *moved;
        }
    };
    hooks.quit = [&owner, &sink](const player::PlayerSession& who, const std::optional<std::string>&) {
        if (!sink.listening(&playerQuit)) {
            return Announcement{};
        }
        auto farewell = std::make_shared<Announcement>();
        static_cast<void>(owner.onOwnerWait(
            [&sink, &who, farewell] {
                const server::plugins::PluginEvent* quit =
                    sink.ask(&playerQuit, [&](PluginEvent& event) { EventSink::player(event, who); });
                if (quit == nullptr) {
                    return;
                }
                if (const auto* text = std::get_if<std::string>(&quit->get("message")); text != nullptr) {
                    farewell->text = *text;
                }
                if (const auto* silent = std::get_if<bool>(&quit->get("silent")); silent != nullptr) {
                    farewell->silent = *silent;
                }
            },
            std::chrono::milliseconds(500)));
        return *farewell;
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
            std::int64_t causing = sourceEntityId.value_or(-1);
            if (sourceEntityId.has_value() && owner.currentWorld() != nullptr) {
                if (const entity::ServerEntity* source = owner.world().entity(*sourceEntityId); source != nullptr) {
                    attacker = source->type().toString();
                    causing = source->ownerId.value_or(*sourceEntityId);
                }
            }
            filled.set("attacker_name", attacker);
            filled.set("attacker_id", causing);
            filled.set("direct_id", static_cast<std::int64_t>(sourceEntityId.value_or(-1)));
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
    hooks.resurrectAllowed = [&sink](const player::PlayerSession& who, bool offhand) {
        return sink.allowed(&entityResurrect, [&](PluginEvent& event) {
            playerAsEntity(event, who);
            event.set("hand", std::string{offhand ? "off_hand" : "hand"});
        });
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
        Announcement said;
        const server::plugins::PluginEvent* event = sink.ask(&playerDeath, [&](PluginEvent& filled) {
            EventSink::player(filled, who);
            filled.set("x", who.x);
            filled.set("y", who.y);
            filled.set("z", who.z);
            filled.set("cause", deathMessage);
        });
        if (event != nullptr) {
            if (const auto* text = std::get_if<std::string>(&event->get("message")); text != nullptr) {
                said.text = *text;
            }
            if (const auto* silent = std::get_if<bool>(&event->get("silent")); silent != nullptr) {
                said.silent = *silent;
            }
        }
        return said;
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
    hooks.hangingBreakAllowed = [&sink](const player::PlayerSession& who, const entity::ServerEntity& hanging) {
        return sink.allowed(&hangingBreak, [&](PluginEvent& event) {
            event.set("entity_id", static_cast<std::int64_t>(hanging.entityId()));
            event.set("entity_type", hanging.type().toString());
            event.set("world", who.worldName);
            event.set("x", hanging.x);
            event.set("y", hanging.y);
            event.set("z", hanging.z);
            event.set("cause", std::string{"entity"});
            event.set("remover_id", static_cast<std::int64_t>(who.entityId()));
        });
    };
    hooks.dismountAllowed = [&sink](const player::PlayerSession& who, const entity::ServerEntity& vehicle) {
        return sink.allowed(&entityMount, [&](PluginEvent& event) {
            EventSink::player(event, who);
            event.set("entity_id", static_cast<std::int64_t>(vehicle.entityId()));
            event.set("entity_type", vehicle.type().toString());
            event.set("mounting", false);
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
    hooks.closed = [&owner, &sink](std::string_view kind, std::string_view reason) {
        if (const player::PlayerSession* who = owner.playerSession(); who != nullptr) {
            sink.notify(&inventoryClose, [&](PluginEvent& event) {
                EventSink::player(event, *who);
                event.set("kind", std::string{kind});
                event.set("reason", std::string{reason});
            });
        }
    };
    hooks.dragAllowed = [&owner, &sink](std::string_view type, std::string_view slots) {
        const player::PlayerSession* who = owner.playerSession();
        if (who == nullptr) {
            return true;
        }
        return sink.allowed(&inventoryDrag, [&](PluginEvent& event) {
            EventSink::player(event, *who);
            event.set("type", std::string{type});
            event.set("slots", std::string{slots});
        });
    };
    hooks.prepared = [&owner, &sink](std::string_view kind, std::string_view recipe, std::string_view name,
                                     std::optional<storage::StoredItemStack>& result, std::int32_t& cost) {
        const player::PlayerSession* who = owner.playerSession();
        if (who == nullptr) {
            return;
        }
        const server::plugins::PluginEvent* answered = sink.ask(&inventoryPrepare, [&](PluginEvent& event) {
            EventSink::player(event, *who);
            event.set("kind", std::string{kind});
            event.set("recipe", std::string{recipe});
            event.set("result", result.has_value() ? snbtOf(*result) : std::string{});
            event.set("name", std::string{name});
            event.set("cost", static_cast<std::int64_t>(cost));
        });
        if (answered == nullptr) {
            return;
        }
        if (const auto* text = std::get_if<std::string>(&answered->get("result")); text != nullptr) {
            result = stackFromSnbt(*text);
        }
        if (const auto* value = std::get_if<std::int64_t>(&answered->get("cost")); value != nullptr) {
            cost = static_cast<std::int32_t>(*value);
        }
    };
    hooks.takeAllowed = [&owner, &sink](std::string_view kind, std::string_view recipe,
                                        const storage::StoredItemStack& result, bool shift) {
        const player::PlayerSession* who = owner.playerSession();
        if (who == nullptr) {
            return true;
        }
        return sink.allowed(&inventoryTake, [&](PluginEvent& event) {
            EventSink::player(event, *who);
            event.set("kind", std::string{kind});
            event.set("recipe", std::string{recipe});
            event.set("result", snbtOf(result));
            event.set("shift", shift);
        });
    };
    hooks.enchantPrepared = [&owner, &sink](const storage::StoredItemStack& item, std::int32_t shelves,
                                            std::array<world::enchanting::Row, 3>& rows) {
        const player::PlayerSession* who = owner.playerSession();
        if (who == nullptr) {
            return true;
        }
        const auto describe = [&rows] {
            std::string text;
            for (const world::enchanting::Row& row : rows) {
                if (!text.empty()) {
                    text += ';';
                }
                if (!row.offers.empty()) {
                    text += row.offers.front().enchantment.toString() + '|' + std::to_string(row.offers.front().level) +
                            '|' + std::to_string(row.cost);
                }
            }
            return text;
        };
        const server::plugins::PluginEvent* answered = sink.ask(&inventoryEnchantPrepare, [&](PluginEvent& event) {
            EventSink::player(event, *who);
            event.set("item", snbtOf(item));
            event.set("bookshelves", static_cast<std::int64_t>(shelves));
            event.set("offers", describe());
        });
        if (answered == nullptr) {
            return true;
        }
        if (answered->cancelled()) {
            return false;
        }
        if (const auto* text = std::get_if<std::string>(&answered->get("offers")); text != nullptr) {
            std::size_t row = 0;
            for (std::size_t from = 0; row < rows.size(); ++row) {
                const std::size_t end = std::min(text->find(';', from), text->size());
                const std::string entry = text->substr(from, end - from);
                from = end + 1;
                const std::size_t first = entry.find('|');
                const std::size_t second = entry.find('|', first == std::string::npos ? 0 : first + 1);
                if (first == std::string::npos || second == std::string::npos) {
                    rows[row] = {};
                } else {
                    const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
                        wire::common::Identifier::parse(entry.substr(0, first));
                    if (id.has_value()) {
                        rows[row].offers = {world::enchanting::Offer{
                            .enchantment = *id,
                            .level = std::atoi(entry.substr(first + 1, second - first - 1).c_str())}};
                        rows[row].clue = rows[row].offers.front();
                        rows[row].cost = std::atoi(entry.substr(second + 1).c_str());
                    }
                }
                if (end >= text->size()) {
                    ++row;
                    break;
                }
            }
        }
        return true;
    };
    hooks.enchantAllowed = [&owner, &sink](const storage::StoredItemStack& item, std::int32_t button,
                                           std::int32_t& cost, std::vector<world::enchanting::Offer>& offers) {
        const player::PlayerSession* who = owner.playerSession();
        if (who == nullptr) {
            return true;
        }
        std::string text;
        for (const world::enchanting::Offer& offer : offers) {
            text += (text.empty() ? "" : ",") + offer.enchantment.toString() + ':' + std::to_string(offer.level);
        }
        const server::plugins::PluginEvent* answered = sink.ask(&inventoryEnchant, [&](PluginEvent& event) {
            EventSink::player(event, *who);
            event.set("item", snbtOf(item));
            event.set("button", static_cast<std::int64_t>(button));
            event.set("cost", static_cast<std::int64_t>(cost));
            event.set("enchants", text);
        });
        if (answered == nullptr) {
            return true;
        }
        if (answered->cancelled()) {
            return false;
        }
        if (const auto* value = std::get_if<std::int64_t>(&answered->get("cost")); value != nullptr) {
            cost = static_cast<std::int32_t>(*value);
        }
        if (const auto* chosen = std::get_if<std::string>(&answered->get("enchants")); chosen != nullptr) {
            offers.clear();
            for (std::size_t from = 0; from < chosen->size();) {
                const std::size_t end = std::min(chosen->find(',', from), chosen->size());
                const std::string entry = chosen->substr(from, end - from);
                from = end + 1;
                const std::size_t colon = entry.rfind(':');
                const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
                    wire::common::Identifier::parse(entry.substr(0, colon));
                if (colon != std::string::npos && id.has_value()) {
                    offers.push_back(
                        world::enchanting::Offer{.enchantment = *id, .level = std::atoi(entry.c_str() + colon + 1)});
                }
            }
        }
        return !offers.empty();
    };
    hooks.changed = [&owner, &sink]() {
        if (const player::PlayerSession* who = owner.playerSession(); who != nullptr) {
            sink.notify(&inventoryChanged, [&](PluginEvent& event) { EventSink::player(event, *who); });
        }
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
