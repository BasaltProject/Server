#pragma once

#include <basalt/server/runtime/MobActionHooks.hpp>
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

namespace basalt::server::runtime::scoreboard {
class Teams;
}

namespace basalt::server::runtime {

class BlockChangeBroadcaster;
class CombatSystem;
class ExplosionSystem;
class NaturalSpawningSystem;
class RaidSystem;

class MobActionSystem final : public entity::ai::MobActions {
public:
    using Hooks = MobActionHooks;
    using Options = MobActionOptions;

    /// Which worlds a mob might be in, since a goal only ever hands over the mob itself.
    using Worlds = std::function<std::vector<world::LoadedWorld*>()>;

    MobActionSystem(player::PlayerRegistry& players, BlockChangeBroadcaster& broadcaster, ExplosionSystem& explosions,
                    NaturalSpawningSystem& spawning, CombatSystem& combat, RaidSystem& raids,
                    world::RealLootTables& lootTables, Worlds worlds, Hooks hooks, Options options);

    /// Asks plugins about a target the goals just picked and puts the old one back when they refuse.
    void retarget(world::LoadedWorld& world, entity::ServerEntity& mob, const std::optional<std::int32_t>& previous);

    void swing(entity::ServerEntity& mob) override;

    void hurtPlayer(entity::ServerEntity& mob, std::int32_t playerId, float damage) override;

    void hurtMob(entity::ServerEntity& mob, entity::ServerEntity& target, float damage) override;

    void breed(entity::ServerEntity& mob, entity::ServerEntity& partner) override;

    void eatGrass(entity::ServerEntity& mob) override;

    void primeCreeper(entity::ServerEntity& mob) override;

    void traderUsesItem(entity::ServerEntity& trader, bool potion, entity::ai::UsePhase phase) override;

    void layTurtleEgg(entity::ServerEntity& turtle, std::int32_t x, std::int32_t y, std::int32_t z,
                      std::int32_t eggs) override;

    void explodeCreeper(entity::ServerEntity& mob) override;

    void setCharging(entity::ServerEntity& mob, bool charging) override;

    void shootFireball(entity::ServerEntity& mob, double targetX, double targetY, double targetZ, bool large) override;

    void setBeamTarget(entity::ServerEntity& mob, std::optional<std::int32_t> targetId) override;

    void setCasting(entity::ServerEntity& mob, bool casting) override;

    void poseChanged(entity::ServerEntity& mob) override;

    bool pluginGoal(entity::ServerEntity& mob, std::string_view key, entity::ai::PluginGoalPhase phase) override;

    void throwPotion(entity::ServerEntity& mob, double targetX, double targetY, double targetZ,
                     std::string_view potion) override;

    void castFangs(entity::ServerEntity& mob, double targetX, double targetY, double targetZ) override;

    void summonVexes(entity::ServerEntity& mob, std::int32_t count) override;

    void applyEffect(std::int32_t playerId, std::string_view effect, std::int32_t durationTicks, std::int32_t amplifier,
                     std::optional<std::int32_t> sourceId) override;

    void setSleeping(entity::ServerEntity& mob, bool sleeping) override;

    void teleported(entity::ServerEntity& mob) override;

    bool mayTeleport(entity::ServerEntity& mob, double x, double y, double z, std::string_view cause) override;

    [[nodiscard]] std::optional<bool> blockTagged(std::string_view tag,
                                                  const wire::common::Identifier& block) const override;

    void shootArrow(entity::ServerEntity& mob, double targetX, double targetY, double targetZ, float power) override;

    void llamaSpit(entity::ServerEntity& llama, double targetX, double targetY, double targetZ,
                   double targetHeight) override;

    void wardenRoar(entity::ServerEntity& warden, std::int32_t targetId) override;

    void wardenAngered(entity::ServerEntity& warden, std::int32_t suspectId, std::int32_t amount, bool sound) override;

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

    void makeHorseMad(entity::ServerEntity& horse);

    /// parity: the rest of `AbstractHorse.aiStep` and `tick` after the goals.
    void tickHorse(world::LoadedWorld& world, entity::ServerEntity& horse, random::RandomSource& random);

    void tickCreaking(world::LoadedWorld& world, entity::ai::MobContext& context);

    void tearDownCreaking(world::LoadedWorld& world, entity::ServerEntity& creaking);

    bool tickCreakingHeart(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                           std::vector<world::BlockChange>& pending);

    /// "spawn" puts a protector at the heart and answers its uuid; "resin" spreads a clump and answers "x,y,z".
    [[nodiscard]] std::string heartCommand(world::LoadedWorld& world, const std::array<std::int32_t, 3>& at,
                                           std::string_view action);

    bool tickBell(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                  wire::nbt::NbtCompound& data);

    void creakingHeartBroken(world::LoadedWorld& world, const player::PlayerSession& player,
                             const std::array<std::int32_t, 3>& at);

    void dolphinPicksUp(world::LoadedWorld& world, entity::ServerEntity& dolphin);

    /// An override's `can-pick-up-loot` mob takes a dropped item whose equipment slot is empty.
    void mobPicksUpLoot(world::LoadedWorld& world, entity::ServerEntity& mob);

    void shulkerPeek(entity::ServerEntity& shulker, std::int32_t amount) override;

    void shootShulkerBullet(entity::ServerEntity& shulker, std::int32_t targetId) override;

    void recruitRaiders(entity::ServerEntity& raider) override;

    void takeRaidBanner(entity::ServerEntity& raider, std::int32_t itemId) override;

    void setCelebrating(entity::ServerEntity& raider, bool celebrating) override;

    bool sitOnShoulder(entity::ServerEntity& parrot, std::int32_t playerId) override;

    void emitMovement(world::LoadedWorld& world, entity::ServerEntity& mob);

    /// parity: the same for every player, whose footsteps and strokes everybody else hears.
    void emitPlayerMovement(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers);

    void tickLeash(world::LoadedWorld& world, entity::ServerEntity& leashed);

    enum class LeashUse : std::uint8_t { Pass, Done, ConsumeOne, DamageShears };

    /// parity: `Entity.interact`'s leash half, before the mob's own `mobInteract`.
    LeashUse useOnLeashable(world::LoadedWorld& world, player::PlayerSession& player, entity::ServerEntity& entity,
                            bool holdingLead, bool holdingShears);

    LeashUse useOnKnot(world::LoadedWorld& world, player::PlayerSession& player, entity::ServerEntity& knot,
                       bool holdingShears);

    bool bindPlayerMobs(world::LoadedWorld& world, player::PlayerSession& player,
                        const std::array<std::int32_t, 3>& fence);

    bool dropAllLeashes(world::LoadedWorld& world, player::PlayerSession& player);

    /// parity: `Leashable.dropLeash` and `removeLeash`, the lead dropped as an item or not.
    void dropLeash(world::LoadedWorld& world, entity::ServerEntity& leashed, bool dropLead,
                   std::string_view reason = "unknown");

    void propagateFall(world::LoadedWorld& world, entity::ServerEntity& vehicle, double fallen,
                       const world::fallLanding::Landing& landing);

    void camelRidden(world::LoadedWorld& world, entity::ServerEntity& camel, const player::PlayerSession& rider);

    void tickParrot(world::LoadedWorld& world, entity::ServerEntity& parrot);

    /// parity: `Parrot.setRecordPlayingNearby`, which makes a parrot dance to a jukebox.
    void parrotListens(world::LoadedWorld& world, entity::ServerEntity& parrot);
    void flapParrot(world::LoadedWorld& world, entity::ServerEntity& parrot);

    /// parity: `Parrot.flapSpeed`, `Entity.flyDist` and `Parrot.nextFlap`, with where the parrot was a tick ago.
    struct ParrotFlap {
        bool placed = false;
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
        float flapSpeed = 0.0F;
        float flyDist = 0.0F;
        float nextFlap = 1.0F;
    };

    std::map<std::int32_t, ParrotFlap> parrotFlaps_;

    void tickPufferfish(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                        entity::ServerEntity& fish, const std::vector<entity::ServerEntity*>& living);

    void tickShoulders(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers);

    void tickShulker(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                     const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& shulker,
                     random::RandomSource& random);

    void tickShulkerBullet(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                           const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& bullet,
                           random::RandomSource& random);

    void tickCopperGolem(world::LoadedWorld& world, entity::ServerEntity& golem, random::RandomSource& random);

    enum class GolemUse : std::uint8_t { Pass, Done, ConsumeOne, DamageTool };

    [[nodiscard]] GolemUse useOnCopperGolem(world::LoadedWorld& world, player::PlayerSession& player,
                                            entity::ServerEntity& golem,
                                            const std::optional<wire::common::Identifier>& held);

    bool awakenStatue(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

    void golemStruckByLightning(entity::ServerEntity& golem, std::int32_t boltId);

    void wardenEmerges(world::LoadedWorld& world, entity::ServerEntity& warden);

    static void wardenStruck(entity::ServerEntity& warden, std::int32_t attackerId, bool direct,
                             double distanceSquared);

    void tickWarden(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                    entity::ServerEntity& warden);

    void trailSnow(world::LoadedWorld& world, const entity::ServerEntity& golem);

    void tickFire(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                  entity::ServerEntity& mob, random::RandomSource& random);

    bool tickConversion(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                        entity::ServerEntity& mob);

    bool convertMob(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                    entity::ServerEntity& mob, const entity::mobConversions::Conversion& conversion,
                    const std::function<void(wire::nbt::NbtCompound& converted)>& finish);

    bool startCure(world::LoadedWorld& world, entity::ServerEntity& mob, const wire::common::Uuid& player);

    bool infectVillager(world::LoadedWorld& world, const entity::ServerEntity& zombie, entity::ServerEntity& villager);

    void thunderHit(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                    entity::ServerEntity& mob, std::int32_t boltId);

    bool addMobEffect(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                      entity::ServerEntity& mob, const world::ActiveEffect& effect, std::int32_t sourceId = -1);

    void tickMobEffects(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                        entity::ServerEntity& mob);

    void tickBreath(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                    entity::ServerEntity& dolphin, random::RandomSource& random);

    void tickRavager(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                     entity::ServerEntity& ravager);

    void ravagerSwings(world::LoadedWorld& world, entity::ServerEntity& ravager);

    void travelMob(world::LoadedWorld& world, entity::ServerEntity& mob, double speed,
                   random::RandomSource* random = nullptr,
                   const std::optional<std::array<double, 3>>& target = std::nullopt);

    /// Drives a mob from its rider's keys and look, as `LivingEntity.travel` does with a controlling passenger.
    void tickRidden(world::LoadedWorld& world, entity::ServerEntity& mob, const player::PlayerSession& rider);

    void tickVexLife(const std::vector<player::PlayerSession*>& watchers, entity::ServerEntity& vex);

    static void blazeRises(entity::ServerEntity& blaze, std::optional<double> targetEye, random::RandomSource& random);

    void tickMobAi(world::LoadedWorld& world, entity::ServerEntity& mob,
                   const std::vector<entity::ai::PlayerView>& views, const std::vector<entity::ServerEntity*>& living,
                   random::RandomSource& random);

    void activateSkeletonTrap(world::LoadedWorld& world, entity::ServerEntity& horse, random::RandomSource& random);

    [[nodiscard]] entity::villager::VillagerWorld& villagersIn(world::LoadedWorld& world);

    void villagerDied(world::LoadedWorld& world, entity::ServerEntity& villager,
                      const std::optional<std::string>& killerUuid);

    /// parity: `Villager.releaseAllPois`, for a villager turned into something else.
    void releaseVillagerPois(world::LoadedWorld& world, entity::ServerEntity& villager);

    void dropDeathLoot(world::LoadedWorld& world, const entity::ServerEntity& entity);

    void dropDeathInventories(world::LoadedWorld& world, const entity::ServerEntity& entity);

    /// What a mob's loot table gives for this death, before anything is spawned.
    [[nodiscard]] std::vector<server::world::loot::ItemStack> deathLootOf(world::LoadedWorld& world,
                                                                          const entity::ServerEntity& entity);

    void spawnLoot(world::LoadedWorld& world, const entity::ServerEntity& entity,
                   const std::vector<server::world::loot::ItemStack>& stacks);

    /// Lets plugins change the drops and the experience of a death before they happen.
    void askDeathDrops(world::LoadedWorld& world, const entity::ServerEntity& entity,
                       std::vector<server::world::loot::ItemStack>& drops, std::int32_t& experience);

    void breed(world::LoadedWorld& world, entity::ServerEntity& animal, entity::ServerEntity& partner);

    void tickAllay(world::LoadedWorld& world, entity::ai::MobContext& context);

    enum class AllayUse : std::uint8_t { Pass, Done, ConsumeOne };

    [[nodiscard]] AllayUse useOnAllay(world::LoadedWorld& world, player::PlayerSession& player,
                                      entity::ServerEntity& allay,
                                      const std::optional<server::world::loot::ItemStack>& held, bool mainHand);

    void dropAllayInventory(world::LoadedWorld& world, const entity::ServerEntity& allay);

    void tickPiglin(world::LoadedWorld& world, entity::ai::MobContext& context);

    [[nodiscard]] bool useOnPiglin(world::LoadedWorld& world, player::PlayerSession& player,
                                   entity::ServerEntity& piglin,
                                   const std::optional<server::world::loot::ItemStack>& held);

    void angerNearbyPiglins(world::LoadedWorld& world, const player::PlayerSession& player, bool onlyIfSeen);

    void dropPiglinInventory(world::LoadedWorld& world, const entity::ServerEntity& piglin);

    void tickGoat(world::LoadedWorld& world, entity::ai::MobContext& context);

    [[nodiscard]] bool milkGoat(world::LoadedWorld& world, player::PlayerSession& player, entity::ServerEntity& goat,
                                std::int32_t slot);

    void tickCamel(world::LoadedWorld& world, entity::ai::MobContext& context);

    void tickAmbientSound(world::LoadedWorld& world, entity::ServerEntity& mob, random::RandomSource& random);

    [[nodiscard]] std::optional<std::string> ambientSoundOf(world::LoadedWorld& world, entity::ServerEntity& mob,
                                                            random::RandomSource& random);

    void tickBreeze(world::LoadedWorld& world, entity::ai::MobContext& context);

    void breezeFights(world::LoadedWorld& world, entity::ai::MobContext& context, entity::breezes::BreezeState& state,
                      const std::array<double, 3>& target, bool inWater);

    void breezeShoots(world::LoadedWorld& world, entity::ai::MobContext& context, entity::breezes::BreezeState& state);

    bool breezeCanJump(entity::ai::MobContext& context, entity::breezes::BreezeState& state,
                       const std::optional<std::array<double, 3>>& target);

    void breezeStartsJump(world::LoadedWorld& world, entity::ai::MobContext& context,
                          entity::breezes::BreezeState& state);

    void breezeJumps(world::LoadedWorld& world, entity::ai::MobContext& context, entity::breezes::BreezeState& state);

    [[nodiscard]] static float breezeTargetHeadYaw(const entity::ai::MobContext& context, std::int32_t targetId);

    void tickArmadillo(world::LoadedWorld& world, entity::ai::MobContext& context);

    void armadilloBallsUp(world::LoadedWorld& world, entity::ai::MobContext& context,
                          entity::armadillos::ArmadilloState& state);

    bool armadilloDrops(world::LoadedWorld& world, const entity::ServerEntity& armadillo, std::string_view tableId);

    bool brushArmadillo(world::LoadedWorld& world, entity::ServerEntity& armadillo);

    void tickSniffer(world::LoadedWorld& world, entity::ai::MobContext& context);

    void snifferRunning(world::LoadedWorld& world, entity::ai::MobContext& context,
                        entity::sniffers::SnifferState& state, bool canSniff, bool canDig);

    void snifferDropsSeed(world::LoadedWorld& world, const entity::ServerEntity& sniffer,
                          const std::array<std::int32_t, 3>& head);

    void tickFrog(world::LoadedWorld& world, entity::ai::MobContext& context);

    void frogJumps(world::LoadedWorld& world, entity::ai::MobContext& context, entity::frogs::FrogState& state);

    void frogTongue(world::LoadedWorld& world, entity::ai::MobContext& context, entity::frogs::FrogState& state);

    void frogLaysSpawn(world::LoadedWorld& world, entity::ai::MobContext& context, entity::frogs::FrogState& state,
                       bool inWater);

    void tickTadpole(world::LoadedWorld& world, entity::ai::MobContext& context);

    void tickAxolotl(world::LoadedWorld& world, entity::ai::MobContext& context);

    void axolotlSupports(player::PlayerSession& player, std::int32_t axolotlId);

    void tickHoglin(world::LoadedWorld& world, entity::ai::MobContext& context);

    void hoglinSetAttackTarget(world::LoadedWorld& world, entity::ServerEntity& hoglin, std::int32_t targetId);

    void tickZombifiedPiglin(world::LoadedWorld& world, entity::ServerEntity& mob, random::RandomSource& random);

    void tickNeutralMob(world::LoadedWorld& world, entity::ServerEntity& mob, random::RandomSource& random);

    void forgiveDeadPlayers(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers);

    void tickBee(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                 entity::ServerEntity& bee, random::RandomSource& random);

    [[nodiscard]] HiveView hiveAt(const entity::ServerEntity& bee, const std::array<std::int32_t, 3>& pos) override;

    void enterHive(entity::ServerEntity& bee, const std::array<std::int32_t, 3>& hive) override;

    [[nodiscard]] std::vector<std::array<std::int32_t, 3>> beeHomesNear(const entity::ServerEntity& bee,
                                                                        std::int32_t radius) override;

    [[nodiscard]] bool beesStayInHive(const entity::ServerEntity& bee) override;

    [[nodiscard]] bool raining(const entity::ServerEntity& bee) override;

    bool growBeeCrop(entity::ServerEntity& bee, const std::array<std::int32_t, 3>& pos) override;

    bool tickBeehive(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                     std::vector<world::BlockChange>& pending);

    void emptyHive(world::LoadedWorld& world, const std::array<std::int32_t, 3>& hive, std::int32_t stateId,
                   const player::PlayerSession* player, world::beehives::Release release);

    /// parity: `BeehiveBlock.playerDestroy`.
    void hiveBroken(world::LoadedWorld& world, const player::PlayerSession& player,
                    const std::array<std::int32_t, 3>& hive, std::int32_t stateId, wire::nbt::NbtCompound data,
                    bool silkTouch);

    void hiveBlown(world::LoadedWorld& world, const std::array<std::int32_t, 3>& hive, std::int32_t stateId,
                   wire::nbt::NbtCompound data, std::int32_t sourceEntityId);

    void angerBeesAround(world::LoadedWorld& world, const std::array<std::int32_t, 3>& hive);

    [[nodiscard]] static bool touchesCart(const entity::ServerEntity& cart, const player::PlayerSession& player);

    /// What the plugin API asks of the mob systems; each says whether the mob could do it.
    [[nodiscard]] std::optional<std::string> pluginAmbientSound(entity::ServerEntity& mob);

    [[nodiscard]] std::int32_t pluginExperienceReward(entity::ServerEntity& mob);

    bool pluginRangedAttack(entity::ServerEntity& mob, double x, double y, double z, float charge);

    /// The entity id of the copy, or -1.
    [[nodiscard]] std::int32_t pluginDuplicateAllay(entity::ServerEntity& allay);

    bool pluginRoll(entity::ServerEntity& armadillo, bool up);

    /// parity: `AbstractPiglin.isConverting` and `Hoglin.isConverting`.
    [[nodiscard]] bool pluginConverting(entity::ServerEntity& mob);

    bool pluginSpell(entity::ServerEntity& caster, std::int32_t spell);

    bool pluginCatLying(entity::ServerEntity& cat, bool lying);

    bool pluginCatRelaxed(entity::ServerEntity& cat, bool relaxed);

    bool pluginCelebrate(entity::ServerEntity& raider, bool celebrating);

    bool pluginGuardianLaser(entity::ServerEntity& guardian, bool active);

    bool pluginEnderTeleport(entity::ServerEntity& enderman, bool towards, double x, double y, double z);

    bool pluginRestock(entity::ServerEntity& villager);

    bool pluginUpdateDemand(entity::ServerEntity& villager);

    [[nodiscard]] std::int32_t pluginAddTrades(entity::ServerEntity& villager, std::int32_t amount);

    bool pluginResetOffers(entity::ServerEntity& villager);

    bool pluginShakeHead(entity::ServerEntity& villager);

    bool pluginSleep(entity::ServerEntity& villager, std::int32_t x, std::int32_t y, std::int32_t z);

    bool pluginWake(entity::ServerEntity& villager);

    /// The entity id of the zombie villager it turned into, or -1 when it did not.
    [[nodiscard]] std::int32_t pluginZombify(entity::ServerEntity& villager);

    /// parity: `Shearable.readyForShearing`, for the mobs that have one.
    [[nodiscard]] bool readyForShearing(const entity::ServerEntity& mob) const;

    /// parity: `Shearable.shear`: its sound, its drops and what it becomes. False when the mob cannot be sheared.
    bool shearMob(entity::ServerEntity& mob, wire::protocol::play::SoundSource source);

    /// The entity id of the copy made as `into`, or -1.
    [[nodiscard]] std::int32_t pluginRetype(entity::ServerEntity& mob, const wire::common::Identifier& into);

    bool pluginGoatRam(entity::ServerEntity& goat, std::int32_t targetId);

    bool pluginSnifferState(entity::ServerEntity& sniffer, std::int32_t pose);

    [[nodiscard]] bool pluginSnifferCanDig(entity::ServerEntity& sniffer);

    [[nodiscard]] std::optional<std::array<std::int32_t, 3>> pluginSnifferDigSpot(entity::ServerEntity& sniffer);

    bool pluginSulfurSwallow(entity::ServerEntity& cube, const server::world::loot::ItemStack& item);

    /// parity: `SulfurCube.canExplode`, an explosive archetype inside it and the fuse not yet lit.
    [[nodiscard]] bool pluginSulfurCanExplode(entity::ServerEntity& cube);

    /// parity: `SulfurCube.primeTime`.
    bool pluginSulfurPrime(entity::ServerEntity& cube, bool imminent);

    /// parity: `ItemSteerable.boost`: false when the vehicle does not take this item or is already boosting.
    bool boostSteerable(entity::ServerEntity& vehicle, std::string_view item);

    /// parity: `SulfurCube.tick`, the fuse and the blast it ends in.
    void tickSulfurCube(world::LoadedWorld& world, entity::ServerEntity& cube);

    enum class SulfurUse : std::uint8_t { Pass, Consume, Wear };

    /// parity: `SulfurCube.mobInteract`, lighting the cube or swallowing what the player holds.
    [[nodiscard]] SulfurUse useOnSulfurCube(entity::ServerEntity& cube, const server::world::loot::ItemStack& held);

    bool pluginWitchDrink(entity::ServerEntity& witch, const std::optional<server::world::loot::ItemStack>& potion);

    /// parity: `Witch.aiStep`, the potions it drinks by itself.
    void tickWitchDrinking(world::LoadedWorld& world, entity::ServerEntity& witch, random::RandomSource& random);

    /// parity: `Strider.tick`, which decides whether it shivers.
    void tickStriderWarmth(world::LoadedWorld& world, entity::ServerEntity& strider);

    [[nodiscard]] bool wololo(entity::ServerEntity& evoker, std::int32_t sheepId) override;

    [[nodiscard]] bool mobGriefing(const entity::ServerEntity& mob) override;

private:
    void refreshSulfurBody(entity::ServerEntity& cube);

    [[nodiscard]] std::optional<bool> heartClaims(world::LoadedWorld& world, const std::array<std::int32_t, 3>& home,
                                                  const entity::ServerEntity& creaking);

    entity::ServerEntity* heartProtector(world::LoadedWorld& world, const std::array<std::int32_t, 3>& at,
                                         entity::creakings::HeartState& heart);

    void heartParticles(world::LoadedWorld& world, const std::array<std::int32_t, 3>& at,
                        const entity::ServerEntity& creaking, std::int32_t count, bool towardsCreaking,
                        random::RandomSource& random);

    void creakingHeartHurt(world::LoadedWorld& world, const std::array<std::int32_t, 3>& at);

    std::optional<std::array<std::int32_t, 3>>
    spreadResin(world::LoadedWorld& world, const std::array<std::int32_t, 3>& from, random::RandomSource& random);

    void removeCreakingProtector(world::LoadedWorld& world, const std::array<std::int32_t, 3>& at,
                                 const player::PlayerSession* breaker);

    entity::ServerEntity* spawnProtector(world::LoadedWorld& world, const std::array<std::int32_t, 3>& at,
                                         random::RandomSource& random);

    /// The world this mob is in, which is the one thing a goal never hands over.
    [[nodiscard]] world::LoadedWorld* worldOf(const entity::ServerEntity& mob);

    [[nodiscard]] std::vector<player::PlayerSession*> watchersOf(const world::LoadedWorld& world) const;

    void sendEntityData(const world::LoadedWorld& world, std::int32_t entityId,
                        const wire::protocol::play::SetEntityDataPacket::Entry& entry);

    void playSound(const world::LoadedWorld& world, const entity::ServerEntity& mob, std::string_view sound,
                   float volume = 1.0F, float pitch = 1.0F,
                   wire::protocol::play::SoundSource source = wire::protocol::play::SoundSource::Hostile);

    void eatGrassUnder(world::LoadedWorld& world, const entity::ServerEntity& mob);

    [[nodiscard]] static std::int32_t lightAt(world::LoadedWorld& world, const std::array<std::int32_t, 3>& position);

    [[nodiscard]] random::RandomSource freshRandom() const;

    bool wardenSwings(world::LoadedWorld& world, entity::ServerEntity& mob);

    void runWardenActions(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                          entity::ServerEntity& warden, const std::vector<entity::wardenBrain::Action>& actions);

    [[nodiscard]] std::optional<std::array<double, 4>> bulletTarget(world::LoadedWorld& world, std::int32_t targetId);

    bool teleportShulker(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                         const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& shulker,
                         random::RandomSource& random);

    void splitShulker(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                      const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& shulker,
                      random::RandomSource& random);

    void bulletHitsEntity(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                          entity::ServerEntity& bullet, entity::ServerEntity* mob, player::PlayerSession* player);

    void turnToStatue(world::LoadedWorld& world, entity::ServerEntity& golem, random::RandomSource& random);

    void sonicBlast(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                    entity::ServerEntity& warden);

    [[nodiscard]] ::basalt::server::world::loot::LootContext deathLootContext(world::LoadedWorld& world,
                                                                              const entity::ServerEntity& dead) const;

    [[nodiscard]] std::int32_t cureProgress(world::LoadedWorld& world, const entity::ServerEntity& mob);

    void carryOver(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                   entity::ServerEntity& from, entity::ServerEntity& to);

    void finalizeConverted(world::LoadedWorld& world, const entity::ServerEntity& from, entity::ServerEntity& to,
                           const entity::mobConversions::Conversion& conversion);

    void villagerCured(world::LoadedWorld& world, const entity::ServerEntity& zombie, entity::ServerEntity& villager);

    void updateNeutralAnger(world::LoadedWorld& world, entity::ServerEntity& mob, entity::neutralAnger::Anger& anger,
                            random::RandomSource& random, bool stayAngry, std::int32_t minTicks, std::int32_t maxTicks);

    void goatRam(world::LoadedWorld& world, entity::ai::MobContext& context, entity::goats::GoatState& state,
                 bool screaming);
    void goatRams(world::LoadedWorld& world, entity::ServerEntity& goat, std::int32_t targetId, bool player,
                  const std::array<double, 3>& direction);
    bool goatDropsHorn(world::LoadedWorld& world, entity::ServerEntity& goat, random::RandomSource& random);

    [[nodiscard]] bool itemTagged(std::string_view tag, const wire::common::Identifier& item) const;

    void equipmentChanged(entity::ServerEntity& mob, std::uint8_t slot);

    void makeSound(world::LoadedWorld& world, const entity::ServerEntity& mob, std::string_view sound, float volume);

    void throwFromMob(world::LoadedWorld& world, const entity::ServerEntity& mob,
                      const server::world::loot::ItemStack& stack, const std::array<double, 3>& target);

    void piglinSense(world::LoadedWorld& world, entity::ai::MobContext& context, entity::piglins::PiglinState& state);
    void piglinCore(world::LoadedWorld& world, entity::ai::MobContext& context, entity::piglins::PiglinState& state);
    void piglinActivity(world::LoadedWorld& world, entity::ai::MobContext& context,
                        entity::piglins::PiglinState& state);
    void piglinIdle(world::LoadedWorld& world, entity::ai::MobContext& context, entity::piglins::PiglinState& state,
                    bool moving);
    void piglinFight(world::LoadedWorld& world, entity::ai::MobContext& context, entity::piglins::PiglinState& state);
    void piglinSpearFight(world::LoadedWorld& world, entity::ai::MobContext& context,
                          entity::piglins::PiglinState& state, const entity::ai::TargetView& target,
                          double distanceSquared);
    void piglinPicksUp(world::LoadedWorld& world, entity::ServerEntity& piglin, entity::piglins::PiglinState& state);
    void piglinStopHoldingOffhand(world::LoadedWorld& world, entity::ServerEntity& piglin,
                                  entity::piglins::PiglinState& state, bool barter);
    void piglinWasHurtBy(world::LoadedWorld& world, entity::ai::MobContext& context,
                         entity::piglins::PiglinState& state, std::int32_t attackerId);
    void piglinSetAngerTarget(world::LoadedWorld& world, entity::ServerEntity& piglin,
                              entity::piglins::PiglinState& state, std::int32_t targetId);
    void piglinBroadcastAnger(world::LoadedWorld& world, entity::ServerEntity& piglin,
                              entity::piglins::PiglinState& state, std::int32_t targetId);
    void piglinRetaliate(world::LoadedWorld& world, entity::ServerEntity& piglin, entity::piglins::PiglinState& state,
                         std::int32_t attackerId, bool byPlayer);
    [[nodiscard]] std::optional<std::int32_t> piglinNearestPlayer(world::LoadedWorld& world,
                                                                  const entity::ServerEntity& piglin);
    [[nodiscard]] std::optional<std::int32_t> piglinAttackTarget(world::LoadedWorld& world,
                                                                 const entity::ai::MobContext& context,
                                                                 entity::piglins::PiglinState& state);
    [[nodiscard]] bool piglinCurrency(const entity::piglins::PiglinState& state,
                                      const wire::common::Identifier& item) const;

    [[nodiscard]] bool piglinLoves(const entity::piglins::PiglinState& state,
                                   const wire::common::Identifier& item) const;

    [[nodiscard]] bool piglinWantsToPickUp(const entity::ServerEntity& piglin,
                                           const entity::piglins::PiglinState& state,
                                           const server::world::loot::ItemStack& stack, std::int64_t now) const;
    bool piglinEquip(world::LoadedWorld& world, entity::ServerEntity& piglin,
                     const server::world::loot::ItemStack& stack);

    void allayHears(world::LoadedWorld& world, entity::ServerEntity& allay, entity::allays::AllayState& state,
                    const std::vector<player::PlayerSession*>& watchers);

    void allayThrows(world::LoadedWorld& world, entity::ServerEntity& allay, entity::allays::AllayState& state,
                     const std::array<double, 3>& target);

    void allayPicksUp(world::LoadedWorld& world, entity::ServerEntity& allay, entity::allays::AllayState& state);

    void updateBeeAnger(world::LoadedWorld& world, entity::ServerEntity& bee, random::RandomSource& random);

    void beeStung(world::LoadedWorld& world, entity::ServerEntity& bee, player::PlayerSession* player,
                  entity::ServerEntity* mob);

    bool releaseAll(world::LoadedWorld& world, const std::array<std::int32_t, 3>& hive, std::int32_t stateId,
                    wire::nbt::NbtCompound& data, const player::PlayerSession* player,
                    world::beehives::Release release);

    bool releaseOccupant(world::LoadedWorld& world, const std::array<std::int32_t, 3>& hive, std::int32_t stateId,
                         const wire::nbt::NbtCompound& occupant, world::beehives::Release release,
                         const std::optional<std::array<std::int32_t, 3>>& flower,
                         std::vector<entity::ServerEntity*>* spawned, std::vector<world::BlockChange>* pending);

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
    std::map<std::pair<std::string, std::array<std::int32_t, 3>>, bool> hivesByFire_;
    std::map<std::pair<std::string, std::array<std::int32_t, 3>>, entity::creakings::HeartState> creakingHearts_;
    bool imitateNearbyMobs(world::LoadedWorld& world, double x, double y, double z, double width, double height,
                           wire::protocol::play::SoundSource source, random::RandomSource& random);
    [[nodiscard]] player::PlayerSession* playerIn(world::LoadedWorld& world, std::int32_t entityId);
    void leashSound(world::LoadedWorld& world, std::string_view sound, double x, double y, double z);
    void setLeashedTo(world::LoadedWorld& world, entity::ServerEntity& leashed, std::int32_t holderId);

    [[nodiscard]] bool playerMayUnleash(world::LoadedWorld& world, const player::PlayerSession& player,
                                        const entity::ServerEntity& leashed);
    [[nodiscard]] std::vector<entity::ServerEntity*>
    leashedTo(world::LoadedWorld& world, const entity::ServerEntity& holder, double x, double y, double z);
    [[nodiscard]] std::vector<entity::ServerEntity*> leashedToId(world::LoadedWorld& world, std::int32_t holderId,
                                                                 double x, double y, double z);
    [[nodiscard]] bool canBeLeashed(const entity::ServerEntity& entity) const;
    [[nodiscard]] bool canHaveLeashAttachedTo(const entity::ServerEntity& leashed, const entity::leashes::Body& holder,
                                              std::int32_t holderId) const;
    entity::ServerEntity* knotAt(world::LoadedWorld& world, const std::array<std::int32_t, 3>& fence, bool create);
    bool cutLeashes(world::LoadedWorld& world, entity::ServerEntity& entity, const player::PlayerSession* player);
    void landMob(world::LoadedWorld& world, entity::ServerEntity& mob);
    void stepSound(world::LoadedWorld& world, entity::ServerEntity& mob, const std::array<std::int32_t, 3>& at,
                   std::int32_t stateId);
    /// parity: `Entity.playSound`, heard by everyone in the world, never from a silenced mob.
    void movementSound(world::LoadedWorld& world, const entity::ServerEntity& mob, std::string_view sound, float volume,
                       float pitch);
    void respawnShoulder(world::LoadedWorld& world, player::PlayerSession& player, const wire::nbt::NbtCompound& tag);

    std::set<std::int32_t> mourned_;
};

} // namespace basalt::server::runtime
