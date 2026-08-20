package org.basaltmc.server.runtime.entity

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.registry.EntityAttributes
import org.basaltmc.server.registry.EntityTypeShapes
import org.basaltmc.server.runtime.world.WorldState
import org.basaltmc.wire.common.Identifier
import kotlin.math.abs
import kotlin.math.floor
import kotlin.math.sqrt

/**
 * The mobs that come for you: who they pick, how they get there, and when they swing.
 *
 * parity: `NearestAttackableTargetGoal` for the picking and `MeleeAttackGoal` for the rest. A mob
 * only walks the path its navigation found, and only hits when its own box, widened by its reach,
 * touches the target's.
 */
public object MobCombat {
    /** parity: `Mob.DEFAULT_ATTACK_REACH`, `sqrt(2.04) - 0.6`. */
    public val ATTACK_REACH: Double = sqrt(2.04) - 0.6

    /** parity: `MeleeAttackGoal.attackInterval`, a swing a second. */
    public const val ATTACK_INTERVAL: Int = 20

    /** parity: `NearestAttackableTargetGoal`'s own one-in-ten roll before it even looks. */
    public const val TARGET_INTERVAL: Int = 10

    /** parity: `MeleeAttackGoal.tick`, which asks its navigation again every 4 to 10 ticks. */
    private const val REPATH_MIN = 4
    private const val REPATH_SPREAD = 7

    /** parity: `Spider.SpiderTargetGoal`, which needs the light below a half to notice anybody. */
    private const val SPIDER_LIGHT = 8

    /**
     * The mobs this build sends after a player.
     *
     * parity: the classes whose target selector holds a `NearestAttackableTargetGoal` on the player
     * and whose goal selector holds a melee attack. The ranged ones (skeletons, blazes, ghasts), the
     * ones that only fight back when hit (piglins, wolves, endermen) and the creeper, which does not
     * swing at all, are deliberately not here.
     */
    private val MELEE: Set<String> =
        setOf(
            "zombie",
            "husk",
            "drowned",
            "zombie_villager",
            "giant",
            "spider",
            "cave_spider",
            "silverfish",
            "endermite",
            "vindicator",
            "ravager",
            "vex",
            "wither_skeleton",
            "piglin_brute",
            "hoglin",
            "zoglin",
            "warden",
        )

    /** Whether this kind of mob hunts players by walking up and hitting them. */
    public fun isMelee(type: Identifier): Boolean = type.path in MELEE

    /** parity: `Spider.SpiderTargetGoal`: a spider in the light leaves you alone. */
    public fun needsDarkness(type: Identifier): Boolean = type.path == "spider" || type.path == "cave_spider"

    /** Whoever a mob can go after: a player, reduced to what the goals actually read. */
    public class Target(
        public val entityId: Int,
        public val x: Double,
        public val y: Double,
        public val z: Double,
        public val width: Double,
        public val height: Double,
    )

    /**
     * The nearest target within the mob's own follow range, or null.
     *
     * parity: `NearestAttackableTargetGoal.canUse`, roll included: it does not even look nine ticks
     * out of ten, which is what staggers a room full of zombies rather than turning them at once.
     */
    public fun acquire(
        mob: ServerEntity,
        targets: List<Target>,
        random: BasaltRandom,
        light: Int,
    ): Target? {
        if (random.nextInt(TARGET_INTERVAL) != 0) return null
        if (needsDarkness(mob.type) && light >= SPIDER_LIGHT) return null
        val range = EntityAttributes.baseValue(mob.type, EntityAttributes.FOLLOW_RANGE) ?: return null
        var best: Target? = null
        var bestDistance = range * range
        for (target in targets) {
            val dx = target.x - mob.x
            val dy = target.y - mob.y
            val dz = target.z - mob.z
            val distance = dx * dx + dy * dy + dz * dz
            if (distance > bestDistance) continue
            bestDistance = distance
            best = target
        }
        return best
    }

    /**
     * One tick of chasing [target]. Answers true when the mob swung, which is the caller's cue to
     * hurt whoever it swung at.
     *
     * parity: `MeleeAttackGoal.tick`: look, walk the path, count the cooldown down, and hit when
     * both the cooldown and the reach allow it.
     */
    public fun tick(
        world: WorldState,
        mob: ServerEntity,
        target: Target,
        random: BasaltRandom,
        speed: Double,
    ): Boolean {
        mob.repathCooldown--
        if (mob.repathCooldown <= 0) {
            mob.repathCooldown = REPATH_MIN + random.nextInt(REPATH_SPREAD)
            MobBrain.pathTo(world, mob, target.x, target.y, target.z)
        }
        MobBrain.walk(world, mob, speed)
        if (mob.attackCooldown > 0) return false
        if (!withinReach(mob, target)) return false
        mob.attackCooldown = ATTACK_INTERVAL
        return true
    }

    /**
     * parity: `Mob.isWithinMeleeAttackRange`: the mob's own box widened by its reach has to touch
     * the target's box, which is why a tall player is hit from further below than a short one.
     */
    public fun withinReach(
        mob: ServerEntity,
        target: Target,
    ): Boolean {
        val reach = EntityTypeShapes.widthOf(mob.type) / 2.0 + ATTACK_REACH + target.width / 2.0
        if (abs(target.x - mob.x) > reach || abs(target.z - mob.z) > reach) return false
        val mobTop = mob.y + EntityTypeShapes.heightOf(mob.type)
        return target.y <= mobTop && target.y + target.height >= mob.y
    }

    /** Where the mob is standing, for the light check the spiders need. */
    public fun blockPosition(mob: ServerEntity): IntArray =
        intArrayOf(floor(mob.x).toInt(), floor(mob.y).toInt(), floor(mob.z).toInt())
}
