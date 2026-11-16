#pragma once

namespace basalt::server::runtime {
class MobActionSystem;
}

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::player {

struct BreakContext;
class PlayerSession;

/// parity: `Bucketable.bucketMobPickup`; answers whether the click was taken, a vetoed one included.
bool pickUpMobFor(const BreakContext& context, MobActionSystem* mobs, PlayerSession& player, world::LoadedWorld& world,
                  entity::ServerEntity& mob, bool mainHand);

} // namespace basalt::server::runtime::player
