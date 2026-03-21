#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/wire/common/Identifier.hpp>

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
    std::optional<wire::common::Identifier> heldItem;
    bool sneaking = false;
    /// parity: `Player.isSwimming`, sprinting in water.
    bool swimming = false;
    /// Where they are looking, which is what an enderman reads.
    float yaw = 0.0F;
    float pitch = 0.0F;
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

    /// parity: `Roar.start`, the warden's roar at somebody it is about to fight.
    virtual void wardenRoar(ServerEntity& /*warden*/, std::int32_t /*targetId*/) {}

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

    /// parity: `Container.setChanged`: the chunk is saved and a comparator reads it again.
    virtual void containerChanged(ServerEntity& /*mob*/, std::int32_t /*x*/, std::int32_t /*y*/, std::int32_t /*z*/) {}

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
    /// parity: `Raid`, which is what puts a village in its hide activity.
    bool raidNearby = false;
    /// parity: `Attributes.MOVEMENT_SPEED` for this mob, with its modifiers.
    double speed = 0.0;

    [[nodiscard]] const PlayerView* nearestPlayer(double range) const;

    [[nodiscard]] double distanceSquaredTo(double x, double y, double z) const;

    [[nodiscard]] const PlayerView* playerById(std::int32_t entityId) const;

    [[nodiscard]] ServerEntity* entityById(std::int32_t entityId) const;
};

} // namespace basalt::server::runtime::entity::ai
