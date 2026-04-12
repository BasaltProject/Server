#pragma once

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::player {
class PlayerSession;
}

namespace basalt::server::runtime::world {

class AdvancementTriggers;
class LoadedWorld;
struct DroppedItem;

/// The criteria a thrown item fires when somebody picks it up, by who threw it.
///
/// parity: `LivingEntity.onItemPickup` and `ServerPlayer.onItemPickup`, over `ItemEntity.getOwner`.
namespace thrownItems {

/// A mob took it: `thrown_item_picked_up_by_entity` for the player who threw it.
void pickedUpByMob(AdvancementTriggers* triggers, const LoadedWorld& world,
                   const std::vector<player::PlayerSession*>& players, const DroppedItem& item, std::int32_t count,
                   const entity::ServerEntity& collector);

/// A player took it: the same for a player thrower, then `thrown_item_picked_up_by_player` for the
/// one who took it, whoever threw it.
void pickedUpByPlayer(AdvancementTriggers* triggers, const LoadedWorld& world,
                      const std::vector<player::PlayerSession*>& players, const DroppedItem& item, std::int32_t count,
                      const player::PlayerSession& collector);

} // namespace thrownItems

} // namespace basalt::server::runtime::world
