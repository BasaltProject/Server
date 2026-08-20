package org.basaltmc.server.runtime.entity.ai

import org.basaltmc.server.registry.EntityAttributes
import org.basaltmc.server.registry.EntityTypeShapes
import org.basaltmc.server.runtime.entity.EntityCombat
import org.basaltmc.server.runtime.entity.MobCombat
import kotlin.math.sqrt

/**
 * parity: `NearestAttackableTargetGoal`: one roll in ten before it even looks, the nearest thing
 * inside the mob's own follow range, and it keeps it until it dies or gets away.
 */
public class NearestAttackablePlayerGoal(
    private val randomInterval: Int = DEFAULT_INTERVAL,
    /** parity: `Spider.SpiderTargetGoal`, which only notices anybody in the dark. */
    private val needsDarkness: Boolean = false,
) : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.TARGET)

    override fun canUse(mob: MobContext): Boolean {
        if (mob.mob.targetId != null) return false
        if (randomInterval > 0 && mob.random.nextInt(reducedTickDelay(randomInterval)) != 0) return false
        if (needsDarkness && mob.light >= DARKNESS) return false
        return nearest(mob) != null
    }

    override fun canContinueToUse(mob: MobContext): Boolean {
        val target = mob.mob.targetId?.let { mob.playerById(it) } ?: return false
        if (!target.targetable) return false
        return mob.distanceSquaredTo(target.x, target.y, target.z) <= followRange(mob) * followRange(mob)
    }

    override fun start(mob: MobContext) {
        mob.mob.targetId = nearest(mob)?.entityId
    }

    override fun stop(mob: MobContext) {
        mob.mob.targetId = null
    }

    private fun nearest(mob: MobContext): PlayerView? = mob.nearestPlayer(followRange(mob))

    private fun followRange(mob: MobContext): Double =
        EntityAttributes.baseValue(mob.mob.type, EntityAttributes.FOLLOW_RANGE) ?: DEFAULT_FOLLOW_RANGE

    private companion object {
        const val DEFAULT_INTERVAL = 10
        const val DEFAULT_FOLLOW_RANGE = 16.0

        /** parity: `getLightLevelDependentMagicValue() >= 0.5`, which is light eight and up. */
        const val DARKNESS = 8
    }
}

/**
 * parity: `NearestAttackableTargetGoal` again, for the kinds of mob that go after other mobs: a
 * zombie after a villager, an iron golem after whatever is hostile.
 */
public class NearestAttackableEntityGoal(
    private val targets: Set<String>,
    private val randomInterval: Int = DEFAULT_INTERVAL,
) : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.TARGET)

    override fun canUse(mob: MobContext): Boolean {
        if (mob.mob.targetId != null) return false
        if (randomInterval > 0 && mob.random.nextInt(reducedTickDelay(randomInterval)) != 0) return false
        return nearest(mob) != null
    }

    override fun canContinueToUse(mob: MobContext): Boolean {
        val target = mob.mob.targetId?.let { mob.entityById(it) } ?: return false
        if (target.removed || target.health <= 0.0) return false
        return mob.distanceSquaredTo(target.x, target.y, target.z) <= followRange(mob) * followRange(mob)
    }

    override fun start(mob: MobContext) {
        mob.mob.targetId = nearest(mob)?.entityId
    }

    override fun stop(mob: MobContext) {
        mob.mob.targetId = null
    }

    private fun nearest(mob: MobContext): org.basaltmc.server.runtime.entity.ServerEntity? {
        val range = followRange(mob)
        return mob.entities
            .filter { it !== mob.mob && !it.removed && it.type.path in targets && it.health > 0.0 }
            .filter { mob.distanceSquaredTo(it.x, it.y, it.z) <= range * range }
            .minByOrNull { mob.distanceSquaredTo(it.x, it.y, it.z) }
    }

    private fun followRange(mob: MobContext): Double =
        EntityAttributes.baseValue(mob.mob.type, EntityAttributes.FOLLOW_RANGE) ?: DEFAULT_FOLLOW_RANGE

    private companion object {
        const val DEFAULT_INTERVAL = 10
        const val DEFAULT_FOLLOW_RANGE = 16.0
    }
}

/**
 * parity: `HurtByTargetGoal`: whoever hit it becomes what it is after, and it stays after them for
 * as long as they are in range.
 */
public class HurtByTargetGoal : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.TARGET)

    override fun canUse(mob: MobContext): Boolean = mob.mob.hurtBy != null && mob.mob.targetId == null

    override fun canContinueToUse(mob: MobContext): Boolean {
        val target = mob.mob.targetId ?: return false
        mob.playerById(target)?.let { return it.targetable }
        return mob.entityById(target)?.removed == false
    }

    override fun start(mob: MobContext) {
        mob.mob.targetId = mob.mob.hurtBy
        mob.mob.hurtBy = null
    }

    override fun stop(mob: MobContext) {
        mob.mob.targetId = null
    }
}

/**
 * parity: `MeleeAttackGoal`: walk the target down, repath every four to ten ticks, and swing once a
 * second when both boxes touch.
 */
public class MeleeAttackGoal(
    private val speedModifier: Double,
    private val followingTargetEvenIfNotSeen: Boolean = false,
) : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.MOVE, GoalFlag.LOOK)

    override val requiresUpdateEveryTick: Boolean = true

    private var repathCooldown = 0

    // parity: `MeleeAttackGoal.canUse`, which asks its navigation for a path first and gives up
    // when there is none and the target is out of reach: that is what leaves a mob free to wander
    // instead of standing still facing somebody it cannot get to.
    override fun canUse(mob: MobContext): Boolean {
        val target = targetOf(mob) ?: return false
        if (withinReach(mob, target)) return true
        return Navigation.canReach(mob.world, mob.mob, target.x, target.y, target.z)
    }

    override fun canContinueToUse(mob: MobContext): Boolean {
        val target = targetOf(mob) ?: return false
        return withinReach(mob, target) || !Navigation.isDone(mob.mob)
    }

    override fun start(mob: MobContext) {
        repathCooldown = 0
    }

    override fun stop(mob: MobContext) {
        Navigation.stop(mob.mob)
    }

    override fun tick(mob: MobContext) {
        val target = targetOf(mob) ?: return
        LookControl.lookAt(mob.mob, target.x, target.y + target.height * EYE_SHARE, target.z, maxYaw = LOOK_SPEED)
        repathCooldown = maxOf(repathCooldown - 1, 0)
        if (repathCooldown <= 0) {
            repathCooldown = REPATH_MIN + mob.random.nextInt(REPATH_SPREAD)
            val distance = mob.distanceSquaredTo(target.x, target.y, target.z)
            if (distance > FAR_SQUARED) {
                repathCooldown += FAR_PENALTY
            } else if (distance > NEAR_SQUARED) {
                repathCooldown += NEAR_PENALTY
            }
            if (!Navigation.moveTo(mob.world, mob.mob, target.x, target.y, target.z, speedModifier)) {
                repathCooldown += NO_PATH_PENALTY
            }
        }
        if (mob.mob.attackCooldown > 0) return
        if (!withinReach(mob, target)) return
        mob.mob.attackCooldown = ATTACK_INTERVAL
        mob.actions.swing(mob.mob)
        val damage = EntityCombat.attackDamageOf(mob.mob)
        if (target.playerId != null) {
            mob.actions.hurtPlayer(mob.mob, target.playerId, damage)
        } else {
            target.entity?.let { mob.actions.hurtMob(mob.mob, it, damage) }
        }
    }

    private fun withinReach(
        mob: MobContext,
        target: TargetView,
    ): Boolean {
        val reach = EntityTypeShapes.widthOf(mob.mob.type) / 2.0 + MobCombat.ATTACK_REACH + target.width / 2.0
        val dx = target.x - mob.mob.x
        val dz = target.z - mob.mob.z
        if (sqrt(dx * dx) > reach || sqrt(dz * dz) > reach) return false
        return target.y <= mob.mob.y + EntityTypeShapes.heightOf(mob.mob.type) && target.y + target.height >= mob.mob.y
    }

    private companion object {
        const val REPATH_MIN = 4
        const val REPATH_SPREAD = 7
        const val FAR_SQUARED = 1024.0
        const val NEAR_SQUARED = 256.0
        const val FAR_PENALTY = 10
        const val NEAR_PENALTY = 5
        const val NO_PATH_PENALTY = 15
        const val ATTACK_INTERVAL = 20
        const val EYE_SHARE = 0.85
        const val LOOK_SPEED = 30f
    }
}

/** Whatever a mob is after, whether that is a player or another mob. */
public class TargetView(
    public val playerId: Int?,
    public val entity: org.basaltmc.server.runtime.entity.ServerEntity?,
    public val x: Double,
    public val y: Double,
    public val z: Double,
    public val width: Double,
    public val height: Double,
)

/** The thing this mob is after, resolved against what is in the world this tick. */
public fun targetOf(mob: MobContext): TargetView? {
    val id = mob.mob.targetId ?: return null
    mob.playerById(id)?.let {
        if (!it.targetable) return null
        return TargetView(it.entityId, null, it.x, it.y, it.z, it.width, it.height)
    }
    val entity = mob.entityById(id) ?: return null
    if (entity.removed) return null
    return TargetView(null, entity, entity.x, entity.y, entity.z, entity.width, entity.height)
}

/**
 * What a warden goes after: whoever it is angriest at, and nobody else.
 *
 * parity: `WardenAi`'s anger, not a target goal at all in vanilla. The point is the same either
 * way: a warden that has not heard you does not know you are there, however close you stand.
 */
public class AngerTargetGoal : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.TARGET)

    override fun canUse(mob: MobContext): Boolean = suspect(mob) != null

    override fun canContinueToUse(mob: MobContext): Boolean = suspect(mob) != null

    override fun start(mob: MobContext) {
        mob.mob.targetId = suspect(mob)
    }

    override fun stop(mob: MobContext) {
        mob.mob.targetId = null
    }

    /** Whoever it is angriest at can change while it is already after somebody. */
    override fun tick(mob: MobContext) {
        mob.mob.targetId = suspect(mob)
    }

    private fun suspect(mob: MobContext): Int? {
        val angriest =
            org.basaltmc.server.runtime.entity.Wardens
                .target(mob.mob) ?: return null
        // It has to still be here and still be worth hitting: a creative or spectating player is
        // neither, and a suspect who logged out or died is nobody to walk towards.
        mob.playerById(angriest)?.let { return if (it.targetable) angriest else null }
        return angriest.takeIf { mob.entityById(it) != null }
    }
}
