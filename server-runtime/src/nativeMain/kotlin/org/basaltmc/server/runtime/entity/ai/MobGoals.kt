package org.basaltmc.server.runtime.entity.ai

import org.basaltmc.server.runtime.entity.Breeding
import org.basaltmc.wire.common.Identifier

/**
 * Which goals each kind of mob is born with.
 *
 * parity: every mob's own `registerGoals`, priority for priority. What is missing is named in the
 * comment beside it rather than replaced by something that looks similar: a skeleton that cannot
 * shoot yet is a skeleton with no attack goal, not a skeleton that punches.
 */
public object MobGoals {
    /** parity: `Mob.registerGoals` for this type, or null when this build has nothing for it. */
    public fun forType(type: Identifier): GoalSelector? {
        val selector = GoalSelector()
        when (type.path) {
            "cow", "mooshroom" -> farmAnimal(selector, type, panic = 2.0, tempt = 1.25, follow = 1.25)
            "pig" -> farmAnimal(selector, type, panic = 1.25, tempt = 1.2, follow = 1.1)
            "chicken" -> farmAnimal(selector, type, panic = 1.4, tempt = 1.0, follow = 1.1)
            "sheep" -> {
                selector.add(0, FloatGoal())
                selector.add(1, PanicGoal(1.25))
                selector.add(2, BreedGoal(1.0))
                selector.add(3, TemptGoal(1.1, Breeding.foodFor(type)))
                selector.add(4, FollowParentGoal(1.1))
                selector.add(5, EatBlockGoal())
                selector.add(6, RandomStrollGoal(1.0))
                selector.add(7, LookAtPlayerGoal(LOOK_NEAR))
                selector.add(8, RandomLookAroundGoal())
            }
            "rabbit" -> {
                selector.add(1, FloatGoal())
                selector.add(1, PanicGoal(2.2))
                selector.add(2, BreedGoal(0.8))
                selector.add(3, TemptGoal(1.0, Breeding.foodFor(type)))
                selector.add(4, AvoidEntityGoal(setOf("wolf"), 10.0, 2.2, 2.2))
                selector.add(6, RandomStrollGoal(0.6))
                selector.add(11, LookAtPlayerGoal(10.0))
            }
            "goat", "horse", "donkey", "mule", "llama", "camel", "sniffer", "armadillo",
            "turtle", "panda", "fox", "bee", "strider", "frog", "axolotl",
            -> farmAnimal(selector, type, panic = 1.25, tempt = 1.2, follow = 1.1)
            "wolf" -> {
                selector.add(1, FloatGoal())
                selector.add(1, PanicGoal(1.5))
                selector.add(2, SitWhenOrderedToGoal())
                selector.add(4, LeapAtTargetGoal(0.4))
                selector.add(5, MeleeAttackGoal(1.0, followingTargetEvenIfNotSeen = true))
                selector.add(6, FollowOwnerGoal(1.0))
                selector.add(7, BreedGoal(1.0))
                selector.add(8, RandomStrollGoal(1.0))
                selector.add(9, BegGoal(8.0, WOLF_BEG_ITEMS))
                selector.add(10, LookAtPlayerGoal(LOOK_FAR))
                selector.add(10, RandomLookAroundGoal())
                selector.add(1, OwnerHurtByTargetGoal())
                selector.add(2, OwnerHurtTargetGoal())
                selector.add(3, HurtByTargetGoal())
                // parity: a wild wolf hunts sheep and rabbits; a tamed one only what its owner does.
                selector.add(4, NearestAttackableEntityGoal(setOf("sheep", "rabbit", "skeleton")))
            }
            "cat", "ocelot" -> {
                selector.add(1, FloatGoal())
                selector.add(2, SitWhenOrderedToGoal())
                selector.add(3, TemptGoal(0.6, Breeding.foodFor(type)))
                selector.add(5, FollowOwnerGoal(1.0))
                selector.add(7, BreedGoal(0.8))
                selector.add(8, RandomStrollGoal(0.8))
                selector.add(10, LookAtPlayerGoal(10.0))
                selector.add(11, RandomLookAroundGoal())
                // parity: `Cat.registerGoals`, which hunts what it can catch.
                selector.add(1, NearestAttackableEntityGoal(setOf("rabbit", "chicken", "tadpole")))
            }
            "parrot" -> {
                selector.add(0, FloatGoal())
                selector.add(2, SitWhenOrderedToGoal())
                selector.add(2, FollowOwnerGoal(1.0))
                selector.add(3, RandomStrollGoal(1.0))
                selector.add(3, LookAtPlayerGoal(LOOK_NEAR))
            }
            "polar_bear" -> {
                selector.add(0, FloatGoal())
                selector.add(1, MeleeAttackGoal(1.25, followingTargetEvenIfNotSeen = true))
                selector.add(4, RandomStrollGoal(1.0))
                selector.add(5, LookAtPlayerGoal(LOOK_NEAR))
                selector.add(6, RandomLookAroundGoal())
                selector.add(1, HurtByTargetGoal())
            }
            "iron_golem", "snow_golem" -> {
                selector.add(1, MeleeAttackGoal(1.0, followingTargetEvenIfNotSeen = true))
                selector.add(4, RandomStrollGoal(0.6))
                selector.add(7, LookAtPlayerGoal(LOOK_NEAR))
                selector.add(8, RandomLookAroundGoal())
                selector.add(2, HurtByTargetGoal())
                // parity: `DefendVillageTargetGoal` and the golem's own list of what counts as a
                // monster, which is everything hostile that walks.
                selector.add(3, NearestAttackableEntityGoal(HOSTILE))
            }
            // parity: `Zombie.registerGoals`, the attack goal included.
            "zombie", "husk", "drowned", "zombie_villager", "giant" -> {
                selector.add(3, MeleeAttackGoal(1.0))
                selector.add(7, RandomStrollGoal(1.0))
                selector.add(8, LookAtPlayerGoal(LOOK_FAR))
                selector.add(8, RandomLookAroundGoal())
                selector.add(1, HurtByTargetGoal())
                selector.add(2, NearestAttackablePlayerGoal())
                // parity: a zombie goes for a villager or a golem as readily as for a player.
                selector.add(3, NearestAttackableEntityGoal(setOf("villager", "wandering_trader", "iron_golem")))
            }
            "zombified_piglin" -> {
                // parity: it only ever goes for whoever hit it or its neighbours.
                selector.add(2, MeleeAttackGoal(1.0))
                selector.add(7, RandomStrollGoal(1.0))
                selector.add(8, LookAtPlayerGoal(LOOK_FAR))
                selector.add(8, RandomLookAroundGoal())
                selector.add(1, HurtByTargetGoal())
            }
            "wither_skeleton", "piglin_brute", "hoglin", "zoglin", "vindicator", "ravager", "warden" -> {
                selector.add(0, FloatGoal())
                selector.add(3, PathfindToRaidGoal(RAID_SPEED))
                selector.add(4, MeleeAttackGoal(1.0))
                selector.add(7, RandomStrollGoal(1.0))
                selector.add(8, LookAtPlayerGoal(LOOK_FAR))
                selector.add(8, RandomLookAroundGoal())
                selector.add(1, HurtByTargetGoal())
                selector.add(2, NearestAttackablePlayerGoal())
                selector.add(3, NearestAttackableEntityGoal(setOf("iron_golem", "villager")))
            }
            "silverfish", "endermite" -> {
                selector.add(1, FloatGoal())
                selector.add(2, MeleeAttackGoal(1.0))
                selector.add(3, RandomStrollGoal(1.0))
                selector.add(7, LookAtPlayerGoal(LOOK_FAR))
                selector.add(8, RandomLookAroundGoal())
                selector.add(1, HurtByTargetGoal())
                selector.add(2, NearestAttackablePlayerGoal())
            }
            "spider", "cave_spider" -> {
                selector.add(1, FloatGoal())
                // parity: `Spider.onClimbable`, which is what walks one up a wall.
                selector.add(2, ClimbWallsGoal())
                selector.add(3, LeapAtTargetGoal(0.4))
                selector.add(4, MeleeAttackGoal(0.8))
                selector.add(5, RandomStrollGoal(0.8))
                selector.add(6, LookAtPlayerGoal(LOOK_FAR))
                selector.add(6, RandomLookAroundGoal())
                selector.add(1, HurtByTargetGoal())
                // parity: `Spider.SpiderTargetGoal`, which needs the light below a half.
                selector.add(2, NearestAttackablePlayerGoal(needsDarkness = true))
                selector.add(3, NearestAttackableEntityGoal(setOf("iron_golem")))
            }
            "enderman" -> {
                // parity: minus carrying blocks about, which is its own pair of goals.
                selector.add(0, FloatGoal())
                selector.add(1, TeleportAwayGoal())
                selector.add(2, MeleeAttackGoal(1.0))
                selector.add(7, RandomStrollGoal(1.0))
                selector.add(8, LookAtPlayerGoal(LOOK_FAR))
                selector.add(8, RandomLookAroundGoal())
                selector.add(1, StareTriggerGoal())
                selector.add(2, HurtByTargetGoal())
            }
            "creeper" -> {
                selector.add(1, FloatGoal())
                selector.add(2, SwellGoal())
                selector.add(3, AvoidEntityGoal(setOf("ocelot", "cat"), 6.0, 1.0, 1.2))
                selector.add(4, MeleeAttackGoal(1.0))
                selector.add(5, RandomStrollGoal(0.8))
                selector.add(6, LookAtPlayerGoal(LOOK_FAR))
                selector.add(6, RandomLookAroundGoal())
                selector.add(1, NearestAttackablePlayerGoal())
                selector.add(2, HurtByTargetGoal())
            }
            "skeleton", "stray", "bogged", "parched" -> {
                // parity: `AbstractSkeleton.reassessWeaponGoal`, which is the bow goal whenever it
                // is holding one, and every skeleton this build spawns is.
                selector.add(3, AvoidEntityGoal(setOf("wolf"), 6.0, 1.0, 1.2))
                selector.add(4, RangedAttackGoal(1.0, BOW_INTERVAL_MIN, BOW_INTERVAL_MAX, BOW_RANGE))
                selector.add(5, RandomStrollGoal(1.0))
                selector.add(6, LookAtPlayerGoal(LOOK_FAR))
                selector.add(6, RandomLookAroundGoal())
                selector.add(1, HurtByTargetGoal())
                selector.add(2, NearestAttackablePlayerGoal())
            }
            "witch" -> {
                selector.add(0, FloatGoal())
                selector.add(3, PathfindToRaidGoal(RAID_SPEED))
                selector.add(2, ThrowPotionGoal())
                selector.add(3, RandomStrollGoal(1.0))
                selector.add(3, LookAtPlayerGoal(LOOK_FAR))
                selector.add(4, RandomLookAroundGoal())
                selector.add(1, HurtByTargetGoal())
                selector.add(2, NearestAttackablePlayerGoal())
            }
            "pillager" -> {
                selector.add(0, FloatGoal())
                selector.add(3, PathfindToRaidGoal(RAID_SPEED))
                selector.add(3, CrossbowAttackGoal(1.0, CROSSBOW_RANGE))
                selector.add(8, RandomStrollGoal(0.6))
                selector.add(9, LookAtPlayerGoal(LOOK_MEET, probability = 1.0f))
                selector.add(10, RandomLookAroundGoal())
                selector.add(1, HurtByTargetGoal())
                selector.add(2, NearestAttackablePlayerGoal())
                selector.add(3, NearestAttackableEntityGoal(setOf("villager", "iron_golem", "wandering_trader")))
            }
            "evoker" -> {
                selector.add(0, FloatGoal())
                selector.add(3, PathfindToRaidGoal(RAID_SPEED))
                // parity: `Evoker.registerGoals`, the fangs before the vexes and the wololo left out.
                selector.add(4, CastSpellGoal(CastSpellGoal.Spell.SUMMON_VEXES, SUMMON_CAST, SUMMON_COOLDOWN))
                selector.add(5, CastSpellGoal(CastSpellGoal.Spell.FANGS, FANG_CAST, FANG_COOLDOWN))
                selector.add(8, RandomStrollGoal(0.6))
                selector.add(9, LookAtPlayerGoal(LOOK_MEET, probability = 1.0f))
                selector.add(10, RandomLookAroundGoal())
                selector.add(1, HurtByTargetGoal())
                selector.add(2, NearestAttackablePlayerGoal())
                selector.add(3, NearestAttackableEntityGoal(setOf("villager", "iron_golem")))
            }
            "illusioner" -> {
                selector.add(0, FloatGoal())
                selector.add(3, PathfindToRaidGoal(RAID_SPEED))
                selector.add(4, CastSpellGoal(CastSpellGoal.Spell.BLINDNESS, BLIND_CAST, BLIND_COOLDOWN))
                selector.add(5, RangedAttackGoal(0.5, BOW_INTERVAL_MIN, BOW_INTERVAL_MAX, BOW_RANGE))
                selector.add(8, RandomStrollGoal(0.6))
                selector.add(9, LookAtPlayerGoal(LOOK_MEET, probability = 1.0f))
                selector.add(10, RandomLookAroundGoal())
                selector.add(1, HurtByTargetGoal())
                selector.add(2, NearestAttackablePlayerGoal())
                selector.add(3, NearestAttackableEntityGoal(setOf("villager", "iron_golem")))
            }
            "ghast" -> {
                selector.add(5, FlyingWanderGoal())
                selector.add(7, FireballAttackGoal())
                selector.add(1, NearestAttackablePlayerGoal())
            }
            "blaze" -> {
                selector.add(4, BlazeAttackGoal())
                selector.add(5, FlyingWanderGoal(BLAZE_RANGE, BLAZE_STEP, gated = false))
                selector.add(7, LookAtPlayerGoal(LOOK_FAR))
                selector.add(8, RandomLookAroundGoal())
                selector.add(1, HurtByTargetGoal())
                selector.add(2, NearestAttackablePlayerGoal())
            }
            "phantom" -> {
                selector.add(1, SwoopAttackGoal())
                selector.add(2, FlyingWanderGoal())
                selector.add(1, NearestAttackablePlayerGoal())
            }
            "vex" -> {
                selector.add(4, ChargeAttackGoal())
                selector.add(9, FlyingWanderGoal(VEX_RANGE, VEX_STEP, gated = false))
                selector.add(1, HurtByTargetGoal())
                selector.add(2, NearestAttackablePlayerGoal())
            }
            "guardian", "elder_guardian" -> {
                selector.add(4, BeamAttackGoal())
                selector.add(5, SwimAboutGoal())
                selector.add(7, LookAtPlayerGoal(LOOK_FAR))
                selector.add(8, RandomLookAroundGoal())
                selector.add(1, HurtByTargetGoal())
                selector.add(2, NearestAttackablePlayerGoal())
            }
            "bat", "allay" -> {
                selector.add(1, FlyingWanderGoal(BAT_RANGE, BAT_STEP, gated = false))
                selector.add(2, LookAtPlayerGoal(LOOK_FAR))
            }
            "villager", "wandering_trader" -> {
                // parity: a villager runs on a brain rather than on goals, so this is that brain's
                // schedule read by goals: the job site by day, the bell in the afternoon, the bed at
                // night, and a run from anything that raids the village.
                selector.add(0, FloatGoal())
                selector.add(1, AvoidEntityGoal(VILLAGER_THREATS, 8.0, 0.5, 0.7))
                selector.add(2, SleepInBedGoal(VILLAGER_SPEED))
                selector.add(3, WorkAtSiteGoal(VILLAGER_SPEED))
                selector.add(4, MeetAtBellGoal(VILLAGER_SPEED))
                selector.add(5, RandomStrollGoal(0.6))
                selector.add(6, LookAtPlayerGoal(LOOK_NEAR))
                selector.add(7, RandomLookAroundGoal())
            }
            // parity: `Squid.SquidRandomMovementGoal` and `AbstractFish.FishSwimGoal`, neither of
            // which walks a path: they push themselves about in the water.
            "squid", "glow_squid" -> selector.add(0, SwimAboutGoal())
            "cod", "salmon", "tropical_fish", "pufferfish", "tadpole" -> selector.add(4, SwimAboutGoal(FISH_PUSH))
            // parity: `AbstractCubeMob`: a cube turns and hops, and hops three times as often at
            // whatever it is after.
            "slime", "magma_cube", "sulfur_cube" -> {
                selector.add(2, HopAboutGoal())
                selector.add(1, NearestAttackablePlayerGoal())
                selector.add(3, HurtByTargetGoal())
            }
            // parity: a boss runs on its own phases rather than goals, and neither exists yet.
            "dolphin", "wither", "ender_dragon" -> return null
            else -> return null
        }
        return selector
    }

    /** parity: the goal set nearly every animal shares, with its own three speeds. */
    private fun farmAnimal(
        selector: GoalSelector,
        type: Identifier,
        panic: Double,
        tempt: Double,
        follow: Double,
    ) {
        selector.add(0, FloatGoal())
        selector.add(1, PanicGoal(panic))
        selector.add(2, BreedGoal(1.0))
        selector.add(3, TemptGoal(tempt, Breeding.foodFor(type)))
        selector.add(4, FollowParentGoal(follow))
        selector.add(5, RandomStrollGoal(1.0))
        selector.add(6, LookAtPlayerGoal(LOOK_NEAR))
        selector.add(7, RandomLookAroundGoal())
    }

    /** parity: `Monster` and the golem's `DefendVillageTargetGoal`: what an iron golem goes after. */
    private val HOSTILE: Set<String> =
        setOf(
            "zombie",
            "husk",
            "drowned",
            "zombie_villager",
            "skeleton",
            "stray",
            "bogged",
            "wither_skeleton",
            "creeper",
            "spider",
            "cave_spider",
            "silverfish",
            "endermite",
            "enderman",
            "witch",
            "pillager",
            "vindicator",
            "evoker",
            "ravager",
            "vex",
            "piglin_brute",
            "zoglin",
        )

    /** parity: `BegGoal`'s own list, which is whatever a wolf will eat. */
    private val WOLF_BEG_ITEMS: Set<String> =
        setOf("bone", "beef", "porkchop", "chicken", "mutton", "rabbit", "cooked_beef", "cooked_porkchop")

    /** parity: `AbstractFish`, which pushes itself along more gently than a squid. */
    private const val FISH_PUSH = 0.1

    /** parity: `AbstractSkeleton`'s own bow goal: a shot every twenty ticks inside fifteen blocks. */
    private const val BOW_INTERVAL_MIN = 20
    private const val BOW_INTERVAL_MAX = 20
    private const val BOW_RANGE = 15.0

    /** parity: `Pillager` and `Illusioner`, both of which carry their weapon eight blocks out. */
    private const val CROSSBOW_RANGE = 8.0

    /** parity: `EvokerSummonSpellGoal` and `EvokerAttackSpellGoal`, wind-up and wait apart. */
    private const val SUMMON_CAST = 100
    private const val SUMMON_COOLDOWN = 340
    private const val FANG_CAST = 40
    private const val FANG_COOLDOWN = 100
    private const val BLIND_CAST = 20
    private const val BLIND_COOLDOWN = 180

    /**
     * parity: how far each flier drifts and how hard it pushes itself along, which is not the same
     * for any two of them: the vex has its own move control, the rest fly on the 0.02 air input.
     */
    private const val BLAZE_RANGE = 8
    private const val BLAZE_STEP = 0.012
    private const val VEX_RANGE = 8
    private const val VEX_STEP = 0.03
    private const val BAT_RANGE = 7
    private const val BAT_STEP = 0.008

    /** parity: `PathfindToRaidGoal`, which walks a raider into the village at its own pace. */
    private const val RAID_SPEED = 1.0

    /** parity: the villager's own 0.5 speed modifier, and what it runs from. */
    private const val VILLAGER_SPEED = 0.5
    private val VILLAGER_THREATS: Set<String> =
        setOf(
            "zombie",
            "husk",
            "drowned",
            "zombie_villager",
            "zoglin",
            "vindicator",
            "evoker",
            "pillager",
            "illusioner",
            "ravager",
            "vex",
            "witch",
        )

    /** parity: the three blocks an illager stands off at to look you in the eye. */
    private const val LOOK_MEET = 3.0

    /** parity: the six blocks an animal looks at a player from, and the eight a monster does. */
    private const val LOOK_NEAR = 6.0
    private const val LOOK_FAR = 8.0
}
