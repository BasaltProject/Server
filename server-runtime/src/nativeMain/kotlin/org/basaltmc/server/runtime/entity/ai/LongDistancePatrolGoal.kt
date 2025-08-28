package org.basaltmc.server.runtime.entity.ai

import org.basaltmc.server.runtime.entity.Aabb
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.server.world.Chunk
import kotlin.math.floor
import kotlin.math.sqrt

/** Minecraft 26.2 `PatrollingMonster.LongDistancePatrolGoal`. */
public class LongDistancePatrolGoal : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.MOVE)

    override fun canUse(mob: MobContext): Boolean =
        mob.mob.patrolling &&
            targetOf(mob) == null &&
            mob.mob.passengerIds.isEmpty() &&
            mob.mob.patrolTarget != null &&
            mob.gameTime >= mob.mob.patrolCooldownUntil

    override fun canContinueToUse(mob: MobContext): Boolean = canUse(mob)

    override fun tick(mob: MobContext) {
        if (!Navigation.isDone(mob.mob)) return
        val companions = companions(mob)
        if (mob.mob.patrolling && companions.isEmpty()) {
            mob.mob.patrolling = false
            return
        }
        val target = mob.mob.patrolTarget ?: return
        if (
            mob.mob.patrolLeader &&
            mob.distanceSquaredTo(
                target[0] + 0.5,
                target[1].toDouble(),
                target[2] + 0.5,
            ) < ARRIVED_SQUARED
        ) {
            findPatrolTarget(mob.mob, mob)
            return
        }

        val pathTarget = intermediateTarget(mob, target) ?: return
        val speed = if (mob.mob.patrolLeader) LEADER_SPEED else SPEED
        if (
            !Navigation.moveTo(
                mob.world,
                mob.mob,
                pathTarget[0].toDouble(),
                pathTarget[1].toDouble(),
                pathTarget[2].toDouble(),
                speed,
            )
        ) {
            moveRandomly(mob)
            mob.mob.patrolCooldownUntil = mob.gameTime + FAILED_PATH_COOLDOWN
        } else if (mob.mob.patrolLeader) {
            for (companion in companions) setPatrolTarget(companion, pathTarget)
        }
    }

    private fun companions(mob: MobContext): List<ServerEntity> {
        val search = Aabb.of(mob.mob.x, mob.mob.y, mob.mob.z, mob.mob.width, mob.mob.height).inflate(COMPANION_RANGE)
        return mob.entities.filter { candidate ->
            candidate !== mob.mob &&
                !candidate.removed &&
                candidate.type.path in RAIDER_TYPES &&
                candidate.raidCenter == null &&
                search.intersects(Aabb.of(candidate.x, candidate.y, candidate.z, candidate.width, candidate.height))
        }
    }

    private fun intermediateTarget(
        mob: MobContext,
        target: IntArray,
    ): IntArray? {
        val targetX = target[0] + 0.5
        val targetY = target[1].toDouble()
        val targetZ = target[2] + 0.5
        val distanceX = mob.mob.x - targetX
        val distanceY = mob.mob.y - targetY
        val distanceZ = mob.mob.z - targetZ
        val sideX = targetX + distanceZ * SIDE_SHARE
        val sideY = targetY + distanceY * SIDE_SHARE
        val sideZ = targetZ - distanceX * SIDE_SHARE
        val stepX = sideX - mob.mob.x
        val stepY = sideY - mob.mob.y
        val stepZ = sideZ - mob.mob.z
        val length = sqrt(stepX * stepX + stepY * stepY + stepZ * stepZ)
        if (length == 0.0) return null
        val x = floor(mob.mob.x + stepX / length * STEP_DISTANCE).toInt()
        val z = floor(mob.mob.z + stepZ / length * STEP_DISTANCE).toInt()
        val y = surfaceY(mob, x, z) ?: return null
        return intArrayOf(x, y, z)
    }

    private fun moveRandomly(mob: MobContext): Boolean {
        val x = floor(mob.mob.x).toInt() - RANDOM_RADIUS + mob.random.nextInt(RANDOM_DIAMETER)
        val z = floor(mob.mob.z).toInt() - RANDOM_RADIUS + mob.random.nextInt(RANDOM_DIAMETER)
        val y = surfaceY(mob, x, z) ?: return false
        return Navigation.moveTo(mob.world, mob.mob, x.toDouble(), y.toDouble(), z.toDouble(), SPEED)
    }

    private fun surfaceY(
        mob: MobContext,
        x: Int,
        z: Int,
    ): Int? =
        mob.world
            .chunkIfLoaded(x shr 4, z shr 4)
            ?.heightAt(x and 15, z and 15, Chunk.HeightmapType.MOTION_BLOCKING_NO_LEAVES)

    private fun findPatrolTarget(
        entity: ServerEntity,
        mob: MobContext,
    ) {
        entity.patrolTarget =
            intArrayOf(
                floor(entity.x).toInt() - TARGET_RADIUS + mob.random.nextInt(TARGET_DIAMETER),
                floor(entity.y).toInt(),
                floor(entity.z).toInt() - TARGET_RADIUS + mob.random.nextInt(TARGET_DIAMETER),
            )
        entity.patrolling = true
    }

    private fun setPatrolTarget(
        entity: ServerEntity,
        target: IntArray,
    ) {
        entity.patrolTarget = target.copyOf()
        entity.patrolling = true
    }

    private companion object {
        const val SPEED = 0.7
        const val LEADER_SPEED = 0.595
        const val COMPANION_RANGE = 16.0
        const val ARRIVED_SQUARED = 100.0
        const val SIDE_SHARE = 0.4
        const val STEP_DISTANCE = 10.0
        const val FAILED_PATH_COOLDOWN = 200L
        const val RANDOM_RADIUS = 8
        const val RANDOM_DIAMETER = 16
        const val TARGET_RADIUS = 500
        const val TARGET_DIAMETER = 1000
        val RAIDER_TYPES = setOf("pillager", "vindicator", "evoker", "illusioner", "witch", "ravager")
    }
}
