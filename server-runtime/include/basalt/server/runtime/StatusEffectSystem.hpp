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

/// Status effects: their per-tick countdown, and what a tick of each one does.
///
/// `tickOmens` is the raid system's: a bad omen ticking down over a village is how a raid starts, so
/// that one case is handed off rather than reimplemented here.
///
/// parity: `LivingEntity.tickEffects` plus each effect's own `applyEffectTick`.
class StatusEffectSystem {
public:
    using TickOmens = std::function<void(world::LoadedWorld&, player::PlayerSession&, const world::ActiveEffect&)>;

    StatusEffectSystem(player::PlayerRegistry& players, TickOmens tickOmens);

    /// Counts every status effect down and applies the ones that act on a tick.
    void tickEffects(world::LoadedWorld& world) const;

private:
    void applyEffectTick(world::LoadedWorld& world, player::PlayerSession& player,
                         const world::ActiveEffect& active) const;

    player::PlayerRegistry* players_;
    TickOmens tickOmens_;
};

} // namespace basalt::server::runtime
