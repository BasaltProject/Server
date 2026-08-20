package org.basaltmc.server.runtime.entity.ai

import kotlin.math.sqrt

/**
 * parity: `RangedCrossbowAttackGoal`: close to inside its range, wind the crossbow for a second and
 * a quarter, let it go, and wait before the next one.
 */
public class CrossbowAttackGoal(
    private val speedModifier: Double,
    private val attackRadius: Double,
) : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.MOVE, GoalFlag.LOOK)

    override val requiresUpdateEveryTick: Boolean = true

    private var charge = 0
    private var attackDelay = 0
    private var seeTime = 0

    override fun canUse(mob: MobContext): Boolean = targetOf(mob) != null

    override fun canContinueToUse(mob: MobContext): Boolean = targetOf(mob) != null

    override fun stop(mob: MobContext) {
        charge = 0
        seeTime = 0
        mob.actions.setCharging(mob.mob, false)
        Navigation.stop(mob.mob)
    }

    override fun tick(mob: MobContext) {
        val target = targetOf(mob) ?: return
        val distanceSquared = mob.distanceSquaredTo(target.x, target.y, target.z)
        seeTime++
        LookControl.lookAt(mob.mob, target.x, target.y + target.height * EYE_SHARE, target.z, maxYaw = LOOK_SPEED)
        val needsToMove = distanceSquared > attackRadius * attackRadius || seeTime < SEE_TIME
        if (needsToMove && attackDelay == 0) {
            Navigation.moveTo(mob.world, mob.mob, target.x, target.y, target.z, speedModifier)
        } else {
            Navigation.stop(mob.mob)
        }
        if (attackDelay > 0) {
            attackDelay--
            return
        }
        // parity: `CrossbowItem.getChargeDuration`, twenty-five ticks of winding.
        charge++
        mob.actions.setCharging(mob.mob, true)
        if (charge < CHARGE_TICKS) return
        charge = 0
        mob.actions.setCharging(mob.mob, false)
        mob.actions.shootArrow(mob.mob, target.x, target.y + target.height * AIM_SHARE, target.z, ARROW_POWER)
        attackDelay = MIN_DELAY + mob.random.nextInt(DELAY_SPREAD)
    }

    private companion object {
        const val CHARGE_TICKS = 25
        const val MIN_DELAY = 20
        const val DELAY_SPREAD = 20
        const val SEE_TIME = 5
        const val EYE_SHARE = 0.85
        const val AIM_SHARE = 0.5
        const val ARROW_POWER = 1.0f
        const val LOOK_SPEED = 30f
    }
}

/**
 * parity: `Witch.performRangedAttack`: which potion it throws depends on how far away you are and
 * what you already have on you, and it drinks one itself when it is hurt.
 */
public class ThrowPotionGoal : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.MOVE, GoalFlag.LOOK)

    override val requiresUpdateEveryTick: Boolean = true

    private var attackDelay = 0

    override fun canUse(mob: MobContext): Boolean = targetOf(mob) != null

    override fun canContinueToUse(mob: MobContext): Boolean = targetOf(mob) != null

    override fun stop(mob: MobContext) {
        Navigation.stop(mob.mob)
    }

    override fun tick(mob: MobContext) {
        val target = targetOf(mob) ?: return
        LookControl.lookAt(mob.mob, target.x, target.y + target.height * EYE_SHARE, target.z, maxYaw = LOOK_SPEED)
        val distance = sqrt(mob.distanceSquaredTo(target.x, target.y, target.z))
        if (distance > THROW_RANGE) {
            Navigation.moveTo(mob.world, mob.mob, target.x, target.y, target.z, WALK_SPEED)
        } else {
            Navigation.stop(mob.mob)
        }
        if (attackDelay > 0) {
            attackDelay--
            return
        }
        if (distance > THROW_RANGE) return
        // parity: slowness past eight blocks, poison against anybody healthy, weakness up close.
        val potion =
            when {
                distance >= SLOWNESS_RANGE -> "slowness"
                mob.random.nextFloat() < WEAKNESS_CHANCE && distance <= WEAKNESS_RANGE -> "weakness"
                else -> "poison"
            }
        mob.actions.throwPotion(mob.mob, target.x, target.y, target.z, potion)
        attackDelay = THROW_INTERVAL
    }

    private companion object {
        const val THROW_RANGE = 10.0
        const val SLOWNESS_RANGE = 8.0
        const val WEAKNESS_RANGE = 3.0
        const val WEAKNESS_CHANCE = 0.25f
        const val THROW_INTERVAL = 60
        const val WALK_SPEED = 1.0
        const val EYE_SHARE = 0.85
        const val LOOK_SPEED = 30f
    }
}

/**
 * parity: `SpellcasterIllager`'s own rhythm: a spell is cast after a wind-up, and then nothing for
 * a while. The evoker's fangs and vexes, and the illusioner's blindness, all hang off this.
 */
public class CastSpellGoal(
    private val spell: Spell,
    private val castingTime: Int,
    private val cooldown: Int,
) : Goal() {
    /** Which spell this goal casts. */
    public enum class Spell {
        FANGS,
        SUMMON_VEXES,
        BLINDNESS,
    }

    override val flags: Set<GoalFlag> = setOf(GoalFlag.MOVE, GoalFlag.LOOK)

    override val requiresUpdateEveryTick: Boolean = true

    private var casting = 0
    private var nextCast = 0

    override fun canUse(mob: MobContext): Boolean {
        if (targetOf(mob) == null) return false
        if (nextCast > 0) {
            nextCast--
            return false
        }
        return true
    }

    override fun canContinueToUse(mob: MobContext): Boolean = targetOf(mob) != null && casting < castingTime

    override fun start(mob: MobContext) {
        casting = 0
        mob.actions.setCasting(mob.mob, true)
        Navigation.stop(mob.mob)
    }

    override fun stop(mob: MobContext) {
        mob.actions.setCasting(mob.mob, false)
        nextCast = cooldown
    }

    override fun tick(mob: MobContext) {
        val target = targetOf(mob) ?: return
        LookControl.lookAt(mob.mob, target.x, target.y, target.z, maxYaw = LOOK_SPEED)
        casting++
        if (casting < castingTime) return
        when (spell) {
            Spell.FANGS -> mob.actions.castFangs(mob.mob, target.x, target.y, target.z)
            Spell.SUMMON_VEXES -> mob.actions.summonVexes(mob.mob, VEX_COUNT)
            Spell.BLINDNESS ->
                target.playerId?.let {
                    mob.actions.applyEffect(it, "blindness", BLINDNESS_TICKS, amplifier = 0)
                }
        }
    }

    private companion object {
        /** parity: `EvokerSummonSpellGoal`, three vexes a cast, and the blindness an illusioner gives. */
        const val VEX_COUNT = 3
        const val BLINDNESS_TICKS = 400
        const val LOOK_SPEED = 30f
    }
}

/**
 * parity: `PathfindToRaidGoal`: a raider with a raid to be at walks to the village and only stops
 * when it is inside it or has somebody to hit.
 */
public class PathfindToRaidGoal(
    private val speed: Double,
) : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.MOVE)

    override val requiresUpdateEveryTick: Boolean = true

    override fun canUse(mob: MobContext): Boolean {
        val center = mob.mob.raidCenter ?: return false
        if (targetOf(mob) != null) return false
        return mob.distanceSquaredTo(center[0] + 0.5, center[1].toDouble(), center[2] + 0.5) > ARRIVED * ARRIVED
    }

    override fun canContinueToUse(mob: MobContext): Boolean = canUse(mob)

    override fun start(mob: MobContext) {
        moveToCentre(mob)
    }

    override fun stop(mob: MobContext) {
        Navigation.stop(mob.mob)
    }

    override fun tick(mob: MobContext) {
        if (!Navigation.isDone(mob.mob)) return
        if (mob.mob.repathCooldown > 0) {
            mob.mob.repathCooldown--
            return
        }
        mob.mob.repathCooldown = REPATH_INTERVAL
        moveToCentre(mob)
    }

    private fun moveToCentre(mob: MobContext) {
        val centre = mob.mob.raidCenter ?: return
        // parity: it heads for the village rather than for its exact middle, which is often a wall.
        val toX = centre[0] + mob.random.nextInt(2 * SPREAD + 1) - SPREAD
        val toZ = centre[2] + mob.random.nextInt(2 * SPREAD + 1) - SPREAD
        Navigation.moveTo(mob.world, mob.mob, toX + 0.5, centre[1].toDouble(), toZ + 0.5, speed)
    }

    private companion object {
        const val ARRIVED = 12.0
        const val REPATH_INTERVAL = 40
        const val SPREAD = 6
    }
}
