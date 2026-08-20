package org.basaltmc.server.runtime.entity.ai

import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.runtime.entity.FlyingMovement
import org.basaltmc.server.runtime.world.WorldState
import kotlin.math.floor
import kotlin.math.sqrt

/**
 * parity: `Ghast.RandomFloatAroundGoal`: a point picked within sixteen blocks across and a few up
 * or down, floated to and then forgotten. It is the whole of a ghast's idle life.
 */
public class FlyingWanderGoal(
    private val range: Int = RANGE,
    private val step: Double = GHAST_IMPULSE,
    /** parity: `GhastMoveControl.floatDuration`, which pushes every few ticks rather than every one. */
    private val gated: Boolean = true,
) : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.MOVE)

    override val requiresUpdateEveryTick: Boolean = true

    private var wantedX = 0.0
    private var wantedY = 0.0
    private var wantedZ = 0.0
    private var arrived = true
    private var floatDuration = 0

    override fun canUse(mob: MobContext): Boolean = arrived || mob.random.nextInt(reducedTickDelay(INTERVAL)) == 0

    override fun canContinueToUse(mob: MobContext): Boolean = !arrived

    override fun start(mob: MobContext) {
        wantedX = mob.mob.x + (mob.random.nextDouble() * 2.0 - 1.0) * range
        wantedY = mob.mob.y + (mob.random.nextDouble() * 2.0 - 1.0) * (range / 2)
        wantedZ = mob.mob.z + (mob.random.nextDouble() * 2.0 - 1.0) * range
        arrived = false
    }

    override fun tick(mob: MobContext) {
        if (!isFree(mob.world, wantedX, wantedY, wantedZ)) {
            arrived = true
            return
        }
        if (gated && floatDuration-- > 0) return
        if (gated) floatDuration = GATE_MIN + mob.random.nextInt(GATE_SPREAD)
        if (!FlyingMovement.approach(mob.mob, wantedX, wantedY, wantedZ, step)) arrived = true
        LookControl.lookAt(mob.mob, wantedX, wantedY, wantedZ, maxYaw = LOOK_SPEED)
    }

    private fun isFree(
        world: WorldState,
        x: Double,
        y: Double,
        z: Double,
    ): Boolean =
        BlockShapes.isEmptyShape(
            world.getBlockState(floor(x).toInt(), floor(y).toInt(), floor(z).toInt()) ?: return false,
        )

    private companion object {
        const val RANGE = 16
        const val INTERVAL = 30

        /** parity: `GhastMoveControl`, a tenth of a block every two to six ticks. */
        const val GHAST_IMPULSE = 0.1
        const val GATE_MIN = 2
        const val GATE_SPREAD = 5
        const val LOOK_SPEED = 10f
    }
}

/**
 * parity: `Ghast.GhastShootFireballGoal`: twenty ticks of charging where it stands, a fireball
 * thrown from four blocks in front of its eyes, and two seconds before the next one.
 */
public class FireballAttackGoal : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.LOOK)

    override val requiresUpdateEveryTick: Boolean = true

    private var chargeTime = 0

    override fun canUse(mob: MobContext): Boolean = targetOf(mob) != null

    override fun stop(mob: MobContext) {
        chargeTime = 0
        mob.actions.setCharging(mob.mob, false)
    }

    override fun tick(mob: MobContext) {
        val target = targetOf(mob) ?: return
        val distance = mob.distanceSquaredTo(target.x, target.y, target.z)
        if (distance < RANGE_SQUARED) {
            LookControl.lookAt(mob.mob, target.x, target.y, target.z, maxYaw = LOOK_SPEED)
            chargeTime++
            if (chargeTime == CHARGE_TICKS) {
                mob.actions.shootFireball(mob.mob, target.x, target.y + target.height / 2.0, target.z, large = true)
                chargeTime = -COOLDOWN_TICKS
            }
        } else if (chargeTime > 0) {
            chargeTime--
        }
        mob.actions.setCharging(mob.mob, chargeTime > CHARGE_TELL)
    }

    private companion object {
        /** parity: sixty-four blocks, twenty ticks of charge, forty of rest, and the tell at ten. */
        const val RANGE_SQUARED = 4096.0
        const val CHARGE_TICKS = 20
        const val COOLDOWN_TICKS = 40
        const val CHARGE_TELL = 10
        const val LOOK_SPEED = 30f
    }
}

/**
 * parity: `Blaze.BlazeAttackGoal`: close in, punch what is next to it, and otherwise throw three
 * small fireballs six ticks apart with five seconds between bursts.
 */
public class BlazeAttackGoal : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.MOVE, GoalFlag.LOOK)

    override val requiresUpdateEveryTick: Boolean = true

    private var attackTime = 0
    private var attackStep = 0

    override fun canUse(mob: MobContext): Boolean = targetOf(mob) != null

    override fun stop(mob: MobContext) {
        attackStep = 0
        mob.actions.setCharging(mob.mob, false)
    }

    override fun tick(mob: MobContext) {
        attackTime--
        val target = targetOf(mob) ?: return
        val distance = mob.distanceSquaredTo(target.x, target.y, target.z)
        LookControl.lookAt(mob.mob, target.x, target.y, target.z, maxYaw = LOOK_SPEED)
        if (distance < MELEE_SQUARED) {
            if (attackTime <= 0) {
                attackTime = MELEE_INTERVAL
                mob.actions.swing(mob.mob)
                val damage =
                    org.basaltmc.server.runtime.entity.EntityCombat
                        .attackDamageOf(mob.mob)
                if (target.playerId != null) {
                    mob.actions.hurtPlayer(mob.mob, target.playerId, damage)
                } else {
                    target.entity?.let { mob.actions.hurtMob(mob.mob, it, damage) }
                }
            }
            FlyingMovement.approach(mob.mob, target.x, target.y, target.z, CHASE_SPEED)
            return
        }
        if (attackTime <= 0) {
            attackStep++
            when {
                attackStep == 1 -> {
                    attackTime = FIRST_DELAY
                    mob.actions.setCharging(mob.mob, true)
                }
                attackStep <= BURST -> attackTime = BURST_DELAY
                else -> {
                    attackTime = REST_DELAY
                    attackStep = 0
                    mob.actions.setCharging(mob.mob, false)
                }
            }
            if (attackStep > 1) {
                mob.actions.shootFireball(mob.mob, target.x, target.y + target.height / 2.0, target.z, large = false)
            }
        }
        // parity: it hovers a little above whatever it is shooting at rather than closing in.
        FlyingMovement.approach(mob.mob, target.x, target.y + HOVER, target.z, CHASE_SPEED)
    }

    private companion object {
        const val MELEE_SQUARED = 4.0
        const val MELEE_INTERVAL = 20
        const val FIRST_DELAY = 60
        const val BURST_DELAY = 6
        const val REST_DELAY = 100
        const val BURST = 4

        /** parity: `travelFlying`'s own 0.02 air input, times what the move control asked for. */
        const val CHASE_SPEED = 0.02
        const val HOVER = 2.0
        const val LOOK_SPEED = 30f
    }
}

/**
 * parity: `Phantom`'s circle and swoop: it climbs to a point above whatever it is after, then dives
 * through it and pulls up again.
 */
public class SwoopAttackGoal : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.MOVE, GoalFlag.LOOK)

    override val requiresUpdateEveryTick: Boolean = true

    private var diving = false

    override fun canUse(mob: MobContext): Boolean = targetOf(mob) != null

    override fun tick(mob: MobContext) {
        val target = targetOf(mob) ?: return
        LookControl.lookAt(mob.mob, target.x, target.y, target.z, maxYaw = LOOK_SPEED)
        val above = mob.mob.y - target.y
        if (!diving && above > CIRCLE_HEIGHT) diving = true
        if (diving && above < DIVE_FLOOR) diving = false
        val wantedY = if (diving) target.y else target.y + CIRCLE_HEIGHT + CIRCLE_MARGIN
        FlyingMovement.approach(mob.mob, target.x, wantedY, target.z, SWOOP_SPEED)
        // parity: the swoop hurts whatever it passes through.
        if (mob.distanceSquaredTo(target.x, target.y, target.z) > HIT_RANGE) return
        if (mob.mob.attackCooldown > 0) return
        mob.mob.attackCooldown = HIT_INTERVAL
        mob.actions.swing(mob.mob)
        val damage =
            org.basaltmc.server.runtime.entity.EntityCombat
                .attackDamageOf(mob.mob)
        if (target.playerId != null) {
            mob.actions.hurtPlayer(mob.mob, target.playerId, damage)
        } else {
            target.entity?.let { mob.actions.hurtMob(mob.mob, it, damage) }
        }
    }

    private companion object {
        const val CIRCLE_HEIGHT = 6.0
        const val CIRCLE_MARGIN = 2.0
        const val DIVE_FLOOR = 0.5

        /** parity: `Phantom.PhantomMoveControl`, which is the quickest of them. */
        const val SWOOP_SPEED = 0.05
        const val HIT_RANGE = 4.0
        const val HIT_INTERVAL = 20
        const val LOOK_SPEED = 30f
    }
}

/** parity: `Vex.VexChargeAttackGoal`: it picks a point past you and flies through it. */
public class ChargeAttackGoal : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.MOVE, GoalFlag.LOOK)

    override val requiresUpdateEveryTick: Boolean = true

    override fun canUse(mob: MobContext): Boolean = targetOf(mob) != null

    override fun tick(mob: MobContext) {
        val target = targetOf(mob) ?: return
        LookControl.lookAt(mob.mob, target.x, target.y, target.z, maxYaw = LOOK_SPEED)
        FlyingMovement.approach(mob.mob, target.x, target.y + target.height / 2.0, target.z, CHARGE_SPEED)
        if (mob.distanceSquaredTo(target.x, target.y, target.z) > HIT_RANGE) return
        if (mob.mob.attackCooldown > 0) return
        mob.mob.attackCooldown = HIT_INTERVAL
        mob.actions.swing(mob.mob)
        val damage =
            org.basaltmc.server.runtime.entity.EntityCombat
                .attackDamageOf(mob.mob)
        if (target.playerId != null) {
            mob.actions.hurtPlayer(mob.mob, target.playerId, damage)
        } else {
            target.entity?.let { mob.actions.hurtMob(mob.mob, it, damage) }
        }
    }

    private companion object {
        /** parity: `Vex.VexMoveControl`, a twentieth of a block a tick towards you. */
        const val CHARGE_SPEED = 0.05
        const val HIT_RANGE = 2.25
        const val HIT_INTERVAL = 20
        const val LOOK_SPEED = 30f
    }
}

/**
 * parity: `Guardian.GuardianAttackGoal`: the beam takes eighty ticks to charge, the guardian holds
 * still while it does, and what it hits takes the damage at the end of it.
 */
public class BeamAttackGoal : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.MOVE, GoalFlag.LOOK)

    override val requiresUpdateEveryTick: Boolean = true

    private var attackTime = 0

    override fun canUse(mob: MobContext): Boolean = targetOf(mob) != null

    override fun start(mob: MobContext) {
        attackTime = -CHARGE_TICKS
        mob.actions.setBeamTarget(mob.mob, mob.mob.targetId)
    }

    override fun stop(mob: MobContext) {
        mob.actions.setBeamTarget(mob.mob, null)
        attackTime = 0
    }

    override fun tick(mob: MobContext) {
        val target = targetOf(mob) ?: return
        LookControl.lookAt(mob.mob, target.x, target.y, target.z, maxYaw = LOOK_SPEED)
        attackTime++
        if (attackTime == 0) {
            mob.actions.setBeamTarget(mob.mob, target.playerId ?: target.entity?.entityId)
        } else if (attackTime >= CHARGE_TICKS) {
            val damage =
                org.basaltmc.server.runtime.entity.EntityCombat
                    .attackDamageOf(mob.mob) + BEAM_BONUS
            if (target.playerId != null) {
                mob.actions.hurtPlayer(mob.mob, target.playerId, damage)
            } else {
                target.entity?.let { mob.actions.hurtMob(mob.mob, it, damage) }
            }
            mob.mob.targetId = null
            attackTime = -CHARGE_TICKS
        }
        // parity: it keeps its distance while the beam charges.
        val distance = sqrt(mob.distanceSquaredTo(target.x, target.y, target.z))
        if (distance > KEEP_RANGE) {
            FlyingMovement.approach(mob.mob, target.x, target.y, target.z, SWIM_SPEED)
        }
    }

    private companion object {
        /** parity: `Guardian`'s own eighty ticks of beam and the two points it adds. */
        const val CHARGE_TICKS = 80
        const val BEAM_BONUS = 2f
        const val KEEP_RANGE = 12.0
        const val SWIM_SPEED = 0.03
        const val LOOK_SPEED = 30f
    }
}
