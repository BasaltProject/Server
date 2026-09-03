#pragma once

#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerSession;
}

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime {

class BlockChangeBroadcaster;
class ExplosionSystem;

/// Everything a projectile does in flight and on landing: what it hits, what that hurts, and the
/// flash and sound an entity being hit owes whoever is watching.
class CombatSystem {
public:
    /// What a plugin gets a say in. Passed straight to `entityCombat::hurt`, which is the only place
    /// a projectile hurts anything that is not a player.
    using Hooks = entity::entityCombat::Hooks;

    /// The rolls a hit needs: a critical arrow's extra damage, and the seed a sound travels with.
    struct Options {
        std::function<std::int32_t(std::int32_t bound)> nextInt;
        std::function<std::int64_t()> nextSeed;
    };

    CombatSystem(BlockChangeBroadcaster& broadcaster, ExplosionSystem& explosions, Hooks hooks, Options options);

    /// One tick of something in flight, and whatever it hits on the way.
    ///
    /// parity: `Projectile.hitTargetOrDeflectSelf`: what sticks stays where it landed, what does not
    /// is gone the moment it lands. What the block itself does about it is `onProjectileHit`.
    void tickProjectile(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                        const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& entity,
                        const entity::projectiles::Kind& kind);

    /// The flash, the push and the sound of an entity being hit by something that is not a player.
    void announceEntityHit(const std::vector<player::PlayerSession*>& watchers, entity::ServerEntity& entity,
                           const wire::common::Identifier& damageType, std::int32_t causeEntityId, bool died);

private:
    /// The nearest thing a projectile passed through this tick, as a mob or as a player.
    struct Target {
        entity::ServerEntity* mob = nullptr;
        player::PlayerSession* player = nullptr;
    };

    [[nodiscard]] static std::optional<Target> targetAlong(const std::vector<player::PlayerSession*>& watchers,
                                                           const std::vector<entity::ServerEntity*>& living,
                                                           const entity::ServerEntity& projectile, double fromX,
                                                           double fromY, double fromZ);

    /// parity: `Projectile.getOwner`, since an arrow kills for whoever shot it. The arrow itself is
    /// the answer once its owner is gone.
    [[nodiscard]] static wire::text::Component shooterName(const std::vector<player::PlayerSession*>& watchers,
                                                           const std::vector<entity::ServerEntity*>& living,
                                                           const entity::ServerEntity& projectile);

    bool hitEntityAlong(const std::vector<player::PlayerSession*>& watchers,
                        const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& arrow, double fromX,
                        double fromY, double fromZ);

    /// What a bottle leaves where it broke.
    void splashPotion(const std::vector<player::PlayerSession*>& watchers, const entity::ServerEntity& bottle);

    /// Where a thrown pearl puts whoever threw it.
    void landEnderPearl(const std::vector<player::PlayerSession*>& watchers, entity::ServerEntity& pearl);

    BlockChangeBroadcaster& broadcaster_;
    ExplosionSystem& explosions_;
    Hooks hooks_;
    Options options_;
};

} // namespace basalt::server::runtime
