#pragma once

#include <basalt/server/runtime/VehicleSystem.hpp>
#include <basalt/server/runtime/entity/DragonState.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/PostAttackEffects.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>
#include <basalt/server/runtime/world/RealEntityTypeTags.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
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

class CombatSystem {
public:
    using Hooks = entity::entityCombat::Hooks;

    struct Options {
        std::function<std::int32_t(std::int32_t bound)> nextInt;
        std::function<std::int64_t()> nextSeed;
        /// parity: `VehicleEntity.hurtServer`; answers whether it counted.
        std::function<bool(world::LoadedWorld& world, entity::ServerEntity& vehicle, const VehicleSystem::Hit& hit)>
            hurtVehicle;
        const world::realBlockTags::TagMembers* blockTags = nullptr;
        const world::realEntityTypeTags::TagMembers* entityTags = nullptr;
        std::function<void(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                           const std::vector<entity::ServerEntity*>& living, const entity::ServerEntity& fireball)>
            dragonFireballLanded;
        std::function<bool(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                           entity::ServerEntity& mob, const world::ActiveEffect& effect, std::int32_t sourceId)>
            mobEffect;
        std::function<void(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                           entity::ServerEntity& skull, entity::ServerEntity* mob, player::PlayerSession* player)>
            witherSkullLanded;
        std::function<void(world::LoadedWorld& world, double x, double y, double z, std::int32_t amount)>
            awardExperience;
        /// parity: `TargetBlock.onProjectileHit`'s statistic and trigger.
        std::function<void(world::LoadedWorld& world, player::PlayerSession& shooter,
                           const entity::ServerEntity& projectile, std::int32_t signal)>
            targetHit;
        world::AdvancementTriggers* advancementTriggers = nullptr;
        std::function<void(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                           const entity::ServerEntity& crystal, std::optional<std::int32_t> byPlayer)>
            crystalDestroyed;
        std::function<const wire::nbt::NbtCompound*(const wire::common::Identifier& id)> enchantmentDefinition;
        /// parity: `summon_entity`.
        std::function<void(world::LoadedWorld& world, double x, double y, double z, std::optional<std::int32_t> cause)>
            summonLightning;
        /// parity: `Entity.spawnAtLocation`.
        std::function<void(world::LoadedWorld& world, double x, double y, double z,
                           const server::world::loot::ItemStack& stack)>
            dropItem;
        /// Said when a projectile strikes a block (hitId < 0) or an entity.
        std::function<void(world::LoadedWorld& world, const entity::ServerEntity& projectile, std::int32_t hitId,
                           std::int32_t x, std::int32_t y, std::int32_t z)>
            projectileHit;
    };

    CombatSystem(BlockChangeBroadcaster& broadcaster, ExplosionSystem& explosions, Hooks hooks, Options options);

    /// parity: `Level.gameEvent`.
    void emitVibration(world::LoadedWorld& world, std::string_view event, double x, double y, double z,
                       const wire::common::Uuid& source);

    /// parity: `Level.gameEvent`, with the owner a `Projectile` answers for itself.
    void emitVibration(world::LoadedWorld& world, std::string_view event, double x, double y, double z,
                       const world::vibrations::Source& source);

    /// parity: `Projectile.hitTargetOrDeflectSelf`.
    void tickProjectile(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                        const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& entity,
                        const entity::projectiles::Kind& kind);

    /// parity: `ServerExplosion`.
    struct Gust {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
        float radius = 0.0F;
        /// parity: `getKnockbackMultiplier`, the same for everything the blast reaches.
        float knockbackMultiplier = 1.0F;
        std::optional<std::int32_t> sourceId;
        /// parity: `ServerPlayer.onExplosionHit`.
        bool windCharge = false;
        /// parity: `block_interaction: trigger`; `immuneTag` is what it leaves alone.
        bool triggersBlocks = true;
        std::string immuneTag = "minecraft:blocks_wind_charge_explosions";
        std::string particle;
        std::string sound;
        bool silent = false;
    };

    void gust(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
              const std::vector<entity::ServerEntity*>& living, const Gust& blast);

    /// parity: `ServerLevel.explode`.
    [[nodiscard]] static Gust gustOf(const world::postAttack::Explosion& blast, double x, double y, double z);

    /// parity: `EndCrystal.hurtServer`.
    void hitCrystal(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                    entity::ServerEntity& crystal, bool byExplosion, bool byDragon = false,
                    std::optional<std::int32_t> byPlayer = std::nullopt, bool byCreative = false);

    /// parity: `ServerExplosion.hurtEntities`.
    void hurtByExplosion(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers, double x,
                         double y, double z, float power, std::int32_t sourceEntityId);

    void announceEntityHit(const std::vector<player::PlayerSession*>& watchers, entity::ServerEntity& entity,
                           const wire::common::Identifier& damageType, std::int32_t causeEntityId, bool died);

    bool giveMobEffect(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                       entity::ServerEntity& mob, const world::ActiveEffect& effect, std::int32_t sourceId = -1) const;

    bool hitVehicle(world::LoadedWorld& world, entity::ServerEntity* target, float amount,
                    const wire::common::Identifier& type, const entity::ServerEntity& projectile);

    static void markWitherFriend(world::LoadedWorld& world, entity::ServerEntity& target,
                                 std::optional<std::int32_t> sourceId);

    /// parity: `Projectile.getOwner`, since an arrow kills for whoever shot it.
    [[nodiscard]] static wire::text::Component shooterName(const std::vector<player::PlayerSession*>& watchers,
                                                           const std::vector<entity::ServerEntity*>& living,
                                                           const entity::ServerEntity& projectile);

    /// parity: `#minecraft:redirectable_projectile`, what a punch sends back.
    [[nodiscard]] static bool isRedirectable(const wire::common::Identifier& type);

    /// parity: `Player.deflectProjectile`: along the puncher's look, and theirs from now on.
    static void deflectByPunch(const std::vector<player::PlayerSession*>& watchers, entity::ServerEntity& projectile,
                               const player::PlayerSession& by);

private:
    struct Target {
        entity::ServerEntity* mob = nullptr;
        player::PlayerSession* player = nullptr;
        std::optional<entity::enderDragon::Part> dragonPart;
    };

    /// parity: `ProjectileUtil.computeMargin`.
    [[nodiscard]] static std::optional<Target> targetAlong(const std::vector<player::PlayerSession*>& watchers,
                                                           const std::vector<entity::ServerEntity*>& living,
                                                           const entity::ServerEntity& projectile, double fromX,
                                                           double fromY, double fromZ, double margin = 0.0,
                                                           const std::vector<std::int32_t>* spared = nullptr);

    /// parity: `LlamaSpit.tick`.
    void tickLlamaSpit(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                       const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& spit,
                       const entity::projectiles::Kind& kind);

    /// parity: `LlamaSpit.onHitEntity`: one point, and only while its owner is alive.
    void spitHits(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                  const std::vector<entity::ServerEntity*>& living, const entity::ServerEntity& spit,
                  const Target& target, double fromX, double fromZ);

    /// parity: `Projectile.onHitBlock`.
    void hitBlock(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                  const entity::ServerEntity& projectile, const entity::BlockHit& hit, bool byArrow);

    [[nodiscard]] static bool firedByPlayer(const std::vector<player::PlayerSession*>& watchers,
                                            const entity::ServerEntity& projectile);

    static void rememberShooter(const std::vector<player::PlayerSession*>& watchers,
                                const entity::ServerEntity& projectile, entity::ServerEntity& target);

    /// parity: `AbstractArrow.doPostHurtEffects`.
    void arrowEffects(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                      const std::vector<entity::ServerEntity*>& living, const entity::ServerEntity& arrow,
                      entity::ServerEntity* mob, player::PlayerSession* player);

    /// parity: `LargeFireball.onHitEntity`, the direct hit before the blast.
    void hitByLargeFireball(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                            const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& fireball,
                            const Target& target, double fromX, double fromZ);

    /// parity: `AbstractArrow.onHitEntity`'s failed hit, the arrow turned back.
    void deflectArrow(entity::ServerEntity& arrow);

    enum class ArrowHit : std::uint8_t { Missed, Stopped, Pierced };

    ArrowHit hitEntityAlong(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                            const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& arrow, double fromX,
                            double fromY, double fromZ);

    /// parity: what `AbstractArrow.onHitEntity` tells its shooting player's advancements.
    void arrowHitSound(const std::vector<player::PlayerSession*>& watchers, const entity::ServerEntity& arrow);

    void arrowTriggers(const world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                       entity::ServerEntity& arrow, const entity::ServerEntity& mob, float damage, bool died);

    void gustWhereItLanded(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                           const std::vector<entity::ServerEntity*>& living, const entity::ServerEntity& charge,
                           double x, double y, double z);

    /// parity: `Breeze.isInvulnerableTo`: nothing a breeze fired hurts another breeze.
    [[nodiscard]] static bool breezeProof(const std::vector<entity::ServerEntity*>& living,
                                          const entity::ServerEntity& charge, const entity::ServerEntity& target);

    void lightWhereItLanded(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                            const entity::BlockHit& hit);

    void splashPotion(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                      const std::vector<entity::ServerEntity*>& living, const entity::ServerEntity& bottle);

    /// parity: `ThrownTrident.tick`.
    bool tridentReturns(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                        const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& trident);

    /// parity: `ThrownTrident.onHitEntity`.
    ArrowHit tridentHitEntity(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                              const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& trident,
                              double fromX, double fromY, double fromZ);

    /// parity: channeling's `hit_block` effect.
    void channelOnBlock(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                        const entity::ServerEntity& trident, const entity::BlockHit& hit);

    std::int32_t tridentLoyalty(entity::ServerEntity& trident);

    void tridentSound(const std::vector<player::PlayerSession*>& watchers, const entity::ServerEntity& trident,
                      std::string_view sound, float volume);

    void landEnderPearl(const std::vector<player::PlayerSession*>& watchers, entity::ServerEntity& pearl);
    /// parity: `Projectile.hitTargetOrDeflectSelf` against a breeze.
    bool deflected(const std::vector<player::PlayerSession*>& watchers,
                   const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& projectile, double fromX,
                   double fromY, double fromZ);

    BlockChangeBroadcaster& broadcaster_;
    ExplosionSystem& explosions_;
    Hooks hooks_;
    Options options_;
};

} // namespace basalt::server::runtime
