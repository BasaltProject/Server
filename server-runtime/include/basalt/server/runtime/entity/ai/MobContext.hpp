#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::world {
class WorldState;
}

namespace basalt::server::runtime::entity::ai {

/// A player, as the goals see one: where they are, and what they are holding.
struct PlayerView {
    std::int32_t entityId = 0;
    /// Their uuid, which is how a pet knows its own owner.
    std::string uuid;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double width = 0.0;
    double height = 0.0;
    /// parity: `TargetingConditions`, which skips a creative or spectating player outright.
    bool targetable = true;
    /// parity: `Entity.isSpectator`, whom nothing sees at all.
    bool spectator = false;
    std::optional<wire::common::Identifier> heldItem;
    bool sneaking = false;
    bool sprinting = false;
    /// parity: `Player.isSwimming`, sprinting in water.
    bool swimming = false;
    /// Where they are looking, which is what an enderman reads.
    float yaw = 0.0F;
    float pitch = 0.0F;
    /// The boat or other vehicle it steers, and whether it moved sideways last tick.
    /// parity: `getControllingPassenger` and `hasMovedHorizontallyRecently`.
    std::optional<std::int32_t> vehicleId;
    bool movedHorizontally = false;
    /// The way it faces, 0 south, 1 west, 2 north, 3 east. parity: `Entity.getDirection`.
    std::int32_t facing = 0;
    /// parity: `Player.isSleeping` and `getSleepTimer`, which a cat on the bed reads.
    bool sleeping = false;
    std::int32_t sleepTicks = 0;
    /// parity: `LivingEntity.PLAYER_NOT_WEARING_DISGUISE_ITEM`, inverted: a carved pumpkin on the head.
    bool gazeDisguised = false;
    /// parity: `Abilities.flying` and `Entity.isInWater`, which keep a parrot off the shoulder.
    bool flying = false;
    bool inWater = false;
};

/// What a goal can make the world do beyond moving its own mob.
///
/// An interface rather than a bundle of callbacks: a goal calls a dozen of these, and naming each
/// one is what keeps the seam readable. The runtime supplies the one implementation.
class MobActions {
public:
    MobActions() = default;
    MobActions(const MobActions&) = default;
    MobActions(MobActions&&) = default;
    MobActions& operator=(const MobActions&) = default;
    MobActions& operator=(MobActions&&) = default;
    virtual ~MobActions() = default;

    /// parity: `Mob.swing`, the arm the client draws before the hit lands.
    virtual void swing(ServerEntity& mob) = 0;

    virtual void hurtPlayer(ServerEntity& mob, std::int32_t playerId, float damage) = 0;

    virtual void hurtMob(ServerEntity& mob, ServerEntity& target, float damage) = 0;

    /// parity: `Animal.spawnChildFromBreeding`, which is the world's to do and not the goal's.
    virtual void breed(ServerEntity& mob, ServerEntity& partner) = 0;

    /// parity: `EatBlockGoal`, which eats the grass under or in front of the mob.
    virtual void eatGrass(ServerEntity& mob) = 0;

    /// parity: `SoundEvents.CREEPER_PRIMED`, the hiss when the fuse starts.
    virtual void primeCreeper(ServerEntity& mob) = 0;

    /// parity: `Creeper.explodeCreeper`, three blocks of blast where it stood.
    virtual void explodeCreeper(ServerEntity& mob) = 0;

    /// parity: `Ghast.setCharging` and `Blaze.setCharged`, the tell before a shot.
    virtual void setCharging(ServerEntity& mob, bool charging) = 0;

    /// parity: `LargeFireball` and `SmallFireball`, thrown from the mob at what it is aiming at.
    virtual void shootFireball(ServerEntity& mob, double targetX, double targetY, double targetZ, bool large) = 0;

    /// parity: `Guardian`'s beam, which the client draws from the mob to whatever it names.
    virtual void setBeamTarget(ServerEntity& mob, std::optional<std::int32_t> targetId) = 0;

    /// parity: `SpellcasterIllager.setIsCastingSpell`, the pose and the particles while it casts.
    virtual void setCasting(ServerEntity& mob, bool casting) = 0;

    /// parity: `Witch.performRangedAttack`, a splash potion thrown at somebody.
    virtual void throwPotion(ServerEntity& mob, double targetX, double targetY, double targetZ,
                             std::string_view potion) = 0;

    /// parity: `Evoker.EvokerAttackSpellGoal`, the two lines of fangs it opens under you.
    virtual void castFangs(ServerEntity& mob, double targetX, double targetY, double targetZ) = 0;

    /// parity: `Evoker.EvokerSummonSpellGoal`, which is where vexes come from.
    virtual void summonVexes(ServerEntity& mob, std::int32_t count) = 0;

    /// parity: an effect put on a player by something that is not a potion they drank.
    virtual void applyEffect(std::int32_t playerId, std::string_view effect, std::int32_t durationTicks,
                             std::int32_t amplifier) = 0;

    /// parity: `Villager` sleeping, which is a pose the client draws lying in the bed.
    virtual void setSleeping(ServerEntity& mob, bool sleeping) = 0;

    /// parity: `EnderMan.teleport`, which is a move plus the sound and particles at both ends.
    virtual void teleported(ServerEntity& mob) = 0;

    /// parity: `AbstractSkeleton.performRangedAttack`, an arrow at 1.6 with the difficulty's spread.
    virtual void shootArrow(ServerEntity& mob, double targetX, double targetY, double targetZ, float power) = 0;

    /// parity: `Llama.spit`, aimed at a third of the target's height, with its sound.
    virtual void llamaSpit(ServerEntity& /*llama*/, double /*targetX*/, double /*targetY*/, double /*targetZ*/,
                           double /*targetHeight*/) {}

    /// parity: `Roar.start`, the warden's roar at somebody it is about to fight.
    virtual void wardenRoar(ServerEntity& /*warden*/, std::int32_t /*targetId*/) {}

    /// parity: `Warden.increaseAngerAt`, with the listening sound it makes when `sound` is set.
    virtual void wardenAngered(ServerEntity& /*warden*/, std::int32_t /*suspectId*/, std::int32_t /*amount*/,
                               bool /*sound*/) {}

    /// parity: `Container.startOpen` and `stopOpen` from a copper golem: the lid and its sound.
    virtual void golemOpensChest(ServerEntity& /*golem*/, std::int32_t /*x*/, std::int32_t /*y*/, std::int32_t /*z*/,
                                 bool /*open*/) {}

    /// parity: `getEntitiesWithContainerOpen`, whether anybody has this chest open now.
    [[nodiscard]] virtual bool chestInUse(const ServerEntity& /*mob*/, std::int32_t /*x*/, std::int32_t /*y*/,
                                          std::int32_t /*z*/) {
        return false;
    }

    /// parity: `Entity.playSound` from the mob itself.
    virtual void mobSound(ServerEntity& /*mob*/, std::string_view /*sound*/) {}

    /// What the mob holds changed, so everyone watching has to see it.
    virtual void heldItemChanged(ServerEntity& /*mob*/) {}

    /// A pet's sitting or lying pose changed, so everyone watching has to see it.
    virtual void poseChanged(ServerEntity& /*mob*/) {}

    /// parity: `ShoulderRidingEntity.setEntityOnShoulder`. Answers whether it sat down.
    virtual bool sitOnShoulder(ServerEntity& /*parrot*/, std::int32_t /*playerId*/) { return false; }

    /// parity: `Container.setChanged`: the chunk is saved and a comparator reads it again.
    virtual void containerChanged(ServerEntity& /*mob*/, std::int32_t /*x*/, std::int32_t /*y*/, std::int32_t /*z*/) {}

    /// An item lying in the world, as a goal sees it.
    struct ItemView {
        std::int32_t entityId = 0;
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
    };

    /// parity: `Dolphin.ALLOWED_ITEMS` in the mob's box grown by `grow`: alive, in water and free to
    /// take, in the level's own order.
    [[nodiscard]] virtual std::vector<ItemView> itemsInWater(const ServerEntity& /*mob*/, double /*grow*/) {
        return {};
    }

    /// parity: `Dolphin.ItemGoal.drop`, the held stack thrown ahead with forty ticks before anyone may
    /// take it. Answers whether it held anything.
    virtual bool throwHeldItem(ServerEntity& /*mob*/) { return false; }

    /// parity: `ServerLevel.findNearestMapStructure(#dolphin_located, pos, 50, false)`.
    [[nodiscard]] virtual std::optional<std::array<std::int32_t, 3>> nearestTreasure(const ServerEntity& /*mob*/) {
        return std::nullopt;
    }

    /// parity: `Level.broadcastEntityEvent`.
    virtual void entityEvent(ServerEntity& /*mob*/, std::int8_t /*event*/) {}

    /// parity: `AbstractHorse.tameWithName`, for the player riding it.
    virtual void tameHorse(ServerEntity& /*horse*/, std::int32_t /*playerId*/) {}

    /// parity: `RunAroundLikeCrazyGoal`'s `ejectPassengers`, `makeMad` and the smoke.
    virtual void throwRiders(ServerEntity& /*horse*/) {}

    /// parity: `RandomStandGoal.start`: up on its hind legs with its ambient sound.
    virtual void horseRears(ServerEntity& /*horse*/) {}

    /// parity: `LivingEntity.dropFromGiftLootTable`: a gift table rolled for this mob, dropped in front
    /// of it.
    virtual void dropGift(ServerEntity& /*mob*/, std::string_view /*table*/) {}

    /// parity: `Shulker.setRawPeekAmount`: the lid, its armour, its sound and its game event.
    virtual void shulkerPeek(ServerEntity& /*shulker*/, std::int32_t /*amount*/) {}

    /// parity: `ShulkerAttackGoal`'s `new ShulkerBullet(level, shulker, target, axis)` and its sound.
    virtual void shootShulkerBullet(ServerEntity& /*shulker*/, std::int32_t /*targetId*/) {}

    /// parity: `PathfindToRaidGoal.recruitNearby`, the idle raiders around this one joining its raid.
    virtual void recruitRaiders(ServerEntity& /*raider*/) {}

    /// parity: `Raider.pickUpItem` for an ominous banner, which makes the raider its wave's captain.
    virtual void takeRaidBanner(ServerEntity& /*raider*/, std::int32_t /*itemId*/) {}

    /// parity: `Raider.setCelebrating`.
    virtual void setCelebrating(ServerEntity& /*raider*/, bool /*celebrating*/) {}

    /// A hive as a bee sees it. parity: `getBeehiveBlockEntity`, `isFull` and `isFireNearby`.
    struct HiveView {
        bool loaded = false;
        bool hive = false;
        bool full = false;
        bool fireNearby = false;
    };

    [[nodiscard]] virtual HiveView hiveAt(const ServerEntity& /*bee*/, const std::array<std::int32_t, 3>& /*pos*/) {
        return {};
    }

    /// parity: `BeehiveBlockEntity.addOccupant`, which takes the bee out of the world.
    virtual void enterHive(ServerEntity& /*bee*/, const std::array<std::int32_t, 3>& /*hive*/) {}

    /// parity: `PoiManager.getInRange(#bee_home, pos, radius, ANY)`, in the manager's own order.
    [[nodiscard]] virtual std::vector<std::array<std::int32_t, 3>> beeHomesNear(const ServerEntity& /*bee*/,
                                                                                std::int32_t /*radius*/) {
        return {};
    }

    /// parity: `EnvironmentAttributes.BEES_STAY_IN_HIVE` at the bee, and `Level.isRaining`.
    [[nodiscard]] virtual bool beesStayInHive(const ServerEntity& /*bee*/) { return false; }

    [[nodiscard]] virtual bool raining(const ServerEntity& /*bee*/) { return false; }

    /// parity: `BeeGrowCropGoal.tick` on one square below the bee. Answers whether it grew.
    virtual bool growBeeCrop(ServerEntity& /*bee*/, const std::array<std::int32_t, 3>& /*pos*/) { return false; }

    /// Whether a block is in a block tag, which 26.3 decides teleports and jumps with. Without the
    /// server's tags nothing is in any, which a goal reads as the tag-free rule.
    [[nodiscard]] virtual std::optional<bool> blockTagged(std::string_view /*tag*/,
                                                          const wire::common::Identifier& /*block*/) const {
        return std::nullopt;
    }
};

/// Everything one mob's goals are allowed to see this tick.
///
/// The mob and the world are the two halves vanilla's goals hold as fields; the rest is what this
/// build has to hand them explicitly, since a goal here cannot reach into the server.
class MobContext {
public:
    world::WorldState& world;
    ServerEntity& mob;
    random::RandomSource& random;
    const std::vector<ServerEntity*>& entities;
    const std::vector<PlayerView>& players;
    MobActions& actions;
    /// parity: `Level.getMaxLocalRawBrightness`, which is what the light-shy goals ask.
    std::int32_t light = 0;
    bool night = false;
    /// parity: the villager schedule reads the day's own time rather than the total.
    std::int64_t dayTime = 0;
    /// parity: `Level.getGameTime`, used by absolute AI cooldown deadlines.
    std::int64_t gameTime = 0;
    /// parity: `GameRules.UNIVERSAL_ANGER`, which the target goals of neutral mobs read.
    bool universalAnger = false;

    /// parity: `Raider.getCurrentRaid`, as a raider's goals read it.
    struct RaiderRaid {
        /// parity: `Raid.isActive`, false for a raid that was stopped under it.
        bool active = false;
        bool over = false;
        bool loss = false;
        std::array<std::int32_t, 3> center{};
        /// parity: `getLeader(wave)`, alive.
        bool leaderAlive = false;
    };

    std::optional<RaiderRaid> raid;
    /// parity: `Attributes.MOVEMENT_SPEED` for this mob, with its modifiers.
    double speed = 0.0;
    /// parity: `Level.getSkyDarken` and the dimension's sky and `ambientLight`, what the light of
    /// a square elsewhere is read with.
    std::int32_t skyDarken = 0;
    bool hasSkylight = true;
    float ambientLight = 0.0F;
    /// parity: `Level.getSeaLevel` and `getMinY`, which a phantom's anchor and a bat's aim read.
    std::int32_t seaLevel = 63;
    std::int32_t minY = -64;

    [[nodiscard]] const PlayerView* nearestPlayer(double range) const;

    [[nodiscard]] double distanceSquaredTo(double x, double y, double z) const;

    [[nodiscard]] const PlayerView* playerById(std::int32_t entityId) const;

    [[nodiscard]] ServerEntity* entityById(std::int32_t entityId) const;
};

} // namespace basalt::server::runtime::entity::ai
