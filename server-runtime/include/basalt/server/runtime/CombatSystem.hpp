#pragma once

#include <basalt/server/runtime/entity/DragonState.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>
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
        /// What the `blocks_wind_charge_explosions` tag holds, the only thing that shortens a gust.
        const world::realBlockTags::TagMembers* blockTags = nullptr;
        /// Where a dragon fireball burst, for the cloud it leaves.
        std::function<void(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                           const std::vector<entity::ServerEntity*>& living, const entity::ServerEntity& fireball)>
            dragonFireballLanded;
        /// A crystal went, by that player if one was behind it.
        std::function<void(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                           const entity::ServerEntity& crystal, std::optional<std::int32_t> byPlayer)>
            crystalDestroyed;
    };

    CombatSystem(BlockChangeBroadcaster& broadcaster, ExplosionSystem& explosions, Hooks hooks, Options options);

    /// One tick of something in flight, and whatever it hits on the way.
    ///
    /// One game event, sent from a place rather than from a block. parity: `Level.gameEvent`.
    void emitVibration(world::LoadedWorld& world, std::string_view event, double x, double y, double z);

    /// parity: `Projectile.hitTargetOrDeflectSelf`: what sticks stays where it landed, what does not
    /// is gone the moment it lands. What the block itself does about it is `onProjectileHit`.
    void tickProjectile(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                        const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& entity,
                        const entity::projectiles::Kind& kind);

    /// A hit on an end crystal, which destroys it whatever the damage. `byExplosion` leaves it
    /// gone without a blast of its own. parity: `EndCrystal.hurtServer`.
    void hitCrystal(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                    entity::ServerEntity& crystal, bool byExplosion, bool byDragon = false,
                    std::optional<std::int32_t> byPlayer = std::nullopt, bool byCreative = false);

    /// What a blast does to the entities around it that are not players or items.
    ///
    /// parity: `ServerExplosion.hurtEntities`, which reaches twice the power and skips the entity
    /// that went off.
    void hurtByExplosion(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers, double x,
                         double y, double z, float power, std::int32_t sourceEntityId);

    /// The flash, the push and the sound of an entity being hit by something that is not a player.
    void announceEntityHit(const std::vector<player::PlayerSession*>& watchers, entity::ServerEntity& entity,
                           const wire::common::Identifier& damageType, std::int32_t causeEntityId, bool died);

private:
    /// The nearest thing a projectile passed through this tick, as a mob or as a player.
    struct Target {
        entity::ServerEntity* mob = nullptr;
        player::PlayerSession* player = nullptr;
        /// Which of a dragon's parts it went through; a dragon itself is never hit.
        std::optional<entity::enderDragon::Part> dragonPart;
    };

    [[nodiscard]] static std::optional<Target> targetAlong(const std::vector<player::PlayerSession*>& watchers,
                                                           const std::vector<entity::ServerEntity*>& living,
                                                           const entity::ServerEntity& projectile, double fromX,
                                                           double fromY, double fromZ);

    /// Whether whoever fired it is a player, which is what an ender dragon asks of a hit.
    [[nodiscard]] static bool firedByPlayer(const std::vector<player::PlayerSession*>& watchers,
                                            const entity::ServerEntity& projectile);

    /// parity: `Projectile.getOwner`, since an arrow kills for whoever shot it. The arrow itself is
    /// the answer once its owner is gone.
    [[nodiscard]] static wire::text::Component shooterName(const std::vector<player::PlayerSession*>& watchers,
                                                           const std::vector<entity::ServerEntity*>& living,
                                                           const entity::ServerEntity& projectile);

    bool hitEntityAlong(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                        const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& arrow, double fromX,
                        double fromY, double fromZ);

    /// The gust a wind charge leaves where it landed.
    void gustWhereItLanded(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers, double x,
                           double y, double z);

    /// The fire a small fireball leaves where it landed.
    void lightWhereItLanded(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                            const entity::BlockHit& hit);

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
