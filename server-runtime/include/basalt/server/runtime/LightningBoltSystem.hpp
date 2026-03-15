#pragma once

#include <cstdint>
#include <vector>

namespace basalt::server::random {
class RandomSource;
}

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::player {
class PlayerSession;
}

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime {

class BlockChangeBroadcaster;
class MobActionSystem;

/// A lightning bolt's few ticks in the world: the strike, the flashes after it and everything it
/// touches while it lasts.
///
/// parity: `LightningBolt.tick`.
class LightningBoltSystem {
public:
    LightningBoltSystem(BlockChangeBroadcaster& broadcaster, MobActionSystem& mobs);

    void tick(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
              const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& bolt,
              random::RandomSource& random);

private:
    void hitAround(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                   const std::vector<entity::ServerEntity*>& living, const entity::ServerEntity& bolt);

    BlockChangeBroadcaster* broadcaster_;
    MobActionSystem* mobs_;
};

} // namespace basalt::server::runtime
