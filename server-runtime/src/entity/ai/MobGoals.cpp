#include <basalt/server/runtime/entity/Breeding.hpp>
#include <basalt/server/runtime/entity/CopperGolem.hpp>
#include <basalt/server/runtime/entity/ai/BeeGoals.hpp>
#include <basalt/server/runtime/entity/ai/CatGoals.hpp>
#include <basalt/server/runtime/entity/ai/CopperGolemGoals.hpp>
#include <basalt/server/runtime/entity/ai/DolphinGoals.hpp>
#include <basalt/server/runtime/entity/ai/DolphinItemGoals.hpp>
#include <basalt/server/runtime/entity/ai/EndermanGoals.hpp>
#include <basalt/server/runtime/entity/ai/FlyingGoals.hpp>
#include <basalt/server/runtime/entity/ai/GoalSelector.hpp>
#include <basalt/server/runtime/entity/ai/Goals.hpp>
#include <basalt/server/runtime/entity/ai/HorseGoals.hpp>
#include <basalt/server/runtime/entity/ai/IllagerGoals.hpp>
#include <basalt/server/runtime/entity/ai/LlamaGoals.hpp>
#include <basalt/server/runtime/entity/ai/LongDistancePatrolGoal.hpp>
#include <basalt/server/runtime/entity/ai/MobGoals.hpp>
#include <basalt/server/runtime/entity/ai/NeutralGoals.hpp>
#include <basalt/server/runtime/entity/ai/ParrotGoals.hpp>
#include <basalt/server/runtime/entity/ai/PhantomGoals.hpp>
#include <basalt/server/runtime/entity/ai/RaiderGoals.hpp>
#include <basalt/server/runtime/entity/ai/ShulkerGoals.hpp>
#include <basalt/server/runtime/entity/ai/SpecialGoals.hpp>
#include <basalt/server/runtime/entity/ai/TameGoals.hpp>
#include <basalt/server/runtime/entity/ai/TargetGoals.hpp>
#include <basalt/server/runtime/entity/ai/WanderingTraderGoals.hpp>
#include <basalt/server/runtime/entity/ai/WardenGoals.hpp>
#include <basalt/server/runtime/entity/ai/WitherGoals.hpp>

#include <cstdint>
#include <set>
#include <string>
#include <string_view>

namespace basalt::server::runtime::entity::ai {
namespace {

using Names = std::set<std::string, std::less<>>;

/// parity: the three blocks an illager stands off at to look you in the eye.
constexpr double kLookMeet = 3.0;

/// parity: the six blocks an animal looks at a player from, and the eight a monster does.
constexpr double kLookNear = 6.0;
constexpr double kLookFar = 8.0;

/// parity: `AbstractFish`, which pushes itself along more gently than a squid.
constexpr double kFishPush = 0.1;

/// parity: `AbstractSkeleton`'s own bow goal.
constexpr std::int32_t kBowIntervalMin = 20;
constexpr std::int32_t kBowIntervalMax = 20;
constexpr double kBowRange = 15.0;

/// parity: `Pillager` and `Illusioner`.
constexpr double kCrossbowRange = 8.0;

/// parity: `EvokerSummonSpellGoal` and `EvokerAttackSpellGoal`, wind-up and wait apart.
constexpr std::int32_t kSummonCast = 100;
constexpr std::int32_t kSummonCooldown = 340;
constexpr std::int32_t kFangCast = 40;
constexpr std::int32_t kFangCooldown = 100;
constexpr std::int32_t kBlindCast = 20;
constexpr std::int32_t kBlindCooldown = 180;

/// parity: `Llama.registerGoals`.
constexpr double kCaravanSpeed = static_cast<double>(2.1F);
constexpr double kSpitSpeed = 1.25;
constexpr std::int32_t kSpitInterval = 40;
constexpr float kSpitRange = 20.0F;

/// parity: `WitherBoss.registerGoals`, a shot every two seconds from twenty blocks.
constexpr std::int32_t kWitherShotInterval = 40;
constexpr double kWitherRange = 20.0;

/// parity: `Monster` and the golem's `DefendVillageTargetGoal`.
const Names& hostile() {
    static const Names kTypes{"zombie",       "husk",       "drowned",         "zombie_villager", "skeleton",
                              "stray",        "bogged",     "wither_skeleton", "creeper",         "spider",
                              "cave_spider",  "silverfish", "endermite",       "enderman",        "witch",
                              "pillager",     "vindicator", "evoker",          "ravager",         "vex",
                              "piglin_brute", "zoglin"};
    return kTypes;
}

const Names& patrollingTypes() {
    static const Names kTypes{"pillager", "vindicator", "evoker", "illusioner", "witch", "ravager"};
    return kTypes;
}

/// parity: `BegGoal`'s own list, which is whatever a wolf will eat.
const Names& wolfBegItems() {
    static const Names kItems{"bone",   "beef",   "porkchop",    "chicken",
                              "mutton", "rabbit", "cooked_beef", "cooked_porkchop"};
    return kItems;
}

template <typename GoalType, typename... Args>
void add(GoalSelector& selector, std::int32_t priority, Args&&... args) {
    selector.add(priority, std::make_unique<GoalType>(std::forward<Args>(args)...));
}

/// parity: `Raider.registerGoals`, which every raider runs before its own.
void addRaiderGoals(GoalSelector& selector) {
    add<ObtainRaidLeaderBannerGoal>(selector, 1);
    add<PathfindToRaidGoal>(selector, 3);
    add<RaiderMoveThroughVillageGoal>(selector, 4);
    add<RaiderCelebrationGoal>(selector, 5);
}

/// parity: the goal set nearly every animal shares, with its own three speeds.
void farmAnimal(GoalSelector& selector, const wire::common::Identifier& type, double panic, double tempt, double follow,
                double stroll = 1.0) {
    add<FloatGoal>(selector, 0);
    add<PanicGoal>(selector, 1, panic);
    add<BreedGoal>(selector, 2, 1.0);
    add<TemptGoal>(selector, 3, tempt, entity::breeding::foodFor(type));
    add<FollowParentGoal>(selector, 4, follow);
    add<RandomStrollGoal>(selector, 5, stroll);
    add<LookAtPlayerGoal>(selector, 6, kLookNear);
    add<RandomLookAroundGoal>(selector, 7);
}

void monsterTail(GoalSelector& selector, double strollSpeed = 1.0) {
    add<RandomStrollGoal>(selector, 7, strollSpeed);
    add<LookAtPlayerGoal>(selector, 8, kLookFar);
    add<RandomLookAroundGoal>(selector, 8);
}

bool buildAnimal(GoalSelector& selector, const wire::common::Identifier& type, std::string_view path);

bool buildTamed(GoalSelector& selector, const wire::common::Identifier& type, std::string_view path);

bool buildUndead(GoalSelector& selector, std::string_view path);

bool buildMonster(GoalSelector& selector, std::string_view path);

bool buildIllager(GoalSelector& selector, std::string_view path);

bool buildFlier(GoalSelector& selector, std::string_view path);

bool buildVillage(GoalSelector& selector, std::string_view path);

bool buildSwimmer(GoalSelector& selector, std::string_view path);

} // namespace

std::unique_ptr<GoalSelector> mobGoals::forType(const wire::common::Identifier& type) {
    auto selector = std::make_unique<GoalSelector>();
    const std::string_view path = type.path();
    // parity: `PatrollingMonster.registerGoals` runs super first.
    if (patrollingTypes().contains(path)) {
        add<LongDistancePatrolGoal>(*selector, 4);
    }
    const bool built = buildAnimal(*selector, type, path) || buildTamed(*selector, type, path) ||
                       buildUndead(*selector, path) || buildMonster(*selector, path) || buildIllager(*selector, path) ||
                       buildFlier(*selector, path) || buildVillage(*selector, path) || buildSwimmer(*selector, path);
    if (!built) {
        return {};
    }
    return selector;
}

namespace {

/// parity: `AbstractHorse.registerGoals`.
bool buildHorse(GoalSelector& selector, std::string_view path) {
    if (path == "llama" || path == "trader_llama") {
        add<FloatGoal>(selector, 0);
        add<RunAroundLikeCrazyGoal>(selector, 1, 1.2);
        add<LlamaFollowCaravanGoal>(selector, 2, kCaravanSpeed);
        add<LlamaSpitGoal>(selector, 3, kSpitSpeed, kSpitInterval, kSpitInterval, kSpitRange);
        add<PanicGoal>(selector, 3, 1.2);
        add<BreedGoal>(selector, 4, 1.0);
        add<TemptGoal>(selector, 5, 1.25, Names{"hay_block"});
        add<FollowParentGoal>(selector, 6, 1.0);
        add<RandomStrollGoal>(selector, 7, 0.7);
        add<LookAtPlayerGoal>(selector, 8, kLookNear);
        add<RandomLookAroundGoal>(selector, 9);
        add<LlamaHurtByTargetGoal>(selector, 1);
        add<LlamaAttackWolfGoal>(selector, 2);
        if (path == "trader_llama") {
            add<PanicGoal>(selector, 1, 2.0);
            add<TraderLlamaDefendTraderGoal>(selector, 1);
            // parity: every `Zombie` but the zombified piglin, and every `AbstractIllager`.
            add<NearestAttackableEntityGoal>(selector, 2, Names{"zombie", "husk", "drowned", "zombie_villager"});
            add<NearestAttackableEntityGoal>(selector, 2, Names{"pillager", "vindicator", "evoker", "illusioner"});
        }
        return true;
    }
    if (path != "horse" && path != "donkey" && path != "mule" && path != "skeleton_horse" && path != "zombie_horse") {
        return false;
    }
    add<RunAroundLikeCrazyGoal>(selector, 1, 1.2);
    add<BreedGoal>(selector, 2, 1.0);
    add<FollowParentGoal>(selector, 4, 1.0);
    add<RandomStrollGoal>(selector, 6, 0.7);
    add<LookAtPlayerGoal>(selector, 7, kLookNear);
    add<RandomLookAroundGoal>(selector, 8);
    add<RandomStandGoal>(selector, 9);
    if (path == "skeleton_horse") {
        return true;
    }
    add<FloatGoal>(selector, 0);
    if (path == "zombie_horse") {
        add<TemptGoal>(selector, 3, 1.25, Names{"red_mushroom"});
        return true;
    }
    add<PanicGoal>(selector, 1, 1.2);
    add<TemptGoal>(selector, 3, 1.25, Names{"golden_carrot", "golden_apple", "enchanted_golden_apple"});
    return true;
}

bool buildAnimal(GoalSelector& selector, const wire::common::Identifier& type, std::string_view path) {
    static const Names kSlow{"turtle", "panda", "fox", "strider"};
    if (buildHorse(selector, path)) {
        return true;
    }
    if (path == "cow" || path == "mooshroom") {
        farmAnimal(selector, type, 2.0, 1.25, 1.25);
        return true;
    }
    if (path == "pig") {
        farmAnimal(selector, type, 1.25, 1.2, 1.1);
        return true;
    }
    if (path == "chicken") {
        farmAnimal(selector, type, 1.4, 1.0, 1.1);
        return true;
    }
    if (kSlow.contains(path)) {
        farmAnimal(selector, type, 1.25, 1.2, 1.1);
        return true;
    }
    if (path == "sheep") {
        add<FloatGoal>(selector, 0);
        add<PanicGoal>(selector, 1, 1.25);
        add<BreedGoal>(selector, 2, 1.0);
        add<TemptGoal>(selector, 3, 1.1, entity::breeding::foodFor(type));
        add<FollowParentGoal>(selector, 4, 1.1);
        add<EatBlockGoal>(selector, 5);
        add<RandomStrollGoal>(selector, 6, 1.0);
        add<LookAtPlayerGoal>(selector, 7, kLookNear);
        add<RandomLookAroundGoal>(selector, 8);
        return true;
    }
    if (path == "rabbit") {
        add<FloatGoal>(selector, 1);
        add<PanicGoal>(selector, 1, 2.2);
        add<BreedGoal>(selector, 2, 0.8);
        add<TemptGoal>(selector, 3, 1.0, entity::breeding::foodFor(type));
        add<AvoidEntityGoal>(selector, 4, Names{"wolf"}, 10.0, 2.2, 2.2);
        add<RandomStrollGoal>(selector, 6, 0.6);
        add<LookAtPlayerGoal>(selector, 11, 10.0);
        return true;
    }
    return false;
}

bool buildTamed(GoalSelector& selector, const wire::common::Identifier& type, std::string_view path) {
    if (path == "wolf") {
        add<FloatGoal>(selector, 1);
        add<PanicGoal>(selector, 1, 1.5);
        add<SitWhenOrderedToGoal>(selector, 2);
        add<WolfAvoidLlamaGoal>(selector, 3);
        add<LeapAtTargetGoal>(selector, 4, 0.4);
        add<MeleeAttackGoal>(selector, 5, 1.0);
        add<FollowOwnerGoal>(selector, 6, 1.0);
        add<BreedGoal>(selector, 7, 1.0);
        add<RandomStrollGoal>(selector, 8, 1.0);
        add<BegGoal>(selector, 9, 8.0, wolfBegItems());
        add<LookAtPlayerGoal>(selector, 10, kLookFar);
        add<RandomLookAroundGoal>(selector, 10);
        add<OwnerHurtByTargetGoal>(selector, 1);
        add<OwnerHurtTargetGoal>(selector, 2);
        add<HurtByTargetGoal>(selector, 3, HurtByTargetGoal::Types{}, true);
        add<AngryAtPlayerGoal>(selector, 4);
        // parity: a wild wolf hunts sheep and rabbits; a tamed one only what its owner does.
        add<NearestAttackableEntityGoal>(selector, 5, Names{"sheep", "rabbit", "skeleton"});
        add<ResetUniversalAngerTargetGoal>(selector, 8, true);
        return true;
    }
    if (path == "cat") {
        // parity: `Cat.registerGoals`, with its 26.3 block goals at 5 and 7.
        add<FloatGoal>(selector, 1);
        add<SitWhenOrderedToGoal>(selector, 2);
        add<CatRelaxOnOwnerGoal>(selector, 3);
        add<TemptGoal>(selector, 4, 0.6, entity::breeding::foodFor(type));
        add<CatLieOnBlockGoal>(selector, 5, 1.1, 8);
        add<FollowOwnerGoal>(selector, 6, 1.0, 10.0, 5.0);
        add<CatSitOnBlockGoal>(selector, 7, 0.8);
        add<BreedGoal>(selector, 10, 0.8);
        add<RandomStrollGoal>(selector, 11, 0.8);
        add<LookAtPlayerGoal>(selector, 12, 10.0);
        add<NearestAttackableEntityGoal>(selector, 1, Names{"rabbit", "chicken", "tadpole"});
        return true;
    }
    if (path == "ocelot") {
        add<FloatGoal>(selector, 1);
        add<SitWhenOrderedToGoal>(selector, 2);
        add<TemptGoal>(selector, 3, 0.6, entity::breeding::foodFor(type));
        add<FollowOwnerGoal>(selector, 5, 1.0);
        add<BreedGoal>(selector, 7, 0.8);
        add<RandomStrollGoal>(selector, 8, 0.8);
        add<LookAtPlayerGoal>(selector, 10, 10.0);
        add<RandomLookAroundGoal>(selector, 11);
        // parity: `Cat.registerGoals`, which hunts what it can catch.
        add<NearestAttackableEntityGoal>(selector, 1, Names{"rabbit", "chicken", "tadpole"});
        return true;
    }
    // parity: `Parrot.registerGoals`, less following other mobs.
    if (path == "parrot") {
        add<FloatGoal>(selector, 0);
        add<LookAtPlayerGoal>(selector, 1, kLookFar);
        add<SitWhenOrderedToGoal>(selector, 2);
        add<FollowOwnerGoal>(selector, 2, 1.0, 5.0, 1.0);
        add<HoverWanderGoal>(selector, 2);
        add<LandOnOwnersShoulderGoal>(selector, 3);
        return true;
    }
    if (path == "polar_bear") {
        add<FloatGoal>(selector, 0);
        add<MeleeAttackGoal>(selector, 1, 1.25);
        add<RandomStrollGoal>(selector, 4, 1.0);
        add<LookAtPlayerGoal>(selector, 5, kLookNear);
        add<RandomLookAroundGoal>(selector, 6);
        add<HurtByTargetGoal>(selector, 1);
        add<AngryAtPlayerGoal>(selector, 3);
        add<ResetUniversalAngerTargetGoal>(selector, 5, false);
        return true;
    }
    // parity: `CopperGolemAi`.
    if (path == "copper_golem") {
        add<PanicGoal>(selector, 0, copperGolems::kPanicSpeed);
        add<CopperGolemTransportGoal>(selector, 1);
        add<CopperGolemIdleGoal>(selector, 2);
        add<LookAtPlayerSometimesGoal>(selector, 3);
        return true;
    }
    if (path == "iron_golem" || path == "snow_golem") {
        add<MeleeAttackGoal>(selector, 1, 1.0);
        add<RandomStrollGoal>(selector, 4, 0.6);
        add<LookAtPlayerGoal>(selector, 7, kLookNear);
        add<RandomLookAroundGoal>(selector, 8);
        add<HurtByTargetGoal>(selector, 2);
        // parity: `IronGolem` alone is a `NeutralMob`.
        if (path == "iron_golem") {
            add<AngryAtPlayerGoal>(selector, 3);
            add<ResetUniversalAngerTargetGoal>(selector, 4, false);
        }
        // parity: `DefendVillageTargetGoal` and the golem's list of what counts as a monster.
        add<NearestAttackableEntityGoal>(selector, 3, hostile());
        return true;
    }
    return false;
}

bool buildUndead(GoalSelector& selector, std::string_view path) {
    static const Names kZombies{"zombie", "husk", "drowned", "zombie_villager", "giant"};
    if (kZombies.contains(path)) {
        // parity: `Zombie.registerGoals`, the attack goal included.
        add<MeleeAttackGoal>(selector, 3, 1.0);
        monsterTail(selector);
        add<HurtByTargetGoal>(selector, 1);
        add<NearestAttackablePlayerGoal>(selector, 2);
        // parity: a zombie goes for a villager or a golem as readily as for a player.
        add<NearestAttackableEntityGoal>(selector, 3, Names{"villager", "wandering_trader", "iron_golem"});
        return true;
    }
    if (path == "zombified_piglin") {
        // parity: `ZombifiedPiglin.addBehaviourGoals`.
        add<MeleeAttackGoal>(selector, 2, 1.0);
        monsterTail(selector);
        add<HurtByTargetGoal>(selector, 1, HurtByTargetGoal::Types{}, true);
        add<AngryAtPlayerGoal>(selector, 2);
        add<ResetUniversalAngerTargetGoal>(selector, 3, true);
        return true;
    }
    if (path == "warden") {
        // parity: `Warden.registerGoals` has no target goal: heard anger decides who it walks to.
        add<FloatGoal>(selector, 0);
        add<WardenBusyGoal>(selector, 0);
        add<MeleeAttackGoal>(selector, 4, 1.0);
        add<WardenInvestigateGoal>(selector, 5);
        add<WardenSniffGoal>(selector, 6);
        monsterTail(selector);
        add<AngerTargetGoal>(selector, 1);
        return true;
    }
    static const Names kBrutes{"wither_skeleton", "zoglin", "vindicator", "ravager"};
    if (kBrutes.contains(path)) {
        if (patrollingTypes().contains(path)) {
            addRaiderGoals(selector);
        }
        add<FloatGoal>(selector, 0);
        add<MeleeAttackGoal>(selector, 4, 1.0);
        monsterTail(selector);
        add<HurtByTargetGoal>(selector, 1);
        add<NearestAttackablePlayerGoal>(selector, 2);
        add<NearestAttackableEntityGoal>(selector, 3, Names{"iron_golem", "villager"});
        return true;
    }
    return false;
}

bool buildMonster(GoalSelector& selector, std::string_view path) {
    // parity: `Shulker.registerGoals`, less the team defence.
    if (path == "shulker") {
        add<LookAtPlayerGoal>(selector, 1, kLookFar, 0.02F);
        add<ShulkerAttackGoal>(selector, 4);
        add<ShulkerPeekGoal>(selector, 7);
        add<RandomLookAroundGoal>(selector, 8);
        add<HurtByTargetGoal>(selector, 1);
        add<ShulkerTargetGoal>(selector, 2);
        return true;
    }
    if (path == "silverfish" || path == "endermite") {
        add<FloatGoal>(selector, 1);
        add<MeleeAttackGoal>(selector, 2, 1.0);
        add<RandomStrollGoal>(selector, 3, 1.0);
        add<LookAtPlayerGoal>(selector, 7, kLookFar);
        add<RandomLookAroundGoal>(selector, 8);
        add<HurtByTargetGoal>(selector, 1);
        add<NearestAttackablePlayerGoal>(selector, 2);
        return true;
    }
    if (path == "spider" || path == "cave_spider") {
        add<FloatGoal>(selector, 1);
        // parity: `Spider.onClimbable`, which is what walks one up a wall.
        add<ClimbWallsGoal>(selector, 2);
        add<LeapAtTargetGoal>(selector, 3, 0.4);
        add<MeleeAttackGoal>(selector, 4, 0.8);
        add<RandomStrollGoal>(selector, 5, 0.8);
        add<LookAtPlayerGoal>(selector, 6, kLookFar);
        add<RandomLookAroundGoal>(selector, 6);
        add<HurtByTargetGoal>(selector, 1);
        // parity: `Spider.SpiderTargetGoal`, which needs the light below a half.
        add<NearestAttackablePlayerGoal>(selector, 2, NearestAttackablePlayerGoal::kDefaultInterval, true);
        add<NearestAttackableEntityGoal>(selector, 3, Names{"iron_golem"});
        return true;
    }
    if (path == "enderman") {
        // parity: minus carrying blocks about, which is its own pair of goals.
        add<FloatGoal>(selector, 0);
        add<EndermanFreezeWhenLookedAtGoal>(selector, 1);
        add<TeleportAwayGoal>(selector, 1);
        add<MeleeAttackGoal>(selector, 2, 1.0);
        monsterTail(selector);
        add<EndermanLookForPlayerGoal>(selector, 1);
        add<HurtByTargetGoal>(selector, 2);
        add<NearestAttackableEntityGoal>(selector, 3, Names{"endermite"});
        add<ResetUniversalAngerTargetGoal>(selector, 4, false);
        return true;
    }
    if (path == "creeper") {
        add<FloatGoal>(selector, 1);
        add<SwellGoal>(selector, 2);
        add<AvoidEntityGoal>(selector, 3, Names{"ocelot", "cat"}, 6.0, 1.0, 1.2);
        add<MeleeAttackGoal>(selector, 4, 1.0);
        add<RandomStrollGoal>(selector, 5, 0.8);
        add<LookAtPlayerGoal>(selector, 6, kLookFar);
        add<RandomLookAroundGoal>(selector, 6);
        add<NearestAttackablePlayerGoal>(selector, 1);
        add<HurtByTargetGoal>(selector, 2);
        return true;
    }
    static const Names kSkeletons{"skeleton", "stray", "bogged", "parched"};
    if (kSkeletons.contains(path)) {
        // parity: `AbstractSkeleton.reassessWeaponGoal`: the bow goal whenever one is held.
        add<AvoidEntityGoal>(selector, 3, Names{"wolf"}, 6.0, 1.0, 1.2);
        add<RangedAttackGoal>(selector, 4, 1.0, kBowIntervalMin, kBowIntervalMax, kBowRange);
        add<RandomStrollGoal>(selector, 5, 1.0);
        add<LookAtPlayerGoal>(selector, 6, kLookFar);
        add<RandomLookAroundGoal>(selector, 6);
        add<HurtByTargetGoal>(selector, 1);
        add<NearestAttackablePlayerGoal>(selector, 2);
        return true;
    }
    // parity: `AbstractCubeMob`: turns and hops, three times as often at its target.
    static const Names kCubes{"slime", "magma_cube", "sulfur_cube"};
    if (kCubes.contains(path)) {
        add<HopAboutGoal>(selector, 2);
        add<NearestAttackablePlayerGoal>(selector, 1);
        add<HurtByTargetGoal>(selector, 3);
        return true;
    }
    return false;
}

bool buildIllager(GoalSelector& selector, std::string_view path) {
    if (path == "witch") {
        addRaiderGoals(selector);
        add<FloatGoal>(selector, 0);
        add<ThrowPotionGoal>(selector, 2);
        add<RandomStrollGoal>(selector, 3, 1.0);
        add<LookAtPlayerGoal>(selector, 3, kLookFar);
        add<RandomLookAroundGoal>(selector, 4);
        add<HurtByTargetGoal>(selector, 1);
        add<NearestAttackablePlayerGoal>(selector, 2);
        return true;
    }
    if (path == "pillager") {
        addRaiderGoals(selector);
        add<FloatGoal>(selector, 0);
        add<CrossbowAttackGoal>(selector, 3, 1.0, kCrossbowRange);
        add<RandomStrollGoal>(selector, 8, 0.6);
        add<LookAtPlayerGoal>(selector, 9, kLookMeet, 1.0F);
        add<RandomLookAroundGoal>(selector, 10);
        add<HurtByTargetGoal>(selector, 1);
        add<NearestAttackablePlayerGoal>(selector, 2);
        add<NearestAttackableEntityGoal>(selector, 3, Names{"villager", "iron_golem", "wandering_trader"});
        return true;
    }
    if (path == "evoker") {
        addRaiderGoals(selector);
        add<FloatGoal>(selector, 0);
        // parity: `Evoker.registerGoals`, the fangs before the vexes and the wololo left out.
        add<CastSpellGoal>(selector, 4, CastSpellGoal::Spell::SummonVexes, kSummonCast, kSummonCooldown);
        add<CastSpellGoal>(selector, 5, CastSpellGoal::Spell::Fangs, kFangCast, kFangCooldown);
        add<RandomStrollGoal>(selector, 8, 0.6);
        add<LookAtPlayerGoal>(selector, 9, kLookMeet, 1.0F);
        add<RandomLookAroundGoal>(selector, 10);
        add<HurtByTargetGoal>(selector, 1);
        add<NearestAttackablePlayerGoal>(selector, 2);
        add<NearestAttackableEntityGoal>(selector, 3, Names{"villager", "iron_golem"});
        return true;
    }
    if (path == "illusioner") {
        addRaiderGoals(selector);
        add<FloatGoal>(selector, 0);
        add<CastSpellGoal>(selector, 4, CastSpellGoal::Spell::Blindness, kBlindCast, kBlindCooldown);
        add<RangedAttackGoal>(selector, 5, 0.5, kBowIntervalMin, kBowIntervalMax, kBowRange);
        add<RandomStrollGoal>(selector, 8, 0.6);
        add<LookAtPlayerGoal>(selector, 9, kLookMeet, 1.0F);
        add<RandomLookAroundGoal>(selector, 10);
        add<HurtByTargetGoal>(selector, 1);
        add<NearestAttackablePlayerGoal>(selector, 2);
        add<NearestAttackableEntityGoal>(selector, 3, Names{"villager", "iron_golem"});
        return true;
    }
    return false;
}

bool buildFlier(GoalSelector& selector, std::string_view path) {
    // parity: `Ghast.registerGoals`.
    if (path == "ghast") {
        add<GhastFloatGoal>(selector, 5);
        add<GhastLookGoal>(selector, 7);
        add<FireballAttackGoal>(selector, 7);
        add<NearestAttackablePlayerGoal>(selector, 1);
        return true;
    }
    // parity: `Blaze.registerGoals`, walking with its own slow fall.
    if (path == "blaze") {
        add<BlazeAttackGoal>(selector, 4);
        add<RandomStrollGoal>(selector, 7, 1.0);
        add<LookAtPlayerGoal>(selector, 8, kLookFar);
        add<RandomLookAroundGoal>(selector, 8);
        add<HurtByTargetGoal>(selector, 1, Names{}, true);
        add<NearestAttackablePlayerGoal>(selector, 2);
        return true;
    }
    if (path == "phantom") {
        add<PhantomStrategyGoal>(selector, 1);
        add<PhantomSweepGoal>(selector, 2);
        add<PhantomCircleGoal>(selector, 3);
        add<PhantomTargetGoal>(selector, 1);
        return true;
    }
    // parity: `Vex.registerGoals`, less copying its owner's target.
    if (path == "vex") {
        add<FloatGoal>(selector, 0);
        add<VexChargeGoal>(selector, 4);
        add<VexRandomMoveGoal>(selector, 8);
        add<LookAtPlayerGoal>(selector, 9, kLookMeet, 1.0F);
        add<HurtByTargetGoal>(selector, 1, Names{"pillager", "vindicator", "evoker", "illusioner", "witch", "ravager"},
                              true);
        add<NearestAttackablePlayerGoal>(selector, 3);
        return true;
    }
    // parity: `WitherBoss.registerGoals`.
    if (path == "wither") {
        add<WitherDoNothingGoal>(selector, 0);
        add<RangedAttackGoal>(selector, 2, 1.0, kWitherShotInterval, kWitherShotInterval, kWitherRange);
        add<HoverWanderGoal>(selector, 5);
        add<LookAtPlayerGoal>(selector, 6, kLookFar);
        add<RandomLookAroundGoal>(selector, 7);
        add<HurtByTargetGoal>(selector, 1);
        add<WitherTargetGoal>(selector, 2);
        return true;
    }
    // parity: `Bat.customServerAiStep`, which is all a bat does.
    if (path == "bat") {
        add<BatFlightGoal>(selector, 0);
        return true;
    }
    if (path == "bee") {
        beeGoals::registerGoals(selector);
        return true;
    }
    return false;
}

bool buildVillage(GoalSelector& selector, std::string_view path) {
    if (path == "wandering_trader") {
        add<FloatGoal>(selector, 0);
        add<AvoidEntityGoal>(selector, 1, Names{"zombie"}, 8.0, 0.5, 0.5);
        add<AvoidEntityGoal>(selector, 1, Names{"evoker"}, 12.0, 0.5, 0.5);
        add<AvoidEntityGoal>(selector, 1, Names{"vindicator"}, 8.0, 0.5, 0.5);
        add<AvoidEntityGoal>(selector, 1, Names{"vex"}, 8.0, 0.5, 0.5);
        add<AvoidEntityGoal>(selector, 1, Names{"pillager"}, 15.0, 0.5, 0.5);
        add<AvoidEntityGoal>(selector, 1, Names{"illusioner"}, 12.0, 0.5, 0.5);
        add<AvoidEntityGoal>(selector, 1, Names{"zoglin"}, 10.0, 0.5, 0.5);
        add<PanicGoal>(selector, 1, 0.5);
        add<WanderingTraderWanderGoal>(selector, 2);
        add<MoveTowardsTraderHomeGoal>(selector, 4);
        add<RandomStrollGoal>(selector, 8, 0.35);
        add<LookAtPlayerGoal>(selector, 10, kLookFar);
        return true;
    }
    return false;
}

bool buildSwimmer(GoalSelector& selector, std::string_view path) {
    // parity: `Squid.SquidRandomMovementGoal` and `AbstractFish.FishSwimGoal` push about in water without a path.
    if (path == "squid" || path == "glow_squid") {
        add<SwimAboutGoal>(selector, 0);
        return true;
    }
    static const Names kFish{"cod", "salmon", "tropical_fish", "pufferfish", "tadpole"};
    if (kFish.contains(path)) {
        add<SwimAboutGoal>(selector, 4, kFishPush);
        return true;
    }
    // parity: `Dolphin.registerGoals`.
    if (path == "dolphin") {
        add<BreathAirGoal>(selector, 0);
        add<TryFindWaterGoal>(selector, 0);
        add<DolphinSwimToTreasureGoal>(selector, 1);
        add<DolphinSwimWithPlayerGoal>(selector, 2, 4.0);
        add<RandomSwimmingGoal>(selector, 4, 1.0, 10);
        add<RandomLookAroundGoal>(selector, 4);
        add<LookAtPlayerGoal>(selector, 5, 6.0);
        add<DolphinJumpGoal>(selector, 5, 10);
        add<MeleeAttackGoal>(selector, 6, static_cast<double>(1.2F));
        add<DolphinMoveToItemGoal>(selector, 7);
        add<DolphinPlayWithItemsGoal>(selector, 8);
        add<FollowRiddenBoatGoal>(selector, 8);
        add<AvoidEntityGoal>(selector, 9, Names{"guardian", "elder_guardian"}, 8.0, 1.0, 1.0);
        add<HurtByTargetGoal>(selector, 1, Names{"guardian", "elder_guardian"}, true);
        return true;
    }
    if (path == "guardian" || path == "elder_guardian") {
        add<BeamAttackGoal>(selector, 4);
        add<SwimAboutGoal>(selector, 5);
        add<LookAtPlayerGoal>(selector, 7, kLookFar);
        add<RandomLookAroundGoal>(selector, 8);
        add<HurtByTargetGoal>(selector, 1);
        add<NearestAttackablePlayerGoal>(selector, 2);
        return true;
    }
    // parity: a boss runs on its own phases rather than goals, and neither exists yet.
    return false;
}

} // namespace

} // namespace basalt::server::runtime::entity::ai
