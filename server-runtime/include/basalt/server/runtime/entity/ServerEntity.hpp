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

namespace mobMisc {
struct State;
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

inline constexpr std::int32_t kDeathTicks = 20;

class ServerEntity {
public:
    static constexpr double kDefaultMaxHealth = 20.0;

    static constexpr std::int32_t kNoFuse = -1;

    static constexpr std::int32_t kDefaultMaxAir = 300;
    static constexpr std::int32_t kDolphinMaxAir = 4800;

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

    std::int32_t objectData = 0;

    double velocityX = 0.0;
    double velocityY = 0.0;
    double velocityZ = 0.0;

    bool onGround = false;
    bool horizontallyBlocked = false;

    std::int32_t age = 0;

    std::int32_t loveTicks = 0;
    std::optional<std::string> loveCause;
    std::int32_t courtshipTicks = 0;
    /// parity: `PARENT_AGE_AFTER_BREEDING`, the five minutes before either can breed again.
    std::int32_t breedCooldown = 0;

    std::string profession = "farmer";
    std::int32_t villagerLevel = 1;
    std::int32_t villagerExperience = 0;
    std::shared_ptr<villagers::Offers> offers;
    std::shared_ptr<villager::VillagerState> villager;
    /// parity: `FireworkRocketEntity`'s own fields.
    std::shared_ptr<fireworkRockets::RocketState> firework;
    /// parity: `EyeOfEnder`'s unsaved target, life and fate; absent for anything else.
    std::shared_ptr<enderEyes::EyeState> enderEye;

    std::int32_t fuse = kNoFuse;

    /// parity: `BlockAttachedEntity.ticksSinceLastCheck`, not saved.
    std::int32_t attachedCheckTicks = 0;

    /// parity: `LivingEntity.invulnerableTime`.
    std::int32_t hurtCooldown = 0;

    std::int32_t panicTicks = 0;

    std::string lastDamageCause;

    struct Blame {
        std::optional<std::int32_t> causingId;
        std::optional<std::int32_t> directId;
        std::optional<wire::common::Identifier> directType;
    };

    Blame nextBlame;
    Blame lastBlame;

    /// parity: `LivingEntity.lastHurt`.
    float lastHurt = 0.0F;

    /// Who shot it, by entity id. parity: `Projectile.owner`, which is who a kill is blamed on.
    std::optional<std::int32_t> ownerId;
    std::optional<wire::common::Uuid> ownerUuid;
    bool leftOwner = false;
    std::optional<std::int32_t> lastDeflectedBy;
    bool critical = false;

    std::optional<std::int32_t> hurtBy;
    /// parity: `LivingEntity.lastHurtByMob` and its timestamp.
    std::optional<std::int32_t> lastHurtByMob;
    std::int32_t lastHurtByMobTimestamp = 0;
    std::optional<wire::common::Uuid> lastHurtByPlayer;
    std::int32_t lastHurtByPlayerTime = 0;
    std::optional<std::int32_t> targetId;

    float vehicleDamage = 0.0F;
    std::int32_t vehicleHurtTime = 0;
    std::int32_t vehicleHurtDir = 1;
    std::int32_t boardingCooldown = 0;

    std::int32_t attackCooldown = 0;
    /// parity: `MeleeAttackGoal.ticksUntilNextPathRecalculation`.
    std::int32_t repathCooldown = 0;

    std::optional<std::string> owner;
    bool sitting = false;
    bool begging = false;
    /// parity: `TamableAnimal.setInSittingPose` without an order.
    bool sittingPose = false;
    bool lying = false;
    /// parity: `Cat.RELAX_STATE_ONE`, the cat settling beside its sleeping owner.
    bool relaxOne = false;
    std::optional<std::int32_t> ownerHurtBy;
    std::optional<std::int32_t> ownerHurt;

    std::int32_t fireTicks = 0;

    std::int32_t airSupply = kDefaultMaxAir;
    std::int32_t moistness = 2400;
    std::int32_t conversionClock = 0;
    std::int32_t conversionCountdown = -1;
    std::map<wire::common::Identifier, world::ActiveEffect> effects;
    std::map<wire::common::Identifier, std::vector<world::ActiveEffect>> hiddenEffects;
    bool powered = false;
    std::int32_t lastLightningBolt = 0;
    std::int32_t boltLife = 2;
    std::int32_t boltFlashes = -1;
    std::int32_t boltFiresSet = 0;
    std::vector<std::int32_t> boltHitIds;
    std::shared_ptr<boats::BoatState> boat;
    std::shared_ptr<minecarts::CartState> cart;
    std::shared_ptr<horses::HorseState> horse;
    std::shared_ptr<creakings::CreakingState> creaking;
    float boatOutOfControl = 0.0F;
    bool paddleLeft = false;
    bool paddleRight = false;
    bool immuneToZombification = false;
    double swimForward = 0.0;
    double swimUp = 0.0;
    bool gotFish = false;
    /// parity: `Dolphin.treasurePos`.
    std::optional<std::array<std::int32_t, 3>> treasurePos;
    bool canPickUpLoot = false;

    std::int32_t cubeSize = 1;

    std::int32_t puffState = 0;
    std::int32_t puffInflateCounter = 0;
    std::int32_t puffDeflateTimer = 0;

    bool charging = false;
    /// parity: `SpellcasterIllager.DATA_SPELL_CASTING_ID`.
    std::int8_t spellType = 0;

    struct Flight {
        std::int32_t floatDuration = 0;
        float phantomSpeed = 0.1F;
        std::array<double, 3> moveTargetPoint{};
        std::optional<std::array<std::int32_t, 3>> anchorPoint;
        bool swooping = false;
        std::optional<std::array<std::int32_t, 3>> boundOrigin;
        std::int32_t limitedLifeTicks = -1;
        std::optional<std::array<std::int32_t, 3>> batTarget;
        bool resting = false;
        float blazeHeightOffset = 0.5F;
        std::int32_t blazeHeightOffsetTicks = 0;
    };

    Flight flight;

    /// parity: `AngerManagement`.
    std::optional<std::map<std::int32_t, std::int32_t>> anger;

    std::int32_t touchCooldown = 0;

    std::optional<std::int32_t> raidId;
    std::int32_t raidWave = 0;
    bool canJoinRaid = false;
    std::int32_t ticksOutsideRaid = 0;
    bool celebrating = false;
    bool raidStale = false;
    /// parity: `Raider.hurtServer` refreshes the raid bar before the hit lands.
    double raidHealthAtHurt = 0.0;
    std::int64_t raidHurtStamp = 0;

    std::optional<std::array<std::int32_t, 3>> patrolTarget;
    bool patrolLeader = false;
    bool patrolling = false;

    std::int32_t despawnDelay = 0;
    std::optional<std::array<std::int32_t, 3>> wanderTarget;
    std::optional<std::array<std::int32_t, 3>> homePosition;
    std::int32_t homeRadius = 0;

    std::optional<std::int32_t> leashHolderId;
    std::optional<std::string> leashHolderUuid;
    std::optional<std::array<std::int32_t, 3>> leashKnotAt;
    double leashAngularMomentum = 0.0;

    /// parity: `Llama.caravanHead` and `caravanTail`, by entity id. A caravan is never saved.
    std::optional<std::int32_t> caravanHead;
    std::optional<std::int32_t> caravanTail;
    /// parity: `Llama.didSpit`, which ends a llama's revenge after one spit.
    bool didSpit = false;

    bool skeletonTrap = false;
    std::int32_t skeletonTrapTime = 0;

    /// parity: `AbstractHorse.isTamed`; distinct from a tamable animal's owner uuid.
    bool tamedHorse = false;

    /// Runtime-only `LongDistancePatrolGoal.cooldownUntil`.
    std::int64_t patrolCooldownUntil = -1;

    bool sleeping = false;
    std::optional<std::array<std::int32_t, 3>> sleepingAt;

    std::optional<std::string> potionEffect;

    std::optional<std::string> heldItem;
    wire::nbt::NbtCompound heldItemComponents;

    enum class ArrowPickup : std::uint8_t { Disallowed, Allowed, CreativeOnly };
    ArrowPickup arrowPickup = ArrowPickup::Disallowed;

    std::int8_t pierceLevel = 0;
    std::vector<std::int32_t> piercedIds;
    std::optional<wire::common::Identifier> firedFromWeapon;
    wire::nbt::NbtCompound firedFromWeaponComponents;
    std::vector<wire::common::Identifier> piercedAndKilled;
    /// parity: `AbstractArrow.soundEvent`.
    bool arrowLanded = false;
    std::int32_t inGroundTime = 0;
    std::int32_t arrowLife = 0;
    bool noPhysics = false;
    std::int8_t tridentLoyalty = -1;
    bool tridentFoil = false;
    bool tridentDealtDamage = false;
    bool tridentReturning = false;
    std::optional<std::int32_t> boltCause;
    bool boltVisualOnly = false;

    std::int32_t ravagerAttackTicks = 0;
    std::int32_t ravagerStunTicks = 0;
    std::int32_t ravagerRoarTicks = 0;

    float explosionPower = 0.0F;

    /// Plugin controls of a projectile: bounce off what it hits, hit this entity or land on the next tick.
    bool bounces = false;
    std::optional<std::int32_t> forcedHitId;
    bool forcedLand = false;
    /// Fire an explosion leaves, or a fireball sets; unset means what vanilla does for the type.
    std::optional<bool> incendiary;
    /// Arrow knockback in levels; unset means what the weapon's enchantments say.
    std::optional<std::int32_t> arrowKnockback;

    bool dangerousSkull = false;

    std::int32_t swell = 0;

    std::int32_t tickCount = 0;
    std::int32_t ambientSoundTime = 0;
    std::int32_t stingerCount = 0;
    std::int32_t removeStingerTime = 0;
    std::shared_ptr<ai::brainKit::SensorScan> sensorScan;
    std::optional<std::array<double, 3>> emittedFrom;
    float moveDist = 0.0F;
    float flyDist = 0.0F;
    float nextStep = 1.0F;
    double fallDistance = 0.0;
    std::int32_t lastCrystalSoundTick = 0;
    float crystalSoundIntensity = 0.0F;
    std::int32_t arrowCount = 0;
    std::int32_t removeArrowTime = 0;

    std::shared_ptr<ai::GoalSelector> goals;

    /// Set by a plugin: whether the default ground navigation opens doors.
    std::optional<bool> navigationOpensDoors;

    /// `Mob.isAggressive`, the arms-up flag that melee goals raise and a plugin may set.
    bool aggressive = false;

    /// Plugin overrides of what a type does by itself, `-1` meaning no override.
    std::int8_t despawnInPeaceful = -1;
    std::int8_t burnsInDay = -1;
    bool ageLocked = false;

    /// `Fox.DATA_FLAGS_ID` bits a plugin owns (crouching, interested, pouncing, faceplanted, defending).
    std::uint8_t foxFlags = 0;
    /// `Panda.DATA_ID_FLAGS` bits (sneeze, roll, sit, on back) and its three counters.
    std::uint8_t pandaFlags = 0;
    std::int32_t pandaUnhappy = 0;
    std::int32_t pandaSneeze = 0;
    std::int32_t pandaEat = 0;

    /// `Guardian.DATA_ID_ATTACK_TARGET` and the beam's charge counter.
    std::optional<std::int32_t> beamTargetId;
    std::int32_t beamTicks = 0;

    double wantedX = 0.0;
    double wantedY = 0.0;
    double wantedZ = 0.0;
    double moveSpeed = 1.0;
    bool moving = false;

    /// parity: a cube mob has no wanted position at all: `CubeMobMoveControl` only sets the speed.
    bool movingForward = false;

    /// parity: `MoveControl.strafe`, the side and forward inputs, consumed on the next tick.
    bool strafing = false;
    double strafeRight = 0.0;
    double strafeForward = 0.0;

    double lookX = 0.0;
    double lookY = 0.0;
    double lookZ = 0.0;
    std::int32_t lookTicks = 0;
    float lookYawSpeed = 10.0F;
    float lookPitchSpeed = 40.0F;

    float headYaw = 0.0F;

    std::optional<std::array<double, 3>> walkTarget;

    std::optional<std::vector<std::array<std::int32_t, 3>>> path;
    std::int32_t pathIndex = 0;

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
    float aiSpeed = 0.0F;

    std::int32_t idleTicks = 0;

    bool persistent = false;

    bool noAi = false;

    std::optional<wire::nbt::NbtCompound> sourceNbt;
    /// Counts the changes made to `sourceNbt` after the entity was made, so the viewers are sent its data again.
    std::uint32_t sourceNbtVersion = 0;

    /// What plugins keep on the entity, saved as `PublicBukkitValues` and given back by `entity_data`.
    wire::nbt::NbtCompound pluginData;

    std::shared_ptr<enderDragon::DragonState> dragon;

    std::shared_ptr<fishingHooks::HookState> hook;

    std::shared_ptr<witherBoss::WitherState> wither;

    std::shared_ptr<wardenBrain::WardenState> warden;

    std::shared_ptr<copperGolems::GolemState> copperGolem;
    /// parity: `Bee`'s own fields. Absent for everything that is not one.
    std::shared_ptr<bees::BeeState> bee;
    /// parity: `Allay`'s own fields and brain. Absent for everything that is not one.
    std::shared_ptr<allays::AllayState> allay;
    std::shared_ptr<piglins::PiglinState> piglin;
    std::shared_ptr<mobMisc::State> misc;
    std::shared_ptr<neutralAnger::Anger> neutral;
    std::shared_ptr<hoglins::HoglinState> hoglin;
    std::shared_ptr<axolotls::AxolotlState> axolotl;
    std::shared_ptr<goats::GoatState> goat;
    std::shared_ptr<frogs::FrogState> frog;
    std::shared_ptr<frogs::TadpoleState> tadpole;
    std::shared_ptr<camels::CamelState> camel;
    std::shared_ptr<sniffers::SnifferState> sniffer;
    std::shared_ptr<armadillos::ArmadilloState> armadillo;
    std::shared_ptr<breezes::BreezeState> breeze;
    /// parity: `LivingEntity.discardFriction`, which a long jump turns on until it lands.
    bool discardFriction = false;
    bool smoothSwimming = false;

    std::shared_ptr<shulkers::ShulkerState> shulker;
    std::shared_ptr<shulkers::BulletState> bullet;

    /// Attribute base values that were changed from their default. parity: `AttributeMap`, without
    std::map<std::string, double, std::less<>> attributeValues;

    double health = kDefaultMaxHealth;

    std::vector<std::string> tags;

    /// parity: `Mob.lootTable` and `lootTableSeed`, the `DeathLootTable` a summon can name.
    std::optional<std::string> deathLootTable;
    std::int64_t deathLootTableSeed = 0;

    std::optional<std::int32_t> vehicleId;
    std::vector<std::int32_t> passengerIds;

    std::optional<std::string> customName;

    std::optional<std::string> displayText;

    std::optional<std::vector<std::string>> visibleTo;

    bool removed = false;

    bool killed = false;

    bool dying = false;

    bool noGravity = false;

    bool announced = false;

    std::int32_t deathTime = 0;

    std::uint8_t forcedFlags = 0;
    std::string spawnReason;
    float hurtDirection = 0.0F;
    bool collidable = true;
    std::int8_t forcedPose = -1;
    std::int8_t visualFire = -1;
    bool freezeLocked = false;
    bool jumping = false;
    std::int32_t portalCooldown = 0;
    std::int8_t activeSlot = -1;
    std::int32_t activeTicks = 0;
    std::int32_t activeUsed = 0;
    bool riptiding = false;
    std::int32_t maxNoDamageTicks = 20;
    std::optional<std::int32_t> maxAirOverride;

    std::map<std::string, std::map<std::string, AttributeModifier, std::less<>>, std::less<>> attributeModifiers;

    /// with, else the attribute's own default. parity: `AttributeMap` built from
    [[nodiscard]] double attribute(std::string_view id) const;

    [[nodiscard]] double attributeWithModifiers(std::string_view id) const;

    [[nodiscard]] double width() const;
    [[nodiscard]] double height() const;

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
