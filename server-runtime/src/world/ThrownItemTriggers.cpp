#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/DroppedItem.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/ThrownItemTriggers.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>

namespace basalt::server::runtime::world::thrownItems {
namespace {

[[nodiscard]] const player::PlayerSession*
playerThrower(const LoadedWorld& world, const std::vector<player::PlayerSession*>& players, const DroppedItem& item) {
    for (const player::PlayerSession* player : players) {
        if (player->uuid() == *item.thrower && player->worldName == world.name()) {
            return player;
        }
    }
    return nullptr;
}

[[nodiscard]] TriggerEvent eventOf(const DroppedItem& item, std::int32_t count,
                                   const server::world::loot::EntityFacts& entity) {
    TriggerEvent event;
    event.items.emplace("item", TriggerItem{.item = item.item, .count = count, .components = item.components});
    event.entities.emplace("entity", entity);
    return event;
}

} // namespace

void pickedUpByMob(AdvancementTriggers* triggers, const LoadedWorld& world,
                   const std::vector<player::PlayerSession*>& players, const DroppedItem& item, std::int32_t count,
                   const entity::ServerEntity& collector) {
    if (triggers == nullptr || !item.thrower.has_value()) {
        return;
    }
    if (const player::PlayerSession* thrower = playerThrower(world, players, item); thrower != nullptr) {
        triggers->fire("thrown_item_picked_up_by_entity", thrower->name(), thrower->uuid().toString(),
                       eventOf(item, count, triggerFacts::ofEntity(world, collector)));
    }
}

void pickedUpByPlayer(AdvancementTriggers* triggers, const LoadedWorld& world,
                      const std::vector<player::PlayerSession*>& players, const DroppedItem& item, std::int32_t count,
                      const player::PlayerSession& collector) {
    if (triggers == nullptr || !item.thrower.has_value()) {
        return;
    }
    const player::PlayerSession* thrower = playerThrower(world, players, item);
    if (thrower != nullptr) {
        triggers->fire("thrown_item_picked_up_by_entity", thrower->name(), thrower->uuid().toString(),
                       eventOf(item, count, triggerFacts::ofPlayer(collector)));
        triggers->fire("thrown_item_picked_up_by_player", collector.name(), collector.uuid().toString(),
                       eventOf(item, count, triggerFacts::ofPlayer(*thrower)));
        return;
    }
    for (const entity::ServerEntity* mob : world.state().entities()) {
        if (mob->uuid() == *item.thrower && !mob->removed) {
            triggers->fire("thrown_item_picked_up_by_player", collector.name(), collector.uuid().toString(),
                           eventOf(item, count, triggerFacts::ofEntity(world, *mob)));
            return;
        }
    }
}

} // namespace basalt::server::runtime::world::thrownItems
