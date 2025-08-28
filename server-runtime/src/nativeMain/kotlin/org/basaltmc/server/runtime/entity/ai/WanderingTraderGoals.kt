package org.basaltmc.server.runtime.entity.ai

import org.basaltmc.server.runtime.entity.MobBrain
import kotlin.math.sqrt

/** WanderingTrader.WanderToPositionGoal, driven by the meeting point chosen by its custom spawner. */
public class WanderingTraderWanderGoal(
    private val stopDistance: Double = 2.0,
    private val speedModifier: Double = 0.35,
) : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.MOVE)

    override val requiresUpdateEveryTick: Boolean = true

    override fun canUse(mob: MobContext): Boolean =
        mob.mob.wanderTarget?.let { isTooFarAway(mob, it, stopDistance) } == true

    override fun canContinueToUse(mob: MobContext): Boolean =
        mob.mob.wanderTarget?.let { isTooFarAway(mob, it, stopDistance) } == true

    override fun stop(mob: MobContext) {
        mob.mob.wanderTarget = null
        Navigation.stop(mob.mob)
    }

    override fun tick(mob: MobContext) {
        val target = mob.mob.wanderTarget ?: return
        if (!Navigation.isDone(mob.mob)) return
        val dx = target[0] - mob.mob.x
        val dy = target[1] - mob.mob.y
        val dz = target[2] - mob.mob.z
        val distance = sqrt(dx * dx + dy * dy + dz * dz)
        if (distance > DIRECT_PATH_DISTANCE) {
            Navigation.moveTo(
                mob.world,
                mob.mob,
                mob.mob.x + dx / distance * DIRECT_PATH_DISTANCE,
                mob.mob.y + dy / distance * DIRECT_PATH_DISTANCE,
                mob.mob.z + dz / distance * DIRECT_PATH_DISTANCE,
                speedModifier,
            )
        } else {
            Navigation.moveTo(
                mob.world,
                mob.mob,
                target[0].toDouble(),
                target[1].toDouble(),
                target[2].toDouble(),
                speedModifier,
            )
        }
    }

    private fun isTooFarAway(
        mob: MobContext,
        target: IntArray,
        distance: Double,
    ): Boolean {
        val dx = mob.mob.x - (target[0] + 0.5)
        val dy = mob.mob.y - (target[1] + 0.5)
        val dz = mob.mob.z - (target[2] + 0.5)
        return dx * dx + dy * dy + dz * dz >= distance * distance
    }

    private companion object {
        const val DIRECT_PATH_DISTANCE = 10.0
    }
}

/** MoveTowardsRestrictionGoal for the transient 16-block home installed by the spawner. */
public class MoveTowardsTraderHomeGoal(
    private val speedModifier: Double = 0.35,
) : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.MOVE)

    private var wanted: DoubleArray? = null

    override fun canUse(mob: MobContext): Boolean {
        val home = mob.mob.homePosition ?: return false
        if (mob.mob.homeRadius <= 0) return false
        val dx = mob.mob.x - (home[0] + 0.5)
        val dy = mob.mob.y - home[1]
        val dz = mob.mob.z - (home[2] + 0.5)
        if (dx * dx + dy * dy + dz * dz <= mob.mob.homeRadius * mob.mob.homeRadius) return false

        repeat(CANDIDATE_PASSES) {
            val candidate =
                MobBrain.strollTarget(
                    mob.world,
                    mob.mob,
                    mob.random,
                    HOME_STEP,
                    HOME_HEIGHT,
                ) ?: return@repeat
            val towardX = home[0] + 0.5 - mob.mob.x
            val towardZ = home[2] + 0.5 - mob.mob.z
            if ((candidate[0] - mob.mob.x) * towardX + (candidate[2] - mob.mob.z) * towardZ > 0.0) {
                wanted = candidate
                return true
            }
        }
        return false
    }

    override fun canContinueToUse(mob: MobContext): Boolean = !Navigation.isDone(mob.mob)

    override fun start(mob: MobContext) {
        val target = wanted ?: return
        Navigation.moveTo(mob.world, mob.mob, target[0], target[1], target[2], speedModifier)
    }

    override fun stop(mob: MobContext) {
        wanted = null
        Navigation.stop(mob.mob)
    }

    private companion object {
        const val HOME_STEP = 16
        const val HOME_HEIGHT = 7
        const val CANDIDATE_PASSES = 4
    }
}
