#pragma once

#include <basalt/server/runtime/entity/Attributes.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
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

namespace basalt::server::runtime::entity::boats {
struct BoatState;
}

namespace basalt::server::runtime::entity::minecarts {
struct CartState;
}

namespace basalt::server::runtime::entity::horses {
struct HorseState;
}

namespace basalt::server::runtime::entity::creakings {
struct CreakingState;
}

namespace basalt::server::runtime::entity {

namespace enderDragon {
struct DragonState;
}

namespace fishingHooks {
struct HookState;
}

namespace witherBoss {
struct WitherState;
}

namespace villagers {
struct Offers;
}

namespace fireworkRockets {
struct RocketState;
}

namespace enderEyes {
struct EyeState;
}

namespace villager {
struct VillagerState;
}

namespace wardenBrain {
struct WardenState;
}

namespace copperGolems {
struct GolemState;
}

namespace bees {
struct BeeState;
}

namespace allays {
struct AllayState;
}

namespace piglins {
struct PiglinState;
}

namespace neutralAnger {
struct Anger;
}

namespace hoglins {
struct HoglinState;
}

namespace axolotls {
struct AxolotlState;
}

namespace goats {
struct GoatState;
}

namespace camels {
struct CamelState;
}

namespace sniffers {
struct SnifferState;
}

namespace armadillos {
struct ArmadilloState;
}

namespace breezes {
struct BreezeState;
}

namespace frogs {
struct FrogState;
struct TadpoleState;
} // namespace frogs

namespace shulkers {
struct ShulkerState;
struct BulletState;
} // namespace shulkers

namespace ai {
class GoalSelector;

namespace brainKit {
struct SensorScan;
} // namespace brainKit
} // namespace ai

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
    /// Who fed it, since the advancement and the stat are that player's. parity: `loveCause`.
    std::optional<std::string> loveCause;
    /// parity: `BreedGoal.loveTime`, the three seconds two of them have to stay together.
    std::int32_t courtshipTicks = 0;
    /// parity: `PARENT_AGE_AFTER_BREEDING`, the five minutes before either can breed again.
    std::int32_t breedCooldown = 0;

    /// parity: `VillagerData.profession`, which decides what it offers.
    std::string profession = "farmer";
    /// parity: `VillagerData.level` and `Villager.villagerXp`, which unlocks the next tier.
    std::int32_t villagerLevel = 1;
    std::int32_t villagerExperience = 0;
    /// parity: `AbstractVillager.offers`, rolled a level at a time and kept.
    std::shared_ptr<villagers::Offers> offers;
    /// What a villager's brain remembers, or null for anything else.
    std::shared_ptr<villager::VillagerState> villager;
    /// parity: `FireworkRocketEntity`'s own fields; absent for everything that is not a rocket.
    std::shared_ptr<fireworkRockets::RocketState> firework;
    /// parity: `EyeOfEnder`'s unsaved target, life and fate; absent for anything else.
    std::shared_ptr<enderEyes::EyeState> enderEye;

    /// Ticks before it goes off, or `kNoFuse` for anything that is not primed. parity:
    /// `PrimedTnt.fuse`.
    std::int32_t fuse = kNoFuse;

    /// parity: `BlockAttachedEntity.ticksSinceLastCheck`, not saved.
    std::int32_t attachedCheckTicks = 0;

    /// parity: `LivingEntity.invulnerableTime`, the half second after a hit when most others miss.
    std::int32_t hurtCooldown = 0;

    /// Ticks left of running away from whatever hurt it. parity: `PanicGoal.shouldPanic`, which
    /// reads `getLastDamageSource`: a source is only kept for as long as it matters.
    std::int32_t panicTicks = 0;

    /// What the last hit that landed was, as `entityCombat::hurt` names it. parity:
    /// `LivingEntity.lastDamageSource`, which a death roll's `damage_source_properties` reads.
    std::string lastDamageCause;

    /// parity: `DamageSource.getEntity` and `getDirectEntity`. The caller sets `nextBlame` just
    /// before a hit; `lastBlame` is what the last hit that landed carried.
    struct Blame {
        std::optional<std::int32_t> causingId;
        std::optional<std::int32_t> directId;
        std::optional<wire::common::Identifier> directType;
    };

    Blame nextBlame;
    Blame lastBlame;

    /// parity: `LivingEntity.lastHurt`: inside that window only a bigger hit lands, and only the
    /// rest of it.
    float lastHurt = 0.0F;

    /// Who shot it, by entity id. parity: `Projectile.owner`, which is who a kill is blamed on.
    std::optional<std::int32_t> ownerId;
    /// The same by uuid, which is what a save keeps and a load finds the shooter by again.
    std::optional<wire::common::Uuid> ownerUuid;
    /// parity: `Projectile.leftOwner`: clear of the shooter's party, whom it may hit from then on.
    bool leftOwner = false;
    /// parity: `Projectile.lastDeflectedBy`, which it passes through untouched.
    std::optional<std::int32_t> lastDeflectedBy;
    /// parity: `AbstractArrow.isCritArrow`, set by a fully drawn bow and worth extra damage.
    bool critical = false;

    /// Who hit it last, waiting for `HurtByTargetGoal` to turn that into a target.
    std::optional<std::int32_t> hurtBy;
    /// parity: `LivingEntity.lastHurtByMob` and its timestamp, which only `stopBeingAngry` clears.
    std::optional<std::int32_t> lastHurtByMob;
    std::int32_t lastHurtByMobTimestamp = 0;
    /// The player whose hit still counts as theirs, and for how many more ticks.
    /// parity: `LivingEntity.lastHurtByPlayer` and `lastHurtByPlayerMemoryTime`.
    std::optional<wire::common::Uuid> lastHurtByPlayer;
    std::int32_t lastHurtByPlayerTime = 0;
    /// Who it is angry at, by entity id. parity: `Mob.target`.
    std::optional<std::int32_t> targetId;

    /// What a vehicle has taken, times ten. parity: `VehicleEntity.getDamage`, which counts up by
    /// ten a point, falls by one a tick, and breaks the cart past forty.
    float vehicleDamage = 0.0F;
    /// parity: `VehicleEntity.DATA_ID_HURT` and `DATA_ID_HURTDIR`, the shake a hit gives it.
    std::int32_t vehicleHurtTime = 0;
    std::int32_t vehicleHurtDir = 1;
    /// parity: `Entity.boardingCooldown`, the three seconds a mob that left a seat waits.
    std::int32_t boardingCooldown = 0;

    /// parity: `Mob.attackTime`, the ticks a mob waits between swings.
    std::int32_t attackCooldown = 0;
    /// parity: `MeleeAttackGoal.ticksUntilNextPathRecalculation`, so a chase does not repath a tick.
    std::int32_t repathCooldown = 0;

    /// Who tamed it, by player uuid, and what it has been told to do. parity: `TamableAnimal`.
    std::optional<std::string> owner;
    bool sitting = false;
    bool begging = false;
    /// parity: `TamableAnimal.setInSittingPose` without an order, and `Cat.IS_LYING`, both from the
    /// cat's block goals.
    bool sittingPose = false;
    bool lying = false;
    /// parity: `Cat.RELAX_STATE_ONE`, the cat settling beside its sleeping owner.
    bool relaxOne = false;
    std::optional<std::int32_t> ownerHurtBy;
    std::optional<std::int32_t> ownerHurt;

    /// Ticks of fire left on it. parity: `Entity.remainingFireTicks`, which the client draws too.
    std::int32_t fireTicks = 0;

    std::int32_t airSupply = kDefaultMaxAir;
    /// parity: `Dolphin.MOISTNESS_LEVEL`, the two minutes it lasts out of water, and `GOT_FISH`.
    std::int32_t moistness = 2400;
    /// parity: `Zombie.inWaterTime`, `Skeleton.inPowderSnowTime` or the piglins' and hoglin's
    /// `timeInOverworld`, whichever this mob counts, and the countdown once it has started to turn.
    std::int32_t conversionClock = 0;
    std::int32_t conversionCountdown = -1;
    /// Status effects it carries, by effect. parity: `LivingEntity.activeEffects`.
    std::map<wire::common::Identifier, world::ActiveEffect> effects;
    /// parity: `MobEffectInstance.hiddenEffect`, the weaker doses a stronger one covers, the next to
    /// come back first.
    std::map<wire::common::Identifier, std::vector<world::ActiveEffect>> hiddenEffects;
    /// parity: `Creeper.DATA_IS_POWERED`, saved as `powered`.
    bool powered = false;
    /// The bolt that last touched it, so a mooshroom turns once per bolt. parity: `lastLightningBoltUUID`.
    std::int32_t lastLightningBolt = 0;
    /// parity: `LightningBolt.life` and `flashes`, for a bolt.
    std::int32_t boltLife = 2;
    std::int32_t boltFlashes = -1;
    /// parity: `LightningBolt.blocksSetOnFire` and `hitEntities`, what `lightning_strike` reads.
    std::int32_t boltFiresSet = 0;
    std::vector<std::int32_t> boltHitIds;
    /// A boat's floating state, made on its first turn.
    std::shared_ptr<boats::BoatState> boat;
    /// A minecart's rails, rotation, fuel and fuse, made on its first turn.
    std::shared_ptr<minecarts::CartState> cart;
    /// A mount's counters and a camel's dash, made on first use.
    std::shared_ptr<horses::HorseState> horse;
    /// parity: `Creaking`'s brain, its freeze and the heart it is bound to.
    std::shared_ptr<creakings::CreakingState> creaking;
    /// parity: `AbstractBoat.outOfControlTicks` and the two paddles its driver pulls, for a boat.
    float boatOutOfControl = 0.0F;
    bool paddleLeft = false;
    bool paddleRight = false;
    /// parity: `IsImmuneToZombification`.
    bool immuneToZombification = false;
    /// parity: `LivingEntity.zza` and `yya` as a smooth swimmer last left them.
    double swimForward = 0.0;
    double swimUp = 0.0;
    bool gotFish = false;
    /// parity: `Mob.canPickUpLoot`; dolphin enables it in its constructor.
    bool canPickUpLoot = false;

    /// How big a slime or a magma cube is. parity: `AbstractCubeMob`'s own size, which is
    /// everything.
    std::int32_t cubeSize = 1;

    /// How far a pufferfish has puffed up, 0 to 2, and the clocks its tick counts.
    /// parity: `Pufferfish`'s PUFF_STATE, `inflateCounter` and `deflateTimer`.
    std::int32_t puffState = 0;
    std::int32_t puffInflateCounter = 0;
    std::int32_t puffDeflateTimer = 0;

    /// Whether it is winding up a shot. parity: `Ghast.setCharging`/`Blaze.setCharged`.
    bool charging = false;

    /// What a flier's own move control and goals keep between ticks.
    struct Flight {
        /// parity: `GhastMoveControl.floatDuration`.
        std::int32_t floatDuration = 0;
        /// parity: `PhantomMoveControl.speed`, `Phantom.moveTargetPoint`, `anchorPoint` and the swoop
        /// phase of `AttackPhase`.
        float phantomSpeed = 0.1F;
        std::array<double, 3> moveTargetPoint{};
        std::optional<std::array<std::int32_t, 3>> anchorPoint;
        bool swooping = false;
        /// parity: `Vex.boundOrigin` and `limitedLifeTicks`, -1 for a vex that lives for ever.
        std::optional<std::array<std::int32_t, 3>> boundOrigin;
        std::int32_t limitedLifeTicks = -1;
        /// parity: `Bat.targetPosition` and its resting flag.
        std::optional<std::array<std::int32_t, 3>> batTarget;
        bool resting = false;
        /// parity: `Blaze.allowedHeightOffset` and `nextHeightOffsetChangeTick`.
        float blazeHeightOffset = 0.5F;
        std::int32_t blazeHeightOffsetTicks = 0;
    };

    Flight flight;

    /// How angry a warden is at each entity it has noticed, by entity id.
    ///
    /// parity: `AngerManagement`, and absent for everything that is not a warden: a map per entity
    /// would cost every arrow and every item in the world one.
    std::optional<std::map<std::int32_t, std::int32_t>> anger;

    /// parity: `MemoryModuleType.TOUCH_COOLDOWN`, the second a warden waits between shoves.
    std::int32_t touchCooldown = 0;

    /// parity: `Raider`'s membership: `getCurrentRaid` by its id in the world's raids, `Wave`,
    /// `CanJoinRaid`, `ticksOutsideRaid` and `IS_CELEBRATING`.
    std::optional<std::int32_t> raidId;
    std::int32_t raidWave = 0;
    bool canJoinRaid = false;
    std::int32_t ticksOutsideRaid = 0;
    bool celebrating = false;
    /// The raid it names was stopped and dropped: it stays this raider's until it is saved, as
    /// vanilla keeps the dead reference until the chunk reloads.
    bool raidStale = false;
    /// parity: `Raider.hurtServer` refreshes the raid bar before the hit lands: the health it saw
    /// and when, in the order hits came.
    double raidHealthAtHurt = 0.0;
    std::int64_t raidHurtStamp = 0;

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
    /// parity: `LeashData` naming a knot by its fence, and `angularMomentum`, the turn the rope gives.
    std::optional<std::array<std::int32_t, 3>> leashKnotAt;
    double leashAngularMomentum = 0.0;

    /// parity: `Llama.caravanHead` and `caravanTail`, by entity id. A caravan is never saved.
    std::optional<std::int32_t> caravanHead;
    std::optional<std::int32_t> caravanTail;
    /// parity: `Llama.didSpit`, which ends a llama's revenge after one spit.
    bool didSpit = false;

    /// Skeleton-horse thunder trap state, including its persisted eighteen-thousand-tick lifetime.
    bool skeletonTrap = false;
    std::int32_t skeletonTrapTime = 0;

    /// parity: `AbstractHorse.isTamed`; distinct from a tamable animal's owner uuid.
    bool tamedHorse = false;

    /// Runtime-only `LongDistancePatrolGoal.cooldownUntil`.
    std::int64_t patrolCooldownUntil = -1;

    /// parity: a villager in its bed, which is a pose rather than anything the mob does.
    bool sleeping = false;
    /// parity: `LivingEntity.SLEEPING_POS_ID`, the head of the bed it lies in.
    std::optional<std::array<std::int32_t, 3>> sleepingAt;

    /// What a thrown bottle leaves behind. parity: `ThrownSplashPotion`'s own contents.
    std::optional<std::string> potionEffect;

    /// The item a projectile was made from, which it carries and saves. parity: `Fireball.setItem`
    /// and `ThrowableItemProjectile.setItem`, whose stack reaches disk as `Item`.
    std::optional<std::string> heldItem;
    /// The rest of an arrow's stack, a tipped arrow's potion among it. parity: the components of
    /// `AbstractArrow.pickupItemStack`, which an arrow saves under `item`.
    wire::nbt::NbtCompound heldItemComponents;

    /// parity: `AbstractArrow.Pickup`: a mob's arrow stays, a player's comes back.
    enum class ArrowPickup : std::uint8_t { Disallowed, Allowed, CreativeOnly };
    ArrowPickup arrowPickup = ArrowPickup::Disallowed;

    /// parity: `AbstractArrow.pierceLevel`, and the ids it has already gone through.
    std::int8_t pierceLevel = 0;
    std::vector<std::int32_t> piercedIds;
    /// parity: `AbstractArrow.firedFromWeapon` and `piercedAndKilledEntities`, what the shooter's
    /// `killed_by_arrow` reads.
    std::optional<wire::common::Identifier> firedFromWeapon;
    wire::nbt::NbtCompound firedFromWeaponComponents;
    std::vector<wire::common::Identifier> piercedAndKilled;
    /// parity: `AbstractArrow.soundEvent`, a crossbow's hit until it first lands, the arrow's after.
    bool arrowLanded = false;
    /// parity: `AbstractArrow.inGroundTime` and `life`, the turns it has been stuck and the despawn
    /// clock that runs meanwhile.
    std::int32_t inGroundTime = 0;
    std::int32_t arrowLife = 0;
    /// parity: `AbstractArrow.isNoPhysics`, a trident flying home through whatever is in the way.
    bool noPhysics = false;
    /// parity: `ThrownTrident.ID_LOYALTY`, `ID_FOIL`, `dealtDamage` and its return sound played.
    std::int8_t tridentLoyalty = -1;
    bool tridentFoil = false;
    bool tridentDealtDamage = false;
    bool tridentReturning = false;
    /// parity: `LightningBolt.cause` and `visualOnly`: the player a channeled bolt answers to, and a
    /// trap's bolt that hurts and burns nothing.
    std::optional<std::int32_t> boltCause;
    bool boltVisualOnly = false;

    /// parity: `Ravager.attackTick`, `stunnedTick` and `roarTick`; any of them holds it still.
    std::int32_t ravagerAttackTicks = 0;
    std::int32_t ravagerStunTicks = 0;
    std::int32_t ravagerRoarTicks = 0;

    /// How hard a fireball goes off where it lands. parity: `LargeFireball.explosionPower`.
    float explosionPower = 0.0F;

    /// A blue wither skull, which keeps more of its speed. parity: `WitherSkull.isDangerous`.
    bool dangerousSkull = false;

    /// How far a creeper's fuse has burned. parity: `Creeper.swell`, which goes off at thirty.
    std::int32_t swell = 0;

    /// Ticks it has been alive for. parity: `Entity.tickCount`, which is what staggers the mobs.
    std::int32_t tickCount = 0;
    /// parity: `Mob.ambientSoundTime`, the climbing odds of its next ambient sound.
    std::int32_t ambientSoundTime = 0;
    /// parity: `LivingEntity.DATA_STINGER_COUNT_ID` and `removeStingerTime`, the bee stings it wears.
    std::int32_t stingerCount = 0;
    std::int32_t removeStingerTime = 0;
    /// parity: what its brain's `NearestLivingEntitySensor` and `PlayerSensor` saw on their last scan.
    std::shared_ptr<ai::brainKit::SensorScan> sensorScan;
    /// Where its movement was last heard from, and `Entity.moveDist`, `flyDist`, `nextStep` and
    /// `fallDistance`, with `walkingStepSound`'s amethyst chime clock.
    std::optional<std::array<double, 3>> emittedFrom;
    float moveDist = 0.0F;
    float flyDist = 0.0F;
    float nextStep = 1.0F;
    double fallDistance = 0.0;
    std::int32_t lastCrystalSoundTick = 0;
    float crystalSoundIntensity = 0.0F;
    /// parity: `LivingEntity.DATA_ARROW_COUNT_ID` and `removeArrowTime`, the arrows stuck in it.
    std::int32_t arrowCount = 0;
    std::int32_t removeArrowTime = 0;

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

    /// parity: `PathNavigation`'s stuck detection: its own tick count, the last hundred-tick check
    /// and the timeout on the node it is heading for.
    struct NavigationWatch {
        std::int32_t tick = 0;
        std::int32_t lastStuckCheck = 0;
        std::array<double, 3> lastStuckCheckPos{};
        std::array<std::int32_t, 3> timeoutNode{};
        std::int64_t timeoutTimer = 0;
        double timeoutLimit = 0.0;
        std::int64_t lastTimeoutCheck = 0;
        bool stuck = false;
    };

    NavigationWatch navigationWatch;
    /// parity: `LivingEntity.speed`, what the move control last walked it at.
    float aiSpeed = 0.0F;

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

    /// What a fishing bobber keeps beyond this, or null for anything else.
    std::shared_ptr<fishingHooks::HookState> hook;

    /// What a wither keeps beyond this, or null for anything else.
    std::shared_ptr<witherBoss::WitherState> wither;

    /// What a warden's timed behaviours keep, or null for anything else.
    std::shared_ptr<wardenBrain::WardenState> warden;

    /// What a copper golem keeps beyond this, or null for anything else.
    std::shared_ptr<copperGolems::GolemState> copperGolem;
    /// parity: `Bee`'s own fields. Absent for everything that is not one.
    std::shared_ptr<bees::BeeState> bee;
    /// parity: `Allay`'s own fields and brain. Absent for everything that is not one.
    std::shared_ptr<allays::AllayState> allay;
    /// parity: `Piglin` and `PiglinBrute`'s own fields and brain.
    std::shared_ptr<piglins::PiglinState> piglin;
    /// parity: `NeutralMob`'s anger, for the neutral mobs that keep it here.
    std::shared_ptr<neutralAnger::Anger> neutral;
    /// parity: `Hoglin`'s brain.
    std::shared_ptr<hoglins::HoglinState> hoglin;
    /// parity: `Axolotl`'s brain.
    std::shared_ptr<axolotls::AxolotlState> axolotl;
    /// parity: `Goat`'s brain.
    std::shared_ptr<goats::GoatState> goat;
    /// parity: `Frog`'s and `Tadpole`'s brains.
    std::shared_ptr<frogs::FrogState> frog;
    std::shared_ptr<frogs::TadpoleState> tadpole;
    /// parity: `Camel`'s pose and brain.
    std::shared_ptr<camels::CamelState> camel;
    /// parity: `Sniffer`'s brain.
    std::shared_ptr<sniffers::SnifferState> sniffer;
    /// parity: `Armadillo`'s state and brain.
    std::shared_ptr<armadillos::ArmadilloState> armadillo;
    /// parity: `Breeze`'s brain.
    std::shared_ptr<breezes::BreezeState> breeze;
    /// parity: `LivingEntity.discardFriction`, which a long jump turns on until it lands.
    bool discardFriction = false;
    /// Whether its move control swims this turn. parity: `AxolotlMoveControl`, which is
    /// `SmoothSwimmingMoveControl` only in water.
    bool smoothSwimming = false;

    /// What a shulker and a shulker bullet keep beyond this, or null for anything else.
    std::shared_ptr<shulkers::ShulkerState> shulker;
    std::shared_ptr<shulkers::BulletState> bullet;

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
    /// with, else the attribute's own default. parity: `AttributeMap` built from
    /// `DefaultAttributes.getSupplier(type)`, so per-type values (cow 0.2, zombie 0.23 speed).
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
