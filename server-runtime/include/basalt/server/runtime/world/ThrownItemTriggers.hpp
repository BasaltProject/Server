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

/// parity: `LivingEntity.onItemPickup` and `ServerPlayer.onItemPickup`.
namespace thrownItems {

void pickedUpByMob(AdvancementTriggers* triggers, const LoadedWorld& world,
                   const std::vector<player::PlayerSession*>& players, const DroppedItem& item, std::int32_t count,
                   const entity::ServerEntity& collector);

void pickedUpByPlayer(AdvancementTriggers* triggers, const LoadedWorld& world,
                      const std::vector<player::PlayerSession*>& players, const DroppedItem& item, std::int32_t count,
                      const player::PlayerSession& collector);

} // namespace thrownItems

} // namespace basalt::server::runtime::world
