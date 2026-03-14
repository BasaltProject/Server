#include <basalt/server/runtime/entity/Breeding.hpp>
#include <basalt/server/runtime/entity/ai/FlyingGoals.hpp>
#include <basalt/server/runtime/entity/ai/GoalSelector.hpp>
#include <basalt/server/runtime/entity/ai/Goals.hpp>
#include <basalt/server/runtime/entity/ai/IllagerGoals.hpp>
#include <basalt/server/runtime/entity/ai/LongDistancePatrolGoal.hpp>
#include <basalt/server/runtime/entity/ai/MobGoals.hpp>
#include <basalt/server/runtime/entity/ai/SpecialGoals.hpp>
#include <basalt/server/runtime/entity/ai/TameGoals.hpp>
#include <basalt/server/runtime/entity/ai/TargetGoals.hpp>
#include <basalt/server/runtime/entity/ai/VillagerGoals.hpp>
#include <basalt/server/runtime/entity/ai/WanderingTraderGoals.hpp>
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

/// parity: `AbstractSkeleton`'s own bow goal: a shot every twenty ticks inside fifteen blocks.
constexpr std::int32_t kBowIntervalMin = 20;
constexpr std::int32_t kBowIntervalMax = 20;
constexpr double kBowRange = 15.0;

/// parity: `Pillager` and `Illusioner`, both of which carry their weapon eight blocks out.
constexpr double kCrossbowRange = 8.0;

/// parity: `EvokerSummonSpellGoal` and `EvokerAttackSpellGoal`, wind-up and wait apart.
constexpr std::int32_t kSummonCast = 100;
constexpr std::int32_t kSummonCooldown = 340;
constexpr std::int32_t kFangCast = 40;
constexpr std::int32_t kFangCooldown = 100;
constexpr std::int32_t kBlindCast = 20;
constexpr std::int32_t kBlindCooldown = 180;

/// parity: how far each flier drifts and how hard it pushes itself along, which is not the same for
/// any two of them: the vex has its own move control, the rest fly on the 0.02 air input.
constexpr std::int32_t kBlazeRange = 8;
constexpr double kBlazeStep = 0.012;
constexpr std::int32_t kVexRange = 8;
constexpr double kVexStep = 0.03;
constexpr std::int32_t kBatRange = 7;
constexpr double kBatStep = 0.008;
/// parity: `WitherBoss.registerGoals`, a shot every two seconds from twenty blocks.
constexpr std::int32_t kWitherShotInterval = 40;
constexpr double kWitherRange = 20.0;

/// parity: `PathfindToRaidGoal`, which walks a raider into the village at its own pace.
constexpr double kRaidSpeed = 1.0;

/// parity: the villager's own 0.5 speed modifier.
constexpr double kVillagerSpeed = 0.5;

/// parity: `Monster` and the golem's `DefendVillageTargetGoal`: what an iron golem goes after.
const Names& hostile() {
    static const Names kTypes{"zombie",       "husk",       "drowned",         "zombie_villager", "skeleton",
                              "stray",        "bogged",     "wither_skeleton", "creeper",         "spider",
                              "cave_spider",  "silverfish", "endermite",       "enderman",        "witch",
                              "pillager",     "vindicator", "evoker",          "ravager",         "vex",
                              "piglin_brute", "zoglin"};
    return kTypes;
}

/// Every Raider inherits `PatrollingMonster`'s priority-four long-distance goal.
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

/// parity: what a villager runs from.
const Names& villagerThreats() {
    static const Names kTypes{"zombie", "husk",     "drowned",    "zombie_villager", "zoglin", "vindicator",
                              "evoker", "pillager", "illusioner", "ravager",         "vex",    "witch"};
    return kTypes;
}

template <typename GoalType, typename... Args>
void add(GoalSelector& selector, std::int32_t priority, Args&&... args) {
    selector.add(priority, std::make_unique<GoalType>(std::forward<Args>(args)...));
}

/// parity: the goal set nearly every animal shares, with its own three speeds.
void farmAnimal(GoalSelector& selector, const wire::common::Identifier& type, double panic, double tempt,
                double follow) {
    add<FloatGoal>(selector, 0);
    add<PanicGoal>(selector, 1, panic);
    add<BreedGoal>(selector, 2, 1.0);
    add<TemptGoal>(selector, 3, tempt, entity::breeding::foodFor(type));
    add<FollowParentGoal>(selector, 4, follow);
    add<RandomStrollGoal>(selector, 5, 1.0);
    add<LookAtPlayerGoal>(selector, 6, kLookNear);
    add<RandomLookAroundGoal>(selector, 7);
}

/// The tail every walking monster shares: wander, look about, and go for whoever is nearest.
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
    // PatrollingMonster.registerGoals runs through super before each concrete Raider adds its own
    // priority-four goals, so insertion order at the same priority matters here too.
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

bool buildAnimal(GoalSelector& selector, const wire::common::Identifier& type, std::string_view path) {
    static const Names kSlow{"goat",   "horse", "donkey", "mule", "llama",   "camel", "sniffer", "armadillo",
                             "turtle", "panda", "fox",    "bee",  "strider", "frog",  "axolotl"};
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
    if (path == "trader_llama") {
        farmAnimal(selector, type, 2.0, 1.2, 1.1);
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
        add<HurtByTargetGoal>(selector, 3);
        // parity: a wild wolf hunts sheep and rabbits; a tamed one only what its owner does.
        add<NearestAttackableEntityGoal>(selector, 4, Names{"sheep", "rabbit", "skeleton"});
        return true;
    }
    if (path == "cat" || path == "ocelot") {
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
    if (path == "parrot") {
        add<FloatGoal>(selector, 0);
        add<SitWhenOrderedToGoal>(selector, 2);
        add<FollowOwnerGoal>(selector, 2, 1.0);
        add<RandomStrollGoal>(selector, 3, 1.0);
        add<LookAtPlayerGoal>(selector, 3, kLookNear);
        return true;
    }
    if (path == "polar_bear") {
        add<FloatGoal>(selector, 0);
        add<MeleeAttackGoal>(selector, 1, 1.25);
        add<RandomStrollGoal>(selector, 4, 1.0);
        add<LookAtPlayerGoal>(selector, 5, kLookNear);
        add<RandomLookAroundGoal>(selector, 6);
        add<HurtByTargetGoal>(selector, 1);
        return true;
    }
    if (path == "iron_golem" || path == "snow_golem") {
        add<MeleeAttackGoal>(selector, 1, 1.0);
        add<RandomStrollGoal>(selector, 4, 0.6);
        add<LookAtPlayerGoal>(selector, 7, kLookNear);
        add<RandomLookAroundGoal>(selector, 8);
        add<HurtByTargetGoal>(selector, 2);
        // parity: `DefendVillageTargetGoal` and the golem's own list of what counts as a monster,
        // which is everything hostile that walks.
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
        // parity: it only ever goes for whoever hit it or its neighbours.
        add<MeleeAttackGoal>(selector, 2, 1.0);
        monsterTail(selector);
        add<HurtByTargetGoal>(selector, 1);
        return true;
    }
    if (path == "warden") {
        // parity: `Warden.registerGoals` has no target goal at all: the anger it has heard is what
        // decides who it walks to, and there is nobody until it has heard somebody.
        add<FloatGoal>(selector, 0);
        add<MeleeAttackGoal>(selector, 4, 1.0);
        monsterTail(selector);
        add<AngerTargetGoal>(selector, 1);
        return true;
    }
    static const Names kBrutes{"wither_skeleton", "piglin_brute", "hoglin", "zoglin", "vindicator", "ravager"};
    if (kBrutes.contains(path)) {
        add<FloatGoal>(selector, 0);
        add<PathfindToRaidGoal>(selector, 3, kRaidSpeed);
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
        add<TeleportAwayGoal>(selector, 1);
        add<MeleeAttackGoal>(selector, 2, 1.0);
        monsterTail(selector);
        add<StareTriggerGoal>(selector, 1);
        add<HurtByTargetGoal>(selector, 2);
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
        // parity: `AbstractSkeleton.reassessWeaponGoal`, which is the bow goal whenever it is
        // holding one, and every skeleton this build spawns is.
        add<AvoidEntityGoal>(selector, 3, Names{"wolf"}, 6.0, 1.0, 1.2);
        add<RangedAttackGoal>(selector, 4, 1.0, kBowIntervalMin, kBowIntervalMax, kBowRange);
        add<RandomStrollGoal>(selector, 5, 1.0);
        add<LookAtPlayerGoal>(selector, 6, kLookFar);
        add<RandomLookAroundGoal>(selector, 6);
        add<HurtByTargetGoal>(selector, 1);
        add<NearestAttackablePlayerGoal>(selector, 2);
        return true;
    }
    // parity: `AbstractCubeMob`: a cube turns and hops, and hops three times as often at whatever it
    // is after.
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
        add<FloatGoal>(selector, 0);
        add<PathfindToRaidGoal>(selector, 3, kRaidSpeed);
        add<ThrowPotionGoal>(selector, 2);
        add<RandomStrollGoal>(selector, 3, 1.0);
        add<LookAtPlayerGoal>(selector, 3, kLookFar);
        add<RandomLookAroundGoal>(selector, 4);
        add<HurtByTargetGoal>(selector, 1);
        add<NearestAttackablePlayerGoal>(selector, 2);
        return true;
    }
    if (path == "pillager") {
        add<FloatGoal>(selector, 0);
        add<PathfindToRaidGoal>(selector, 3, kRaidSpeed);
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
        add<FloatGoal>(selector, 0);
        add<PathfindToRaidGoal>(selector, 3, kRaidSpeed);
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
        add<FloatGoal>(selector, 0);
        add<PathfindToRaidGoal>(selector, 3, kRaidSpeed);
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
    if (path == "ghast") {
        add<FlyingWanderGoal>(selector, 5);
        add<FireballAttackGoal>(selector, 7);
        add<NearestAttackablePlayerGoal>(selector, 1);
        return true;
    }
    if (path == "blaze") {
        add<BlazeAttackGoal>(selector, 4);
        add<FlyingWanderGoal>(selector, 5, kBlazeRange, kBlazeStep, false);
        add<LookAtPlayerGoal>(selector, 7, kLookFar);
        add<RandomLookAroundGoal>(selector, 8);
        add<HurtByTargetGoal>(selector, 1);
        add<NearestAttackablePlayerGoal>(selector, 2);
        return true;
    }
    if (path == "phantom") {
        add<SwoopAttackGoal>(selector, 1);
        add<FlyingWanderGoal>(selector, 2);
        add<NearestAttackablePlayerGoal>(selector, 1);
        return true;
    }
    if (path == "vex") {
        add<ChargeAttackGoal>(selector, 4);
        add<FlyingWanderGoal>(selector, 9, kVexRange, kVexStep, false);
        add<HurtByTargetGoal>(selector, 1);
        add<NearestAttackablePlayerGoal>(selector, 2);
        return true;
    }
    // parity: `WitherBoss.registerGoals`.
    if (path == "wither") {
        add<WitherDoNothingGoal>(selector, 0);
        add<RangedAttackGoal>(selector, 2, 1.0, kWitherShotInterval, kWitherShotInterval, kWitherRange);
        add<FlyingWanderGoal>(selector, 5, kBatRange, kBatStep, false);
        add<LookAtPlayerGoal>(selector, 6, kLookFar);
        add<RandomLookAroundGoal>(selector, 7);
        add<HurtByTargetGoal>(selector, 1);
        add<WitherTargetGoal>(selector, 2);
        return true;
    }
    if (path == "bat" || path == "allay") {
        add<FlyingWanderGoal>(selector, 1, kBatRange, kBatStep, false);
        add<LookAtPlayerGoal>(selector, 2, kLookFar);
        return true;
    }
    return false;
}

bool buildVillage(GoalSelector& selector, std::string_view path) {
    if (path == "villager") {
        // parity: a villager runs on a brain rather than on goals, so this is that brain's schedule
        // read by goals: the job site by day, the bell in the afternoon, the bed at night, and a run
        // from anything that raids the village.
        add<FloatGoal>(selector, 0);
        add<AvoidEntityGoal>(selector, 1, villagerThreats(), 8.0, 0.5, 0.7);
        add<SleepInBedGoal>(selector, 2, kVillagerSpeed);
        add<WorkAtSiteGoal>(selector, 3, kVillagerSpeed);
        add<MeetAtBellGoal>(selector, 4, kVillagerSpeed);
        add<RandomStrollGoal>(selector, 5, 0.6);
        add<LookAtPlayerGoal>(selector, 6, kLookNear);
        add<RandomLookAroundGoal>(selector, 7);
        return true;
    }
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
    // parity: `Squid.SquidRandomMovementGoal` and `AbstractFish.FishSwimGoal`, neither of which
    // walks a path: they push themselves about in the water.
    if (path == "squid" || path == "glow_squid") {
        add<SwimAboutGoal>(selector, 0);
        return true;
    }
    static const Names kFish{"cod", "salmon", "tropical_fish", "pufferfish", "tadpole"};
    if (kFish.contains(path)) {
        add<SwimAboutGoal>(selector, 4, kFishPush);
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
