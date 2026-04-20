#pragma once

#include <basalt/server/runtime/world/MobEffects.hpp>

#include <functional>

namespace basalt::server::runtime::player {
class PlayerRegistry;
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime {

class StatusEffectSystem {
public:
    using TickOmens = std::function<void(world::LoadedWorld&, player::PlayerSession&, const world::ActiveEffect&)>;

    StatusEffectSystem(player::PlayerRegistry& players, TickOmens tickOmens);

    void tickEffects(world::LoadedWorld& world) const;

private:
    void applyEffectTick(world::LoadedWorld& world, player::PlayerSession& player,
                         const world::ActiveEffect& active) const;

    player::PlayerRegistry* players_;
    TickOmens tickOmens_;
};

} // namespace basalt::server::runtime
