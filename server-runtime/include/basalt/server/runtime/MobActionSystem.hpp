#pragma once

#include <basalt/server/runtime/VillagerWorldSystem.hpp>
#include <basalt/server/runtime/entity/Creakings.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/Leashes.hpp>
#include <basalt/server/runtime/entity/MobConversions.hpp>
#include <basalt/server/runtime/entity/WardenBrain.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/world/Beehives.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/FallLanding.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/RealEntityTypeTags.hpp>
#include <basalt/server/runtime/world/RealItemTags.hpp>
#include <basalt/server/world/loot/Loot.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerRegistry;
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::world {
class LoadedWorld;
class AdvancementTriggers;
class RealLootTables;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime::world {
class RealRecipes;
}

namespace basalt::server::runtime::entity::allays {
struct AllayState;
}

namespace basalt::server::runtime::entity::piglins {
struct PiglinState;
}

namespace basalt::server::runtime::entity::neutralAnger {
struct Anger;
}

namespace basalt::server::runtime::entity::frogs {
struct FrogState;
}

namespace basalt::server::runtime::entity::sniffers {
struct SnifferState;
}

namespace basalt::server::runtime::entity::armadillos {
struct ArmadilloState;
}

namespace basalt::server::runtime::entity::breezes {
struct BreezeState;
}

namespace basalt::server::runtime::entity::goats {
struct GoatState;
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
        /// The player who fed the parents, told what came of it: their stat and their advancement.
        /// parity: `finalizeSpawnChildFromBreeding`.
        std::function<void(std::string_view cause, const wire::common::Identifier& parent,
                           const wire::common::Identifier& partner, const wire::common::Identifier& child)>
            bredAnimals;
        /// One experience orb of that value. parity: `new ExperienceOrb`.
        std::function<void(world::LoadedWorld& world, double x, double y, double z, std::int32_t value)> spawnOrb;
        /// A wither's middle head firing at its target, which is its ranged attack.
        std::function<void(world::LoadedWorld& world, entity::ServerEntity& wither)> witherShoots;
        /// A chest's viewer count changed because a golem opened or left it: the lid and its sound.
        std::function<void(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                           std::int32_t openers)>
            openersChanged;
        /// A converted mob takes the old one's scoreboard team, by uuid string.
        std::function<void(const std::string& from, const std::string& to)> teamCarried;
        /// parity: `CriteriaTriggers.CURED_ZOMBIE_VILLAGER`, for whoever gave the apple.
        std::function<void(const wire::common::Uuid& player)> curedZombieVillager;
        /// A firework rocket a mob set off, from the item it was made of.
        std::function<void(world::LoadedWorld& world, wire::nbt::NbtCompound item, double x, double y, double z,
                           std::int32_t ownerId)>
            launchFirework;
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
        /// The block tags, which decide where a mob may teleport to.
        const world::realBlockTags::TagMembers* blockTags = nullptr;
        /// The item tags, which a piglin's likes and fears are read from.
        const world::realItemTags::TagMembers* itemTags = nullptr;
        /// The jar's trade sets, which a villager's offers are rolled from.
        world::RealVillagerTrades* villagerTrades = nullptr;
        /// Where the triggers a mob's doing earns a player go: a hive broken, an allay's delivery.
        world::AdvancementTriggers* advancementTriggers = nullptr;
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

    void poseChanged(entity::ServerEntity& mob) override;

    void throwPotion(entity::ServerEntity& mob, double targetX, double targetY, double targetZ,
                     std::string_view potion) override;

    void castFangs(entity::ServerEntity& mob, double targetX, double targetY, double targetZ) override;

    void summonVexes(entity::ServerEntity& mob, std::int32_t count) override;

    void applyEffect(std::int32_t playerId, std::string_view effect, std::int32_t durationTicks, std::int32_t amplifier,
                     std::optional<std::int32_t> sourceId) override;

    void setSleeping(entity::ServerEntity& mob, bool sleeping) override;

    void teleported(entity::ServerEntity& mob) override;

    [[nodiscard]] std::optional<bool> blockTagged(std::string_view tag,
                                                  const wire::common::Identifier& block) const override;

    void shootArrow(entity::ServerEntity& mob, double targetX, double targetY, double targetZ, float power) override;

    void llamaSpit(entity::ServerEntity& llama, double targetX, double targetY, double targetZ,
                   double targetHeight) override;

    void wardenRoar(entity::ServerEntity& warden, std::int32_t targetId) override;

    void wardenAngered(entity::ServerEntity& warden, std::int32_t suspectId, std::int32_t amount, bool sound) override;

    /// parity: the warden's `VibrationSystem`: what it hears, the travel, and `onReceiveVibration`.
    void wardenHears(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                     entity::ServerEntity& warden);

    void golemOpensChest(entity::ServerEntity& golem, std::int32_t x, std::int32_t y, std::int32_t z,
                         bool open) override;

    [[nodiscard]] bool chestInUse(const entity::ServerEntity& mob, std::int32_t x, std::int32_t y,
                                  std::int32_t z) override;

    void mobSound(entity::ServerEntity& mob, std::string_view sound) override;

    void heldItemChanged(entity::ServerEntity& mob) override;

    void containerChanged(entity::ServerEntity& mob, std::int32_t x, std::int32_t y, std::int32_t z) override;

    void dropGift(entity::ServerEntity& mob, std::string_view table) override;

    [[nodiscard]] std::vector<ItemView> itemsInWater(const entity::ServerEntity& mob, double grow) override;

    bool throwHeldItem(entity::ServerEntity& mob) override;

    [[nodiscard]] std::optional<std::array<std::int32_t, 3>> nearestTreasure(const entity::ServerEntity& mob) override;

    void entityEvent(entity::ServerEntity& mob, std::int8_t event) override;

    void tameHorse(entity::ServerEntity& horse, std::int32_t playerId) override;

    void throwRiders(entity::ServerEntity& horse) override;

    void horseRears(entity::ServerEntity& horse) override;

    /// parity: `AbstractHorse.makeMad`: up on its hind legs and its angry sound.
    void makeHorseMad(entity::ServerEntity& horse);

    /// parity: the rest of `AbstractHorse.aiStep` and `tick` after the goals: healing, grazing, the
    /// mouth and the rearing running out, and a camel's dash cooling down.
    void tickHorse(world::LoadedWorld& world, entity::ServerEntity& horse, random::RandomSource& random);

    /// parity: `Creaking.tick`, `aiStep` and its brain over `CreakingAi`.
    void tickCreaking(world::LoadedWorld& world, entity::ai::MobContext& context);

    /// parity: `Creaking.tearDown`.
    void tearDownCreaking(world::LoadedWorld& world, entity::ServerEntity& creaking);

    /// parity: `CreakingHeartBlockEntity.serverTick`. Answers whether the heart's tag changed.
    bool tickCreakingHeart(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                           std::vector<world::BlockChange>& pending);

    /// parity: `CreakingHeartBlock.playerWillDestroy`, which kills its creaking at the player's hand.
    void creakingHeartBroken(world::LoadedWorld& world, const player::PlayerSession& player,
                             const std::array<std::int32_t, 3>& at);

    /// parity: `Mob.aiStep`'s looting and `Dolphin.pickUpItem`: an empty mouth takes what it touches.
    void dolphinPicksUp(world::LoadedWorld& world, entity::ServerEntity& dolphin);

    void shulkerPeek(entity::ServerEntity& shulker, std::int32_t amount) override;

    void shootShulkerBullet(entity::ServerEntity& shulker, std::int32_t targetId) override;

    void recruitRaiders(entity::ServerEntity& raider) override;

    void takeRaidBanner(entity::ServerEntity& raider, std::int32_t itemId) override;

    void setCelebrating(entity::ServerEntity& raider, bool celebrating) override;

    bool sitOnShoulder(entity::ServerEntity& parrot, std::int32_t playerId) override;

    /// parity: `Entity.move`'s tail for a mob: the fall counted and landed, the footsteps, the swimming.
    void emitMovement(world::LoadedWorld& world, entity::ServerEntity& mob);

    /// parity: the same for every player, whose footsteps and strokes everybody else hears.
    void emitPlayerMovement(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers);

    /// parity: `Leashable.tickLeash`: the holder found again, the rope snapping, pulling or walked.
    void tickLeash(world::LoadedWorld& world, entity::ServerEntity& leashed);

    /// What a click did with a lead, shears or neither.
    enum class LeashUse : std::uint8_t { Pass, Done, ConsumeOne, DamageShears };

    /// parity: `Entity.interact`'s leash half, before the mob's own `mobInteract`.
    LeashUse useOnLeashable(world::LoadedWorld& world, player::PlayerSession& player, entity::ServerEntity& entity,
                            bool holdingLead, bool holdingShears);

    /// parity: `LeashFenceKnotEntity.interact`.
    LeashUse useOnKnot(world::LoadedWorld& world, player::PlayerSession& player, entity::ServerEntity& knot,
                       bool holdingShears);

    /// parity: `LeadItem.bindPlayerMobs`, the player's mobs tied to a knot on this fence.
    bool bindPlayerMobs(world::LoadedWorld& world, player::PlayerSession& player,
                        const std::array<std::int32_t, 3>& fence);

    /// parity: `Entity.dropAllLeashConnections(null)` for a player. Answers whether any was held.
    bool dropAllLeashes(world::LoadedWorld& world, player::PlayerSession& player);

    /// parity: `Leashable.dropLeash` and `removeLeash`, the lead dropped as an item or not.
    void dropLeash(world::LoadedWorld& world, entity::ServerEntity& leashed, bool dropLead);

    /// parity: `Entity.propagateFallToPassengers`.
    void propagateFall(world::LoadedWorld& world, entity::ServerEntity& vehicle, double fallen,
                       const world::fallLanding::Landing& landing);

    /// parity: `Camel.tickRidden`, the rider's forward key standing it up.
    void camelRidden(world::LoadedWorld& world, entity::ServerEntity& camel, const player::PlayerSession& rider);

    /// parity: `Parrot.aiStep`'s one in four hundred imitation of a mob nearby.
    void tickParrot(world::LoadedWorld& world, entity::ServerEntity& parrot);

    /// parity: `ServerPlayer.handleShoulderEntities` and `removeEntitiesOnShoulder` for every player.
    void tickShoulders(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers);

    /// parity: `Shulker.tick`: the reaction to a hit, a new face or a jump when it lost its hold,
    /// and the lid easing towards its peek.
    void tickShulker(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                     const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& shulker,
                     random::RandomSource& random);

    /// parity: `ShulkerBullet.tick`.
    void tickShulkerBullet(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                           const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& bullet,
                           random::RandomSource& random);

    /// The copper golem's own turn: its weathering, the statue it may become and the cooldown its
    /// chest rounds wait on. parity: `CopperGolem.tick` and the brain's core countdown.
    void tickCopperGolem(world::LoadedWorld& world, entity::ServerEntity& golem, random::RandomSource& random);

    /// What using an item on a copper golem did to the item. parity: `CopperGolem.mobInteract`.
    enum class GolemUse : std::uint8_t { Pass, Done, ConsumeOne, DamageTool };

    [[nodiscard]] GolemUse useOnCopperGolem(world::LoadedWorld& world, player::PlayerSession& player,
                                            entity::ServerEntity& golem,
                                            const std::optional<wire::common::Identifier>& held);

    /// An axe on a statue that never weathered wakes the golem in it. Answers whether one rose.
    /// parity: `WeatheringCopperGolemStatueBlock.useItemOn` over `removeStatue`.
    bool awakenStatue(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

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

    /// parity: `Ravager.aiStep`: held still while it attacks, is stunned or roars, and the roar.
    void tickRavager(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                     entity::ServerEntity& ravager);

    /// parity: `Ravager.doHurtTarget`, the swing that holds it still for half a second.
    void ravagerSwings(world::LoadedWorld& world, entity::ServerEntity& ravager);

    /// parity: `LivingEntity.aiStep`'s travel half, which runs for a mob this build has no goals
    /// for as much as for one it does.
    void travelMob(world::LoadedWorld& world, entity::ServerEntity& mob, double speed,
                   random::RandomSource* random = nullptr,
                   const std::optional<std::array<double, 3>>& target = std::nullopt);

    /// parity: `Vex.tick`, the summoned vex starving once its life is up.
    void tickVexLife(const std::vector<player::PlayerSession*>& watchers, entity::ServerEntity& vex);

    /// parity: `Blaze.customServerAiStep`, the lift towards a target above it.
    static void blazeRises(entity::ServerEntity& blaze, std::optional<double> targetEye, random::RandomSource& random);

    /// One mob's turn to decide and then move.
    void tickMobAi(world::LoadedWorld& world, entity::ServerEntity& mob,
                   const std::vector<entity::ai::PlayerView>& views, const std::vector<entity::ServerEntity*>& living,
                   random::RandomSource& random);

    /// The TRIGGERED rider forest a thunder horse becomes when somebody walks up to it.
    void activateSkeletonTrap(world::LoadedWorld& world, entity::ServerEntity& horse, random::RandomSource& random);

    /// What a villager's brain does to this world, bound to it for the calls that follow.
    [[nodiscard]] entity::villager::VillagerWorld& villagersIn(world::LoadedWorld& world);

    /// parity: `Villager.die`: witnesses hear of the killer and the villager's spots are let go.
    void villagerDied(world::LoadedWorld& world, entity::ServerEntity& villager,
                      const std::optional<std::string>& killerUuid);

    /// parity: `Villager.releaseAllPois`, for a villager turned into something else.
    void releaseVillagerPois(world::LoadedWorld& world, entity::ServerEntity& villager);

    /// parity: `Entity.die` over `LootTable`, the drops a mob leaves where it fell.
    void dropDeathLoot(world::LoadedWorld& world, const entity::ServerEntity& entity);

    /// parity: `Animal.spawnChildFromBreeding`: the calf appears where its parent stands.
    void breed(world::LoadedWorld& world, entity::ServerEntity& animal, entity::ServerEntity& partner);

    /// parity: `Allay.customServerAiStep` over `AllayAi`, with the rest of `Allay.aiStep`.
    void tickAllay(world::LoadedWorld& world, entity::ai::MobContext& context);

    /// What using an item on an allay did with it. parity: `Allay.mobInteract`.
    enum class AllayUse : std::uint8_t { Pass, Done, ConsumeOne };

    [[nodiscard]] AllayUse useOnAllay(world::LoadedWorld& world, player::PlayerSession& player,
                                      entity::ServerEntity& allay,
                                      const std::optional<server::world::loot::ItemStack>& held, bool mainHand);

    /// parity: `Allay.dropEquipment`'s inventory, spilled where it died.
    void dropAllayInventory(world::LoadedWorld& world, const entity::ServerEntity& allay);

    /// parity: `Piglin.customServerAiStep` and `PiglinBrute.customServerAiStep` over their brains.
    void tickPiglin(world::LoadedWorld& world, entity::ai::MobContext& context);

    /// parity: `PiglinAi.mobInteract`: a gold ingot handed over to admire. Answers whether it took one.
    [[nodiscard]] bool useOnPiglin(world::LoadedWorld& world, player::PlayerSession& player,
                                   entity::ServerEntity& piglin,
                                   const std::optional<server::world::loot::ItemStack>& held);

    /// parity: `PiglinAi.angerNearbyPiglins`, for a guarded container opened or a guarded block broken.
    void angerNearbyPiglins(world::LoadedWorld& world, const player::PlayerSession& player, bool onlyIfSeen);

    /// parity: `Piglin.dropCustomDeathLoot`'s inventory.
    void dropPiglinInventory(world::LoadedWorld& world, const entity::ServerEntity& piglin);

    /// parity: `Goat.customServerAiStep` over `GoatAi`.
    void tickGoat(world::LoadedWorld& world, entity::ai::MobContext& context);

    /// parity: `Goat.mobInteract`'s milking. Answers whether it gave milk.
    [[nodiscard]] bool milkGoat(world::LoadedWorld& world, player::PlayerSession& player, entity::ServerEntity& goat,
                                std::int32_t slot);

    /// parity: `Camel.customServerAiStep` over `CamelAi`, the husk's too.
    void tickCamel(world::LoadedWorld& world, entity::ai::MobContext& context);

    /// parity: `Mob.baseTick`'s ambient sound, at the mob's own volume, pitch and category.
    void tickAmbientSound(world::LoadedWorld& world, entity::ServerEntity& mob, random::RandomSource& random);

    /// parity: each mob's `getAmbientSound`, and the `playAmbientSound` overrides that stay quiet.
    [[nodiscard]] std::optional<std::string> ambientSoundOf(world::LoadedWorld& world, entity::ServerEntity& mob,
                                                            random::RandomSource& random);

    /// parity: `Breeze.customServerAiStep` over `BreezeAi`.
    void tickBreeze(world::LoadedWorld& world, entity::ai::MobContext& context);

    /// The fight activity: `Shoot`, `LongJump`, `ShootWhenStuck` and `Slide`, as they start.
    void breezeFights(world::LoadedWorld& world, entity::ai::MobContext& context, entity::breezes::BreezeState& state,
                      const std::array<double, 3>& target, bool inWater);

    /// parity: `Shoot.tick` and `stop`.
    void breezeShoots(world::LoadedWorld& world, entity::ai::MobContext& context, entity::breezes::BreezeState& state);

    /// parity: `LongJump.canRun`, which picks the landing when it can.
    bool breezeCanJump(entity::ai::MobContext& context, entity::breezes::BreezeState& state,
                       const std::optional<std::array<double, 3>>& target);

    /// parity: `LongJump.start`.
    void breezeStartsJump(world::LoadedWorld& world, entity::ai::MobContext& context,
                          entity::breezes::BreezeState& state);

    /// parity: `LongJump.tick` and `stop`.
    void breezeJumps(world::LoadedWorld& world, entity::ai::MobContext& context, entity::breezes::BreezeState& state);

    /// Where the breeze's target looks, which the jump and the slide land behind.
    [[nodiscard]] static float breezeTargetHeadYaw(const entity::ai::MobContext& context, std::int32_t targetId);

    /// parity: `Armadillo.customServerAiStep` over `ArmadilloAi`.
    void tickArmadillo(world::LoadedWorld& world, entity::ai::MobContext& context);

    /// parity: `ArmadilloBallUp.tick`: rolled, scared, peeking and unrolling.
    void armadilloBallsUp(world::LoadedWorld& world, entity::ai::MobContext& context,
                          entity::armadillos::ArmadilloState& state);

    /// One of the armadillo's loot tables dropped at its feet. Answers whether anything came out.
    bool armadilloDrops(world::LoadedWorld& world, const entity::ServerEntity& armadillo, std::string_view tableId);

    /// parity: `Armadillo.brushOffScute`. Answers whether the brush did anything.
    bool brushArmadillo(world::LoadedWorld& world, entity::ServerEntity& armadillo);

    /// parity: `Sniffer.customServerAiStep` over `SnifferAi`.
    void tickSniffer(world::LoadedWorld& world, entity::ai::MobContext& context);

    /// The sniffer's behaviours that run for a while: their end, and what each leaves behind.
    void snifferRunning(world::LoadedWorld& world, entity::ai::MobContext& context,
                        entity::sniffers::SnifferState& state, bool canSniff, bool canDig);

    /// parity: `Sniffer.dropSeed`.
    void snifferDropsSeed(world::LoadedWorld& world, const entity::ServerEntity& sniffer,
                          const std::array<std::int32_t, 3>& head);

    /// parity: `Frog.customServerAiStep` over `FrogAi`.
    void tickFrog(world::LoadedWorld& world, entity::ai::MobContext& context);

    /// parity: `LongJumpToPreferredBlock` and `LongJumpMidJump`, the frog's jumps.
    void frogJumps(world::LoadedWorld& world, entity::ai::MobContext& context, entity::frogs::FrogState& state);

    /// parity: `ShootTongue`, the frog's tongue activity.
    void frogTongue(world::LoadedWorld& world, entity::ai::MobContext& context, entity::frogs::FrogState& state);

    /// parity: `TryFindLandNearLiquid` and `TryLaySpawnOnFluidNearLand`, a pregnant frog's own two.
    void frogLaysSpawn(world::LoadedWorld& world, entity::ai::MobContext& context, entity::frogs::FrogState& state,
                       bool inWater);

    /// parity: `Tadpole.customServerAiStep` over `TadpoleAi`.
    void tickTadpole(world::LoadedWorld& world, entity::ai::MobContext& context);

    /// parity: `Axolotl.customServerAiStep` over `AxolotlAi`.
    void tickAxolotl(world::LoadedWorld& world, entity::ai::MobContext& context);

    /// parity: `Axolotl.applySupportingEffects`.
    void axolotlSupports(player::PlayerSession& player, std::int32_t axolotlId);

    /// parity: `Hoglin.customServerAiStep` over `HoglinAi`.
    void tickHoglin(world::LoadedWorld& world, entity::ai::MobContext& context);

    /// parity: `HoglinAi.setAttackTarget`, the nearer of the two kept, for two hundred ticks.
    void hoglinSetAttackTarget(world::LoadedWorld& world, entity::ServerEntity& hoglin, std::int32_t targetId);

    /// parity: `ZombifiedPiglin.customServerAiStep`: the anger, its growl, its speed and the others called in.
    void tickZombifiedPiglin(world::LoadedWorld& world, entity::ServerEntity& mob, random::RandomSource& random);

    /// parity: `updatePersistentAnger(level, true)` in the `aiStep` of the wolf, the iron golem, the
    /// polar bear and the enderman, and the enderman's `setTarget` speed.
    void tickNeutralMob(world::LoadedWorld& world, entity::ServerEntity& mob, random::RandomSource& random);

    /// parity: `ServerPlayer.tellNeutralMobsThatIDied` for every player who died since the last turn.
    void forgiveDeadPlayers(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers);

    /// The bee's own turn beyond its goals. parity: `Bee.customServerAiStep` and the rest of `Bee.aiStep`.
    void tickBee(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                 entity::ServerEntity& bee, random::RandomSource& random);

    [[nodiscard]] HiveView hiveAt(const entity::ServerEntity& bee, const std::array<std::int32_t, 3>& pos) override;

    void enterHive(entity::ServerEntity& bee, const std::array<std::int32_t, 3>& hive) override;

    [[nodiscard]] std::vector<std::array<std::int32_t, 3>> beeHomesNear(const entity::ServerEntity& bee,
                                                                        std::int32_t radius) override;

    [[nodiscard]] bool beesStayInHive(const entity::ServerEntity& bee) override;

    [[nodiscard]] bool raining(const entity::ServerEntity& bee) override;

    bool growBeeCrop(entity::ServerEntity& bee, const std::array<std::int32_t, 3>& pos) override;

    /// parity: `BeehiveBlockEntity.serverTick`. The honey it adds is left in `pending` for the caller to
    /// set once the block entities are done; answers whether the hive changed.
    bool tickBeehive(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                     std::vector<world::BlockChange>& pending);

    /// parity: `BeehiveBlockEntity.emptyAllLivingFromHive`: every bee out, and the ones near `player`
    /// after them unless the hive is smoked.
    void emptyHive(world::LoadedWorld& world, const std::array<std::int32_t, 3>& hive, std::int32_t stateId,
                   const player::PlayerSession* player, world::beehives::Release release);

    /// parity: `BeehiveBlock.playerDestroy`: the bees out after the player, and the ones around angry,
    /// unless silk touch took the hive with them inside.
    void hiveBroken(world::LoadedWorld& world, const player::PlayerSession& player,
                    const std::array<std::int32_t, 3>& hive, std::int32_t stateId, wire::nbt::NbtCompound data,
                    bool silkTouch);

    /// parity: `BeehiveBlock.getDrops` and `onExplosionHit`: tnt, a creeper, a wither or its skull let
    /// the bees out, and every blast angers the ones around.
    void hiveBlown(world::LoadedWorld& world, const std::array<std::int32_t, 3>& hive, std::int32_t stateId,
                   wire::nbt::NbtCompound data, std::int32_t sourceEntityId);

    /// parity: `BeehiveBlock.angerNearbyBees`.
    void angerBeesAround(world::LoadedWorld& world, const std::array<std::int32_t, 3>& hive);

    /// Whether somebody is stood close enough to a cart to shove it. parity: two boxes touching.
    [[nodiscard]] static bool touchesCart(const entity::ServerEntity& cart, const player::PlayerSession& player);

private:
    /// Whether the heart at `home` still claims this creaking, or nothing once the heart is gone.
    [[nodiscard]] std::optional<bool> heartClaims(world::LoadedWorld& world, const std::array<std::int32_t, 3>& home,
                                                  const entity::ServerEntity& creaking);

    /// parity: `CreakingHeartBlockEntity.getCreakingProtector`.
    entity::ServerEntity* heartProtector(world::LoadedWorld& world, const std::array<std::int32_t, 3>& at,
                                         entity::creakings::HeartState& heart);

    void heartParticles(world::LoadedWorld& world, const std::array<std::int32_t, 3>& at,
                        const entity::ServerEntity& creaking, std::int32_t count, bool towardsCreaking,
                        random::RandomSource& random);

    /// parity: `CreakingHeartBlockEntity.creakingHurt`.
    void creakingHeartHurt(world::LoadedWorld& world, const std::array<std::int32_t, 3>& at);

    /// parity: `spreadResin`.
    std::optional<std::array<std::int32_t, 3>>
    spreadResin(world::LoadedWorld& world, const std::array<std::int32_t, 3>& from, random::RandomSource& random);

    /// parity: `removeProtector`, torn down or, with a breaker, killed by them.
    void removeCreakingProtector(world::LoadedWorld& world, const std::array<std::int32_t, 3>& at,
                                 const player::PlayerSession* breaker);

    /// parity: `spawnProtector`.
    entity::ServerEntity* spawnProtector(world::LoadedWorld& world, const std::array<std::int32_t, 3>& at,
                                         random::RandomSource& random);

    /// The world this mob is in, which is the one thing a goal never hands over.
    [[nodiscard]] world::LoadedWorld* worldOf(const entity::ServerEntity& mob);

    [[nodiscard]] std::vector<player::PlayerSession*> watchersOf(const world::LoadedWorld& world) const;

    /// One entity-data entry to everybody watching, which is how a mob's own flags travel.
    void sendEntityData(const world::LoadedWorld& world, std::int32_t entityId,
                        const wire::protocol::play::SetEntityDataPacket::Entry& entry);

    void playSound(const world::LoadedWorld& world, const entity::ServerEntity& mob, std::string_view sound,
                   float volume = 1.0F, float pitch = 1.0F,
                   wire::protocol::play::SoundSource source = wire::protocol::play::SoundSource::Hostile);

    /// parity: `EatBlockGoal`: the grass in front becomes air and the block below becomes dirt.
    void eatGrassUnder(world::LoadedWorld& world, const entity::ServerEntity& mob);

    /// The light a mob is standing in, which is all the spiders need to know.
    [[nodiscard]] static std::int32_t lightAt(world::LoadedWorld& world, const std::array<std::int32_t, 3>& position);

    [[nodiscard]] random::RandomSource freshRandom() const;

    /// Whether a warden's swing may land now, running what the swing sets off when it does.
    bool wardenSwings(world::LoadedWorld& world, entity::ServerEntity& mob);

    void runWardenActions(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                          entity::ServerEntity& warden, const std::vector<entity::wardenBrain::Action>& actions);

    /// Where a bullet's target stands and how tall it is, or nothing once it is gone.
    [[nodiscard]] std::optional<std::array<double, 4>> bulletTarget(world::LoadedWorld& world, std::int32_t targetId);

    /// parity: `Shulker.teleportSomewhere`.
    bool teleportShulker(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                         const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& shulker,
                         random::RandomSource& random);

    /// parity: `Shulker.hitByShulkerBullet`, the jump and the new shulker left behind.
    void splitShulker(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                      const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& shulker,
                      random::RandomSource& random);

    /// parity: `ShulkerBullet.onHitEntity`: four points and levitation.
    void bulletHitsEntity(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                          entity::ServerEntity& bullet, entity::ServerEntity* mob, player::PlayerSession* player);

    /// parity: `CopperGolem.turnToStatue`.
    void turnToStatue(world::LoadedWorld& world, entity::ServerEntity& golem, random::RandomSource& random);

    /// parity: `SonicBoom.tick`'s blast.
    void sonicBlast(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                    entity::ServerEntity& warden);

    /// What a death roll knows about the one that died and whoever killed it.
    [[nodiscard]] ::basalt::server::world::loot::LootContext deathLootContext(world::LoadedWorld& world,
                                                                              const entity::ServerEntity& dead) const;

    [[nodiscard]] std::int32_t cureProgress(world::LoadedWorld& world, const entity::ServerEntity& mob);

    /// parity: `ConversionType.SINGLE`'s riders, seat, leash and team, carried to the new mob.
    void carryOver(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                   entity::ServerEntity& from, entity::ServerEntity& to);

    /// parity: the `finalizeSpawn(CONVERSION)` a frog, a cured villager or an infected one gets.
    void finalizeConverted(world::LoadedWorld& world, const entity::ServerEntity& from, entity::ServerEntity& to,
                           const entity::mobConversions::Conversion& conversion);

    /// parity: the rest of `ZombieVillager.finishConversion`: the kept gear dropped and the trigger.
    void villagerCured(world::LoadedWorld& world, const entity::ServerEntity& zombie, entity::ServerEntity& villager);

    /// parity: `NeutralMob.updatePersistentAnger`.
    void updateNeutralAnger(world::LoadedWorld& world, entity::ServerEntity& mob, entity::neutralAnger::Anger& anger,
                            random::RandomSource& random, bool stayAngry, std::int32_t minTicks, std::int32_t maxTicks);

    /// The goat brain's ram. parity: `PrepareRamNearestTarget`
    /// and `RamTarget`.
    void goatRam(world::LoadedWorld& world, entity::ai::MobContext& context, entity::goats::GoatState& state,
                 bool screaming);
    void goatRams(world::LoadedWorld& world, entity::ServerEntity& goat, std::int32_t targetId, bool player,
                  const std::array<double, 3>& direction);
    bool goatDropsHorn(world::LoadedWorld& world, entity::ServerEntity& goat, random::RandomSource& random);

    /// Whether an item is in an item tag.
    [[nodiscard]] bool itemTagged(std::string_view tag, const wire::common::Identifier& item) const;

    /// One equipment slot to everybody watching, components and all.
    void equipmentChanged(entity::ServerEntity& mob, std::uint8_t slot);

    /// parity: `Mob.makeSound`: the mob's own voice, its volume and a pitch a baby raises.
    void makeSound(world::LoadedWorld& world, const entity::ServerEntity& mob, std::string_view sound, float volume);

    /// parity: `BehaviorUtils.throwItem(body, stack, target)` at 0.3, from 0.3 under the eyes.
    void throwFromMob(world::LoadedWorld& world, const entity::ServerEntity& mob,
                      const server::world::loot::ItemStack& stack, const std::array<double, 3>& target);

    /// The piglin brain's pieces. parity: `PiglinAi` and `PiglinBruteAi`.
    void piglinSense(world::LoadedWorld& world, entity::ai::MobContext& context, entity::piglins::PiglinState& state);
    void piglinCore(world::LoadedWorld& world, entity::ai::MobContext& context, entity::piglins::PiglinState& state);
    void piglinActivity(world::LoadedWorld& world, entity::ai::MobContext& context,
                        entity::piglins::PiglinState& state);
    void piglinIdle(world::LoadedWorld& world, entity::ai::MobContext& context, entity::piglins::PiglinState& state,
                    bool moving);
    void piglinFight(world::LoadedWorld& world, entity::ai::MobContext& context, entity::piglins::PiglinState& state);
    void piglinPicksUp(world::LoadedWorld& world, entity::ServerEntity& piglin, entity::piglins::PiglinState& state);
    void piglinStopHoldingOffhand(world::LoadedWorld& world, entity::ServerEntity& piglin,
                                  entity::piglins::PiglinState& state, bool barter);
    void piglinWasHurtBy(world::LoadedWorld& world, entity::ai::MobContext& context,
                         entity::piglins::PiglinState& state, std::int32_t attackerId);
    void piglinSetAngerTarget(world::LoadedWorld& world, entity::ServerEntity& piglin,
                              entity::piglins::PiglinState& state, std::int32_t targetId);
    void piglinBroadcastAnger(world::LoadedWorld& world, entity::ServerEntity& piglin,
                              entity::piglins::PiglinState& state, std::int32_t targetId);
    /// parity: `PiglinAi.maybeRetaliate`'s anger, universal or at the attacker, told to the others.
    void piglinRetaliate(world::LoadedWorld& world, entity::ServerEntity& piglin, entity::piglins::PiglinState& state,
                         std::int32_t attackerId, bool byPlayer);
    /// parity: `getNearestVisibleTargetablePlayer`, off the piglin's own player sensor.
    [[nodiscard]] std::optional<std::int32_t> piglinNearestPlayer(world::LoadedWorld& world,
                                                                  const entity::ServerEntity& piglin);
    [[nodiscard]] std::optional<std::int32_t> piglinAttackTarget(world::LoadedWorld& world,
                                                                 const entity::ai::MobContext& context,
                                                                 entity::piglins::PiglinState& state);
    [[nodiscard]] bool piglinWantsToPickUp(const entity::ServerEntity& piglin,
                                           const entity::piglins::PiglinState& state,
                                           const server::world::loot::ItemStack& stack, std::int64_t now) const;
    bool piglinEquip(world::LoadedWorld& world, entity::ServerEntity& piglin,
                     const server::world::loot::ItemStack& stack);

    /// parity: the allay's vibration user and `JukeboxListener`.
    void allayHears(world::LoadedWorld& world, entity::ServerEntity& allay, entity::allays::AllayState& state);

    /// parity: `AllayAi.throwItem`.
    void allayThrows(world::LoadedWorld& world, entity::ServerEntity& allay, entity::allays::AllayState& state,
                     const std::array<double, 3>& target);

    /// parity: `Mob.aiStep`'s pickup over `InventoryCarrier.pickUpItem`.
    void allayPicksUp(world::LoadedWorld& world, entity::ServerEntity& allay, entity::allays::AllayState& state);

    /// parity: `NeutralMob.updatePersistentAnger(level, false)`.
    void updateBeeAnger(world::LoadedWorld& world, entity::ServerEntity& bee, random::RandomSource& random);

    /// parity: the rest of `Bee.doHurtTarget` once the sting landed.
    void beeStung(world::LoadedWorld& world, entity::ServerEntity& bee, player::PlayerSession* player,
                  entity::ServerEntity* mob);

    /// parity: `releaseAllOccupants` over a hive's saved form, and the aim at `player`. Answers
    /// whether anybody came out.
    bool releaseAll(world::LoadedWorld& world, const std::array<std::int32_t, 3>& hive, std::int32_t stateId,
                    wire::nbt::NbtCompound& data, const player::PlayerSession* player,
                    world::beehives::Release release);

    /// parity: `BeehiveBlockEntity.releaseOccupant`. Answers whether the bee came out.
    bool releaseOccupant(world::LoadedWorld& world, const std::array<std::int32_t, 3>& hive, std::int32_t stateId,
                         const wire::nbt::NbtCompound& occupant, world::beehives::Release release,
                         const std::optional<std::array<std::int32_t, 3>>& flower,
                         std::vector<entity::ServerEntity*>* spawned, std::vector<world::BlockChange>* pending);

    /// parity: `Villager.customServerAiStep`, the brain every tick instead of goals every other.
    void tickVillager(world::LoadedWorld& world, entity::ServerEntity& villager,
                      const std::vector<entity::ai::PlayerView>& views,
                      const std::vector<entity::ServerEntity*>& living, random::RandomSource& random);

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
    std::unique_ptr<VillagerWorldSystem> villagers_;
    /// Whether each hive had fire beside it at its last turn, by world and square, so that only
    /// fire that has just arrived empties it.
    std::map<std::pair<std::string, std::array<std::int32_t, 3>>, bool> hivesByFire_;
    /// Each creaking heart's unsaved counters, by world and square.
    std::map<std::pair<std::string, std::array<std::int32_t, 3>>, entity::creakings::HeartState> creakingHearts_;
    /// parity: `Parrot.imitateNearbyMobs` from a box at `x, y, z`. Answers whether it made a sound.
    bool imitateNearbyMobs(world::LoadedWorld& world, double x, double y, double z, double width, double height,
                           wire::protocol::play::SoundSource source, random::RandomSource& random);
    [[nodiscard]] player::PlayerSession* playerIn(world::LoadedWorld& world, std::int32_t entityId);
    void leashSound(world::LoadedWorld& world, std::string_view sound, double x, double y, double z);
    void setLeashedTo(world::LoadedWorld& world, entity::ServerEntity& leashed, std::int32_t holderId);
    [[nodiscard]] std::vector<entity::ServerEntity*>
    leashedTo(world::LoadedWorld& world, const entity::ServerEntity& holder, double x, double y, double z);
    [[nodiscard]] std::vector<entity::ServerEntity*> leashedToId(world::LoadedWorld& world, std::int32_t holderId,
                                                                 double x, double y, double z);
    [[nodiscard]] bool canBeLeashed(const entity::ServerEntity& entity) const;
    [[nodiscard]] bool canHaveLeashAttachedTo(const entity::ServerEntity& leashed, const entity::leashes::Body& holder,
                                              std::int32_t holderId) const;
    entity::ServerEntity* knotAt(world::LoadedWorld& world, const std::array<std::int32_t, 3>& fence, bool create);
    /// parity: `shearOffAllLeashConnections`.
    bool cutLeashes(world::LoadedWorld& world, entity::ServerEntity& entity, const player::PlayerSession* player);
    /// parity: `LivingEntity.checkFallDamage` on landing: the dust, `fallOn` and `causeFallDamage`.
    void landMob(world::LoadedWorld& world, entity::ServerEntity& mob);
    /// parity: `walkingStepSound` on the square at `at`.
    void stepSound(world::LoadedWorld& world, entity::ServerEntity& mob, const std::array<std::int32_t, 3>& at,
                   std::int32_t stateId);
    /// parity: `Entity.playSound`, heard by everyone in the world, never from a silenced mob.
    void movementSound(world::LoadedWorld& world, const entity::ServerEntity& mob, std::string_view sound, float volume,
                       float pitch);
    /// parity: `ServerPlayer.respawnEntityOnShoulder`.
    void respawnShoulder(world::LoadedWorld& world, player::PlayerSession& player, const wire::nbt::NbtCompound& tag);

    /// The players whose death the neutral mobs have already been told of.
    std::set<std::int32_t> mourned_;
};

} // namespace basalt::server::runtime
