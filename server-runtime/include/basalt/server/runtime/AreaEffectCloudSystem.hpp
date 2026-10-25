#pragma once

#include <basalt/server/runtime/entity/AreaEffectClouds.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/RealEntityTypeTags.hpp>

#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerRegistry;
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime {

class CombatSystem;

/// parity: `AreaEffectCloud.serverTick` and `DragonFireball.onHit`.
class AreaEffectCloudSystem {
public:
    struct Options {
        const world::realEntityTypeTags::TagMembers* entityTags = nullptr;
    };

    AreaEffectCloudSystem(player::PlayerRegistry& players, CombatSystem& combat, Options options);

    std::optional<std::int32_t> spawn(world::LoadedWorld& world, const entity::areaEffectClouds::Cloud& settings,
                                      double x, double y, double z, std::optional<std::int32_t> owner = std::nullopt);

    void tick(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
              const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& cloud);

    /// parity: `DragonFireball.onHit`.
    void potionLanded(world::LoadedWorld& world, const entity::ServerEntity& bottle,
                      const std::vector<world::ActiveEffect>& effects);

    void fireballLanded(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                        const std::vector<entity::ServerEntity*>& living, const entity::ServerEntity& fireball);

private:
    /// parity: `HealOrHarmMobEffect.applyInstantenousEffect`.
    void affect(world::LoadedWorld& world, const entity::ServerEntity& cloud,
                const entity::areaEffectClouds::Cloud& settings, player::PlayerSession* player,
                entity::ServerEntity* mob, const std::vector<player::PlayerSession*>& watchers,
                const std::vector<entity::ServerEntity*>& living);

    player::PlayerRegistry* players_;
    CombatSystem* combat_;
    Options options_;
    std::mutex lock_;
    std::unordered_map<std::int32_t, std::unordered_map<std::int32_t, std::int32_t>> victims_;
};

} // namespace basalt::server::runtime
