#pragma once

#include <basalt/server/runtime/entity/Attributes.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::entity {

namespace enderDragon {
struct DragonState;
}

namespace ai {
class GoalSelector;
}

/// parity: `LivingEntity.tickDeath`, which removes the body once `deathTime` reaches twenty.
inline constexpr std::int32_t kDeathTicks = 20;

/// One entity that is not a player: what `/summon` puts in the world and what `/damage`, `/ride`,
/// `/rotate`, `/swing` and `/attribute` then act on.
///
/// A holder, deliberately: what it knows is the identity, the position and the state the commands
/// and the systems around it change. The behaviour lives in those systems, not here.
class ServerEntity {
public:
    static constexpr double kDefaultMaxHealth = 20.0;

    /// parity: `PrimedTnt.NO_FUSE`.
    static constexpr std::int32_t kNoFuse = -1;

    /// parity: `Entity.DATA_AIR_SUPPLY_ID`; dolphins override the normal 300-tick maximum.
    static constexpr std::int32_t kDefaultMaxAir = 300;
    static constexpr std::int32_t kDolphinMaxAir = 4800;

    /// parity: `TraderLlama`'s own despawn delay, which is what makes a trader's escort temporary.
    static constexpr std::int32_t kTraderLlamaDespawnDelay = 47999;

    ServerEntity(std::int32_t entityId, wire::common::Uuid uuid, wire::common::Identifier type,
                 std::int32_t protocolType, double x, double y, double z, float yaw = 0.0F, float pitch = 0.0F);

    [[nodiscard]] std::int32_t entityId() const noexcept { return entityId_; }

    [[nodiscard]] const wire::common::Uuid& uuid() const noexcept { return uuid_; }

    [[nodiscard]] const wire::common::Identifier& type() const noexcept { return type_; }

    [[nodiscard]] std::int32_t protocolType() const noexcept { return protocolType_; }

    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    float yaw = 0.0F;
    float pitch = 0.0F;

    /// What the spawn packet carries beyond the type: for a falling block, the state it holds.
    std::int32_t objectData = 0;

    /// How fast it is going, in blocks a tick. parity: `Entity.deltaMovement`.
    double velocityX = 0.0;
    double velocityY = 0.0;
    double velocityZ = 0.0;

    /// Whether the last move ended against something below. parity: `Entity.onGround`.
    bool onGround = false;
    /// Whether the last move was stopped sideways. parity: `Entity.horizontalCollision`.
    bool horizontallyBlocked = false;

    /// Its age. parity: `AgeableMob.age`: negative is a baby counting up to nothing, positive is a
    /// grown animal counting down the wait before it can breed again, and zero is ready.
    ///
    /// For anything that is not an animal this is simply the ticks it has lived.
    std::int32_t age = 0;

    /// parity: `Animal.inLove`, the thirty seconds a fed animal spends looking for a partner.
    std::int32_t loveTicks = 0;
    /// parity: `BreedGoal.loveTime`, the three seconds two of them have to stay together.
    std::int32_t courtshipTicks = 0;
    /// parity: `PARENT_AGE_AFTER_BREEDING`, the five minutes before either can breed again.
    std::int32_t breedCooldown = 0;

    /// parity: `VillagerData.profession`, which decides what it offers.
    std::string profession = "farmer";
    /// parity: `VillagerData.level` and `Villager.villagerXp`, which unlocks the next tier.
    std::int32_t villagerLevel = 1;
    std::int32_t villagerExperience = 0;

    /// Ticks before it goes off, or `kNoFuse` for anything that is not primed. parity:
    /// `PrimedTnt.fuse`.
    std::int32_t fuse = kNoFuse;

    /// parity: `LivingEntity.invulnerableTime`, the half second after a hit when most others miss.
    std::int32_t hurtCooldown = 0;

    /// Ticks left of running away from whatever hurt it. parity: `PanicGoal.shouldPanic`, which
    /// reads `getLastDamageSource`: a source is only kept for as long as it matters.
    std::int32_t panicTicks = 0;

    /// parity: `LivingEntity.lastHurt`: inside that window only a bigger hit lands, and only the
    /// rest of it.
    float lastHurt = 0.0F;

    /// Who shot it, by entity id. parity: `Projectile.owner`, which is who a kill is blamed on.
    std::optional<std::int32_t> ownerId;
    /// parity: `AbstractArrow.isCritArrow`, set by a fully drawn bow and worth extra damage.
    bool critical = false;

    /// Who hit it last, waiting for `HurtByTargetGoal` to turn that into a target.
    std::optional<std::int32_t> hurtBy;
    /// Who it is angry at, by entity id. parity: `Mob.target`.
    std::optional<std::int32_t> targetId;

    /// What a vehicle has taken, times ten. parity: `VehicleEntity.getDamage`, which counts up by
    /// ten a point, falls by one a tick, and breaks the cart past forty.
    float vehicleDamage = 0.0F;

    /// parity: `Mob.attackTime`, the ticks a mob waits between swings.
    std::int32_t attackCooldown = 0;
    /// parity: `MeleeAttackGoal.ticksUntilNextPathRecalculation`, so a chase does not repath a tick.
    std::int32_t repathCooldown = 0;

    /// Who tamed it, by player uuid, and what it has been told to do. parity: `TamableAnimal`.
    std::optional<std::string> owner;
    bool sitting = false;
    bool begging = false;
    std::optional<std::int32_t> ownerHurtBy;
    std::optional<std::int32_t> ownerHurt;

    /// Ticks of fire left on it. parity: `Entity.remainingFireTicks`, which the client draws too.
    std::int32_t fireTicks = 0;

    std::int32_t airSupply = kDefaultMaxAir;
    /// parity: `Mob.canPickUpLoot`; dolphin enables it in its constructor.
    bool canPickUpLoot = false;

    /// How big a slime or a magma cube is. parity: `AbstractCubeMob`'s own size, which is
    /// everything.
    std::int32_t cubeSize = 1;

    /// Whether it is winding up a shot. parity: `Ghast.setCharging`/`Blaze.setCharged`.
    bool charging = false;

    /// How angry a warden is at each entity it has noticed, by entity id.
    ///
    /// parity: `AngerManagement`, and absent for everything that is not a warden: a map per entity
    /// would cost every arrow and every item in the world one.
    std::optional<std::map<std::int32_t, std::int32_t>> anger;

    /// parity: `MemoryModuleType.TOUCH_COOLDOWN`, the second a warden waits between shoves.
    std::int32_t touchCooldown = 0;

    /// parity: `Raider.getCurrentRaid`, kept here as the village this raider is walking to.
    std::optional<std::array<std::int32_t, 3>> raidCenter;

    /// parity: `PatrollingMonster`'s persisted route and role.
    std::optional<std::array<std::int32_t, 3>> patrolTarget;
    bool patrolLeader = false;
    bool patrolling = false;

    /// Periodic trader state set after event finalization.
    std::int32_t despawnDelay = 0;
    std::optional<std::array<std::int32_t, 3>> wanderTarget;
    std::optional<std::array<std::int32_t, 3>> homePosition;
    std::int32_t homeRadius = 0;

    /// Runtime and save identities of the entity holding this mob's leash.
    std::optional<std::int32_t> leashHolderId;
    std::optional<std::string> leashHolderUuid;

    /// Skeleton-horse thunder trap state, including its persisted eighteen-thousand-tick lifetime.
    bool skeletonTrap = false;
    std::int32_t skeletonTrapTime = 0;

    /// parity: `AbstractHorse.isTamed`; distinct from a tamable animal's owner uuid.
    bool tamedHorse = false;

    /// Runtime-only `LongDistancePatrolGoal.cooldownUntil`.
    std::int64_t patrolCooldownUntil = -1;

    /// parity: a villager in its bed, which is a pose rather than anything the mob does.
    bool sleeping = false;

    /// Villager brain memories involved in social iron-golem summoning.
    std::int64_t lastSleptGameTime = std::numeric_limits<std::int64_t>::min();
    std::int32_t golemDetectedTicks = 0;

    /// Runtime-only cadence of the villager gossip path that asks five neighbours to agree.
    std::int64_t lastGossipTime = std::numeric_limits<std::int64_t>::min();

    /// What a thrown bottle leaves behind. parity: `ThrownSplashPotion`'s own contents.
    std::optional<std::string> potionEffect;

    /// The item a projectile was made from, which it carries and saves. parity: `Fireball.setItem`
    /// and `ThrowableItemProjectile.setItem`, whose stack reaches disk as `Item`.
    std::optional<std::string> heldItem;

    /// How hard a fireball goes off where it lands. parity: `LargeFireball.explosionPower`.
    float explosionPower = 0.0F;

    /// How far a creeper's fuse has burned. parity: `Creeper.swell`, which goes off at thirty.
    std::int32_t swell = 0;

    /// Ticks it has been alive for. parity: `Entity.tickCount`, which is what staggers the mobs.
    std::int32_t tickCount = 0;

    /// What the goals decided it is doing. parity: `Mob.goalSelector`, one set of goals per mob.
    std::shared_ptr<ai::GoalSelector> goals;

    /// parity: `MoveControl`'s own wanted position and speed modifier, and whether it wants to move.
    double wantedX = 0.0;
    double wantedY = 0.0;
    double wantedZ = 0.0;
    double moveSpeed = 1.0;
    bool moving = false;

    /// Whether it is pushing straight ahead rather than at a point.
    ///
    /// parity: a cube mob has no wanted position at all: `CubeMobMoveControl` only sets the speed,
    /// and the direction is whatever the mob is facing.
    bool movingForward = false;

    /// parity: `LookControl`: where the head is pointed, and for how much longer.
    double lookX = 0.0;
    double lookY = 0.0;
    double lookZ = 0.0;
    std::int32_t lookTicks = 0;
    float lookYawSpeed = 10.0F;
    float lookPitchSpeed = 40.0F;

    /// parity: `Mob.yHeadRot`, which is what the client draws the head with.
    float headYaw = 0.0F;

    /// Where it is walking, or nothing when it is standing still. parity: `MoveControl`'s wanted
    /// position.
    std::optional<std::array<double, 3>> walkTarget;

    /// The way there, block by block. parity: `PathNavigation.path`, walked one node at a time.
    std::optional<std::vector<std::array<std::int32_t, 3>>> path;
    /// Which node of it comes next. parity: `Path.nextNodeIndex`.
    std::int32_t pathIndex = 0;

    /// Ticks with nobody near it. parity: `Mob.noActionTime`, which is what the random despawn
    /// waits on, and what standing beside a mob resets.
    std::int32_t idleTicks = 0;

    /// Whether it is here to stay. parity: `Mob.isPersistenceRequired`, set by naming or a command.
    bool persistent = false;

    /// parity: `Mob.isNoAi`, preserving an explicit structure or summon `NoAI` flag.
    bool noAi = false;

    /// Presentation state inherited from a structure or a save, such as villager biome, cat variant
    /// and equipment. Runtime-owned fields are overlaid whenever it is saved.
    std::optional<wire::nbt::NbtCompound> sourceNbt;

    /// What an ender dragon keeps beyond this, or null for anything else.
    std::shared_ptr<enderDragon::DragonState> dragon;

    /// Attribute base values that were changed from their default. parity: `AttributeMap`, without
    /// modifiers.
    std::map<std::string, double, std::less<>> attributeValues;

    /// Its health.
    double health = kDefaultMaxHealth;

    /// Scoreboard tags, as `/tag` sets them on an entity.
    std::vector<std::string> tags;

    /// What it is riding, and who is riding it. parity: `Entity.vehicle`/`passengers`.
    std::optional<std::int32_t> vehicleId;
    std::vector<std::int32_t> passengerIds;

    std::optional<std::string> customName;

    /// The text a `text_display` shows, as MiniMessage. Absent for everything that is not one.
    ///
    /// A hologram is a real entity in 26.2 rather than a fake armour stand, so this is its text and
    /// not a trick: the client draws it because the entity is a text display, not because of any
    /// packet a plugin sent by hand.
    std::optional<std::string> displayText;

    /// Who can see it, by uuid, or absent for everybody.
    ///
    /// Per-player visibility lives here rather than in each connection because the entity is the
    /// thing that knows: a connection asking "should I draw this?" gets one answer, and a hologram
    /// shown to one player is not a different entity from the one nobody else sees.
    std::optional<std::vector<std::string>> visibleTo;

    /// Set when it has been killed or removed, so the sync pass tells clients to forget it.
    bool removed = false;

    /// Whether it was killed rather than simply forgotten.
    ///
    /// parity: `Entity.die` against `Entity.discard`, which is the difference between a cow that
    /// drops beef and one that despawned because nobody was near it. Set for the one tick the death
    /// is dealt with; `dying` is what lasts.
    bool killed = false;

    /// Whether it is playing out its death. parity: `LivingEntity.isDeadOrDying`: what a mob does
    /// for its last twenty ticks is nothing, but it is still there while it does it.
    bool dying = false;

    /// parity: `Entity.NoGravity`, the tag that makes something hang in the air. `/summon` writes it
    /// and it decides where a projectile goes, so it is not decoration.
    bool noGravity = false;

    /// Whether it has already announced itself. parity: `Projectile.tick` sends `PROJECTILE_SHOOT`
    /// on the projectile's **first** turn and never again, whoever fired it and even when nobody
    /// did: one summoned by command announces itself just the same.
    bool announced = false;

    /// parity: `LivingEntity.deathTime` and its `DeathTime` tag, which counts those twenty ticks.
    std::int32_t deathTime = 0;

    /// Modifiers by attribute then by id. parity: `AttributeInstance`'s three operations.
    std::map<std::string, std::map<std::string, AttributeModifier, std::less<>>, std::less<>> attributeModifiers;

    /// The base value of one attribute: what a command set, else what this kind of entity is born
    /// with, else the attribute's own default.
    ///
    /// parity: `AttributeMap`, which is built from `DefaultAttributes.getSupplier(type)`: a cow
    /// walks at 0.2 and a zombie at 0.23 because their own suppliers say so, not because of any
    /// global.
    [[nodiscard]] double attribute(std::string_view id) const;

    /// The value with modifiers applied.
    ///
    /// parity: `AttributeInstance.calculateValue`: every addition first, then each multiply-base
    /// against the base, then each multiply-total against the running total.
    [[nodiscard]] double attributeWithModifiers(std::string_view id) const;

    /// parity: `EntityType`'s own box, which is what the world collides with.
    [[nodiscard]] double width() const;
    [[nodiscard]] double height() const;

    /// Whether this entity has that scoreboard tag, as `/tag list` reads them.
    [[nodiscard]] bool hasTag(std::string_view tag) const;

    /// Answers whether it was not already there, as vanilla's own set does.
    bool addTag(std::string tag);

    bool removeTag(std::string_view tag);

private:
    std::int32_t entityId_ = 0;
    wire::common::Uuid uuid_;
    wire::common::Identifier type_;
    std::int32_t protocolType_ = 0;
};

} // namespace basalt::server::runtime::entity
