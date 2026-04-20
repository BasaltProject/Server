#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>

#include <cstdint>
#include <set>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerSession;
}

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime {

class EntityPushSystem;
class MobActionSystem;

namespace entityRiding {

void position(const entity::ServerEntity& vehicle, entity::ServerEntity& passenger);

} // namespace entityRiding

class MobTickSystem {
public:
    MobTickSystem(MobActionSystem& actions, EntityPushSystem& pushes) : actions_(&actions), pushes_(&pushes) {}

    [[nodiscard]] static bool hasActiveVehicle(world::LoadedWorld& world, entity::ServerEntity& entity);

    void tickTree(world::LoadedWorld& world, entity::ServerEntity& root,
                  const std::vector<player::PlayerSession*>& watchers, const std::vector<entity::ai::PlayerView>& views,
                  const std::vector<entity::ServerEntity*>& living, random::RandomSource& random);

    void tickPassengers(world::LoadedWorld& world, entity::ServerEntity& vehicle,
                        const std::vector<player::PlayerSession*>& watchers,
                        const std::vector<entity::ai::PlayerView>& views,
                        const std::vector<entity::ServerEntity*>& living, random::RandomSource& random,
                        std::set<std::int32_t>& visited);

private:
    void tickMob(world::LoadedWorld& world, entity::ServerEntity& entity,
                 const std::vector<player::PlayerSession*>& watchers, const std::vector<entity::ai::PlayerView>& views,
                 const std::vector<entity::ServerEntity*>& living, random::RandomSource& random);

    MobActionSystem* actions_;
    EntityPushSystem* pushes_;
};

} // namespace basalt::server::runtime
