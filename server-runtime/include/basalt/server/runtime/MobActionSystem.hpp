#pragma once

#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/MobConversions.hpp>
#include <basalt/server/runtime/entity/WardenBrain.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/RealEntityTypeTags.hpp>
#include <basalt/server/world/loot/Loot.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerRegistry;
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::world {
class LoadedWorld;
class RealLootTables;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime::world {
class RealRecipes;
}

namespace basalt::server::runtime {

class BlockChangeBroadcaster;
class CombatSystem;
class ExplosionSystem;
class NaturalSpawningSystem;
class RaidSystem;

/// What a goal is allowed to make the world do (swing, hit, breed, eat, ...), plus the rest of a
/// mob's own turn: fire and sunlight, the AI step itself, breeding and the loot it drops on death.
///
/// The goals cannot reach the server themselves, so implementing `MobActions` here is the whole of
/// their arm's length.
class MobActionSystem final : public entity::ai::MobActions {
public:
    /// What a plugin gets a say in. Callbacks rather than the plugin bus itself, and unset means
    /// nobody objects.
    struct Hooks {
        /// The hooks `entityCombat::hurt` itself consults, passed straight through.
        entity::entityCombat::Hooks combat;
        /// Whether a mob may catch fire in the sun.
        std::function<bool(const world::LoadedWorld& world, const entity::ServerEntity& mob, std::int32_t fireTicks,
                           std::string_view cause)>
            combustAllowed;
        /// Whether these two animals may have a calf.
        std::function<bool(const world::LoadedWorld& world, const entity::ServerEntity& animal,
                           const entity::ServerEntity& partner)>
            breedAllowed;
        /// One experience orb of that value. parity: `new ExperienceOrb`.
        std::function<void(world::LoadedWorld& world, double x, double y, double z, std::int32_t value)> spawnOrb;
        /// A wither's middle head firing at its target, which is its ranged attack.
        std::function<void(world::LoadedWorld& world, entity::ServerEntity& wither)> witherShoots;
        /// A chest's viewer count changed because a golem opened or left it: the lid and its sound.
        std::function<void(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                           std::int32_t openers)>
            openersChanged;
    };

    /// The rolls a mob's turn needs, none of which is part of the level's own stream.
    struct Options {
        std::function<std::int64_t()> nextSeed;
        /// The entity type tags, which a death roll asks about its killer.
        const world::realEntityTypeTags::TagMembers* entityTags = nullptr;
        std::function<double()> nextDouble;
        /// What a furnace makes of an item, which a mob that burned to death drops.
        const world::RealRecipes* recipes = nullptr;
        /// The damage type tags, which a death roll asks about what killed it.
        const world::realDamageTypeTags::TagMembers* damageTypeTags = nullptr;
    };

    /// Which worlds a mob might be in, since a goal only ever hands over the mob itself.
    using Worlds = std::function<std::vector<world::LoadedWorld*>()>;

    MobActionSystem(player::PlayerRegistry& players, BlockChangeBroadcaster& broadcaster, ExplosionSystem& explosions,
                    NaturalSpawningSystem& spawning, CombatSystem& combat, RaidSystem& raids,
                    world::RealLootTables& lootTables, Worlds worlds, Hooks hooks, Options options);

    void swing(entity::ServerEntity& mob) override;

    void hurtPlayer(entity::ServerEntity& mob, std::int32_t playerId, float damage) override;

    void hurtMob(entity::ServerEntity& mob, entity::ServerEntity& target, float damage) override;

    void breed(entity::ServerEntity& mob, entity::ServerEntity& partner) override;

    void eatGrass(entity::ServerEntity& mob) override;

    void primeCreeper(entity::ServerEntity& mob) override;

    void explodeCreeper(entity::ServerEntity& mob) override;

    void setCharging(entity::ServerEntity& mob, bool charging) override;

    void shootFireball(entity::ServerEntity& mob, double targetX, double targetY, double targetZ, bool large) override;

    void setBeamTarget(entity::ServerEntity& mob, std::optional<std::int32_t> targetId) override;

    void setCasting(entity::ServerEntity& mob, bool casting) override;

    void throwPotion(entity::ServerEntity& mob, double targetX, double targetY, double targetZ,
                     std::string_view potion) override;

    void castFangs(entity::ServerEntity& mob, double targetX, double targetY, double targetZ) override;

    void summonVexes(entity::ServerEntity& mob, std::int32_t count) override;

    void applyEffect(std::int32_t playerId, std::string_view effect, std::int32_t durationTicks,
                     std::int32_t amplifier) override;

    void setSleeping(entity::ServerEntity& mob, bool sleeping) override;

    void teleported(entity::ServerEntity& mob) override;

    void shootArrow(entity::ServerEntity& mob, double targetX, double targetY, double targetZ, float power) override;

    void wardenRoar(entity::ServerEntity& warden, std::int32_t targetId) override;

    void golemOpensChest(entity::ServerEntity& golem, std::int32_t x, std::int32_t y, std::int32_t z,
                         bool open) override;

    [[nodiscard]] bool chestInUse(const entity::ServerEntity& mob, std::int32_t x, std::int32_t y,
                                  std::int32_t z) override;

    void mobSound(entity::ServerEntity& mob, std::string_view sound) override;

    void heldItemChanged(entity::ServerEntity& mob) override;

    void containerChanged(entity::ServerEntity& mob, std::int32_t x, std::int32_t y, std::int32_t z) override;

    /// The copper golem's own turn: its weathering, the statue it may become and the cooldown its
    /// chest rounds wait on. parity: `CopperGolem.tick` and the brain's core countdown.
    void tickCopperGolem(world::LoadedWorld& world, entity::ServerEntity& golem, random::RandomSource& random);

    /// What using an item on a copper golem did to the item. parity: `CopperGolem.mobInteract`.
    enum class GolemUse : std::uint8_t { Pass, Done, ConsumeOne, DamageTool };

    [[nodiscard]] GolemUse useOnCopperGolem(world::LoadedWorld& world, player::PlayerSession& player,
                                            entity::ServerEntity& golem,
                                            const std::optional<wire::common::Identifier>& held);

    /// parity: `CopperGolem.thunderHit`.
    void golemStruckByLightning(entity::ServerEntity& golem, std::int32_t boltId);

    /// parity: a warden summoned by a shrieker, which rises out of the ground.
    void wardenEmerges(world::LoadedWorld& world, entity::ServerEntity& warden);

    /// parity: `Warden.hurtServer`'s own reaction to whoever hit it.
    static void wardenStruck(entity::ServerEntity& warden, std::int32_t attackerId, bool direct,
                             double distanceSquared);

    /// The warden's anger, timed behaviours and darkness. parity: `Warden.customServerAiStep`.
    void tickWarden(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                    entity::ServerEntity& warden);

    /// parity: `SnowGolem.aiStep`, the layer of snow it leaves on every square it stands on.
    void trailSnow(world::LoadedWorld& world, const entity::ServerEntity& golem);

    /// The undead burning in the sun, and what burning costs anything that is on fire.
    void tickFire(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                  entity::ServerEntity& mob, random::RandomSource& random);

    /// A zombie drowning, a skeleton freezing or a piglin out of the Nether turning into another
    /// mob. Answers whether this mob is gone because it did.
    bool tickConversion(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                        entity::ServerEntity& mob);

    /// Turns the mob into another in place, `finish` touching the new one's saved form first.
    /// parity: `Mob.convertTo`.
    bool convertMob(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                    entity::ServerEntity& mob, const entity::mobConversions::Conversion& conversion,
                    const std::function<void(wire::nbt::NbtCompound& converted)>& finish);

    /// A golden apple given to a zombie villager. Answers whether it started the cure and the apple
    /// is spent. parity: `ZombieVillager.mobInteract`.
    bool startCure(world::LoadedWorld& world, entity::ServerEntity& mob, const wire::common::Uuid& player);

    /// parity: `Zombie.killedEntity`: a villager a zombie kills may rise as one. Answers whether it did.
    bool infectVillager(world::LoadedWorld& world, const entity::ServerEntity& zombie, entity::ServerEntity& villager);

    /// parity: `Entity.thunderHit` and its overrides: what a bolt does to a mob it touches.
    void thunderHit(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                    entity::ServerEntity& mob, std::int32_t boltId);

    /// Gives a mob an effect, applying an instant one on the spot. Answers whether it took.
    /// parity: `LivingEntity.addEffect`.
    bool addMobEffect(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                      entity::ServerEntity& mob, const world::ActiveEffect& effect, std::int32_t sourceId = -1);

    /// parity: `LivingEntity.tickEffects`.
    void tickMobEffects(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                        entity::ServerEntity& mob);

    /// The mob's air, and a dolphin drying out on land and flopping about there.
    /// parity: the air half of `LivingEntity.baseTick`, the `handleAirSupply` overrides and
    /// `Dolphin.tick`.
    void tickBreath(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                    entity::ServerEntity& dolphin, random::RandomSource& random);

    /// parity: `LivingEntity.aiStep`'s travel half, which runs for a mob this build has no goals
    /// for as much as for one it does.
    void travelMob(world::LoadedWorld& world, entity::ServerEntity& mob, double speed);

    /// One mob's turn to decide and then move.
    void tickMobAi(world::LoadedWorld& world, entity::ServerEntity& mob,
                   const std::vector<entity::ai::PlayerView>& views, const std::vector<entity::ServerEntity*>& living,
                   random::RandomSource& random);

    /// The TRIGGERED rider forest a thunder horse becomes when somebody walks up to it.
    void activateSkeletonTrap(world::LoadedWorld& world, entity::ServerEntity& horse, random::RandomSource& random);

    void tickVillageGolem(world::LoadedWorld& world, entity::ServerEntity& villager,
                          const std::vector<entity::ServerEntity*>& living, random::RandomSource& random);

    /// parity: `Entity.die` over `LootTable`, the drops a mob leaves where it fell.
    void dropDeathLoot(world::LoadedWorld& world, const entity::ServerEntity& entity);

    /// parity: `Animal.spawnChildFromBreeding`: the calf appears where its parent stands.
    void breed(world::LoadedWorld& world, entity::ServerEntity& animal, entity::ServerEntity& partner);

    /// Whether somebody is stood close enough to a cart to shove it. parity: two boxes touching.
    [[nodiscard]] static bool touchesCart(const entity::ServerEntity& cart, const player::PlayerSession& player);

private:
    /// The world this mob is in, which is the one thing a goal never hands over.
    [[nodiscard]] world::LoadedWorld* worldOf(const entity::ServerEntity& mob);

    [[nodiscard]] std::vector<player::PlayerSession*> watchersOf(const world::LoadedWorld& world) const;

    /// One entity-data entry to everybody watching, which is how a mob's own flags travel.
    void sendEntityData(const world::LoadedWorld& world, std::int32_t entityId,
                        const wire::protocol::play::SetEntityDataPacket::Entry& entry);

    void playSound(const world::LoadedWorld& world, const entity::ServerEntity& mob, std::string_view sound,
                   float volume = 1.0F, float pitch = 1.0F);

    /// parity: `EatBlockGoal`: the grass in front becomes air and the block below becomes dirt.
    void eatGrassUnder(world::LoadedWorld& world, const entity::ServerEntity& mob);

    /// The light a mob is standing in, which is all the spiders need to know.
    [[nodiscard]] static std::int32_t lightAt(world::LoadedWorld& world, const std::array<std::int32_t, 3>& position);

    [[nodiscard]] random::RandomSource freshRandom() const;

    /// Whether a warden's swing may land now, running what the swing sets off when it does.
    bool wardenSwings(world::LoadedWorld& world, entity::ServerEntity& mob);

    void runWardenActions(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                          entity::ServerEntity& warden, const std::vector<entity::wardenBrain::Action>& actions);

    /// parity: `CopperGolem.turnToStatue`.
    void turnToStatue(world::LoadedWorld& world, entity::ServerEntity& golem, random::RandomSource& random);

    /// parity: `SonicBoom.tick`'s blast.
    void sonicBlast(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                    entity::ServerEntity& warden);

    /// What a death roll knows about the one that died and whoever killed it.
    [[nodiscard]] ::basalt::server::world::loot::LootContext deathLootContext(world::LoadedWorld& world,
                                                                              const entity::ServerEntity& dead) const;

    [[nodiscard]] std::int32_t cureProgress(world::LoadedWorld& world, const entity::ServerEntity& mob);

    player::PlayerRegistry* players_;
    BlockChangeBroadcaster* broadcaster_;
    ExplosionSystem* explosions_;
    NaturalSpawningSystem* spawning_;
    CombatSystem* combat_;
    RaidSystem* raids_;
    world::RealLootTables* lootTables_;
    Worlds worlds_;
    Hooks hooks_;
    Options options_;
};

} // namespace basalt::server::runtime
