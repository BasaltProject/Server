#pragma once

#include <basalt/server/runtime/entity/DragonState.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>
#include <basalt/server/runtime/world/RealEntityTypeTags.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/Uuid.hpp>

#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerSession;
}

namespace basalt::server::runtime::world {
class AdvancementTriggers;
class LoadedWorld;
} // namespace basalt::server::runtime::world

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
        /// What `deflects_projectiles` holds: the breeze.
        const world::realEntityTypeTags::TagMembers* entityTags = nullptr;
        /// Where a dragon fireball burst, for the cloud it leaves.
        std::function<void(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                           const std::vector<entity::ServerEntity*>& living, const entity::ServerEntity& fireball)>
            dragonFireballLanded;
        /// A status effect given to a mob, instant ones included. `sourceId` is what threw it.
        std::function<bool(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                           entity::ServerEntity& mob, const world::ActiveEffect& effect, std::int32_t sourceId)>
            mobEffect;
        /// Where a wither skull struck, and who if anyone.
        std::function<void(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                           entity::ServerEntity& skull, entity::ServerEntity* mob, player::PlayerSession* player)>
            witherSkullLanded;
        /// Experience left as orbs at a point.
        std::function<void(world::LoadedWorld& world, double x, double y, double z, std::int32_t amount)>
            awardExperience;
        /// A target block struck by a player's projectile, where, and the signal it now gives.
        /// parity: `TargetBlock.onProjectileHit`'s statistic and trigger.
        std::function<void(world::LoadedWorld& world, player::PlayerSession& shooter,
                           const entity::ServerEntity& projectile, std::int32_t signal)>
            targetHit;
        /// Where a player's arrow's `player_hurt_entity` and `killed_by_arrow` go.
        world::AdvancementTriggers* advancementTriggers = nullptr;
        /// A crystal went, by that player if one was behind it.
        std::function<void(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                           const entity::ServerEntity& crystal, std::optional<std::int32_t> byPlayer)>
            crystalDestroyed;
    };

    CombatSystem(BlockChangeBroadcaster& broadcaster, ExplosionSystem& explosions, Hooks hooks, Options options);

    /// One tick of something in flight, and whatever it hits on the way.
    ///
    /// One game event, sent from a place rather than from a block. parity: `Level.gameEvent`.
    void emitVibration(world::LoadedWorld& world, std::string_view event, double x, double y, double z,
                       const wire::common::Uuid& source);

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

    /// Gives a mob an effect through the server's own rules. Answers whether it took.
    bool giveMobEffect(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                       entity::ServerEntity& mob, const world::ActiveEffect& effect, std::int32_t sourceId = -1) const;

    /// Tells a wither the hit about to land comes from an undead, through its projectile or blast.
    static void markWitherFriend(world::LoadedWorld& world, entity::ServerEntity& target,
                                 std::optional<std::int32_t> sourceId);

    /// parity: `Projectile.getOwner`, since an arrow kills for whoever shot it. The arrow itself is
    /// the answer once its owner is gone.
    [[nodiscard]] static wire::text::Component shooterName(const std::vector<player::PlayerSession*>& watchers,
                                                           const std::vector<entity::ServerEntity*>& living,
                                                           const entity::ServerEntity& projectile);

    /// parity: `#minecraft:redirectable_projectile`, what a punch sends back.
    [[nodiscard]] static bool isRedirectable(const wire::common::Identifier& type);

    /// parity: `Player.deflectProjectile`: along the puncher's look, and theirs from now on.
    static void deflectByPunch(const std::vector<player::PlayerSession*>& watchers, entity::ServerEntity& projectile,
                               const player::PlayerSession& by);

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

    /// Marks the target as hurt by the player who fired, when a player did.
    static void rememberShooter(const std::vector<player::PlayerSession*>& watchers,
                                const entity::ServerEntity& projectile, entity::ServerEntity& target);

    /// parity: `AbstractArrow.doPostHurtEffects`, what an arrow that landed leaves on whoever it hit.
    void arrowEffects(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                      const std::vector<entity::ServerEntity*>& living, const entity::ServerEntity& arrow,
                      entity::ServerEntity* mob, player::PlayerSession* player);

    /// parity: `LargeFireball.onHitEntity`, the direct hit before the blast.
    void hitByLargeFireball(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                            const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& fireball,
                            const Target& target, double fromX, double fromZ);

    /// parity: `AbstractArrow.onHitEntity`'s failed hit, the arrow turned back.
    void deflectArrow(entity::ServerEntity& arrow);

    /// What an arrow did with the next thing along its flight.
    enum class ArrowHit : std::uint8_t { Missed, Stopped, Pierced };

    ArrowHit hitEntityAlong(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                            const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& arrow, double fromX,
                            double fromY, double fromZ);

    /// parity: what `AbstractArrow.onHitEntity` tells its shooting player's advancements.
    /// parity: `AbstractArrow`'s hit sound, on a hit and on landing.
    void arrowHitSound(const std::vector<player::PlayerSession*>& watchers, const entity::ServerEntity& arrow);

    void arrowTriggers(const world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                       entity::ServerEntity& arrow, const entity::ServerEntity& mob, float damage, bool died);

    /// The gust a wind charge leaves where it landed: the blocks it triggers, the push it gives
    /// everything around, and the burst the clients see and hear.
    void gustWhereItLanded(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                           const std::vector<entity::ServerEntity*>& living, const entity::ServerEntity& charge,
                           double x, double y, double z);

    /// parity: `Breeze.isInvulnerableTo`: nothing a breeze fired hurts another breeze.
    [[nodiscard]] static bool breezeProof(const std::vector<entity::ServerEntity*>& living,
                                          const entity::ServerEntity& charge, const entity::ServerEntity& target);

    /// The fire a small fireball leaves where it landed.
    void lightWhereItLanded(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                            const entity::BlockHit& hit);

    /// What a bottle leaves where it broke.
    void splashPotion(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                      const std::vector<entity::ServerEntity*>& living, const entity::ServerEntity& bottle);

    /// Where a thrown pearl puts whoever threw it.
    void landEnderPearl(const std::vector<player::PlayerSession*>& watchers, entity::ServerEntity& pearl);
    /// parity: `Projectile.hitTargetOrDeflectSelf` against a breeze. Answers whether it was turned.
    bool deflected(const std::vector<player::PlayerSession*>& watchers,
                   const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& projectile, double fromX,
                   double fromY, double fromZ);

    BlockChangeBroadcaster& broadcaster_;
    ExplosionSystem& explosions_;
    Hooks hooks_;
    Options options_;
};

} // namespace basalt::server::runtime
