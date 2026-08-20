package org.basaltmc.server.runtime.entity.ai

import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.runtime.world.WorldState
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.sin
import kotlin.math.sqrt

/**
 * A spider going up a wall.
 *
 * parity: `Spider.onClimbable`, which is simply "am I walking into something": while it is, the
 * climb replaces the fall, which is what takes a spider straight up the side of a house.
 */
public class ClimbWallsGoal : Goal() {
    override val requiresUpdateEveryTick: Boolean = true

    override fun canUse(mob: MobContext): Boolean = mob.mob.horizontallyBlocked

    override fun canContinueToUse(mob: MobContext): Boolean = mob.mob.horizontallyBlocked

    override fun tick(mob: MobContext) {
        // parity: `LivingEntity.travel`'s climbing branch, which pins the fall at 0.2 upwards.
        mob.mob.velocityY = CLIMB_SPEED
        mob.mob.onGround = false
    }

    private companion object {
        const val CLIMB_SPEED = 0.2
    }
}

/**
 * An enderman noticing that somebody is staring at it.
 *
 * parity: `EnderMan.EndermanLookForPlayerGoal` over `isLookingAtMe`: the player has to be looking
 * almost straight at its eyes, and the cone widens the closer they are.
 */
public class StareTriggerGoal : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.TARGET)

    private var aggroTime = 0
    private var pending: Int? = null

    override fun canUse(mob: MobContext): Boolean {
        if (mob.mob.targetId != null) return false
        pending =
            mob.players
                .filter { it.targetable && staredAtBy(mob, it) }
                .minByOrNull { mob.distanceSquaredTo(it.x, it.y, it.z) }
                ?.entityId
        return pending != null
    }

    override fun start(mob: MobContext) {
        // parity: five ticks of being stared at before it comes for you.
        aggroTime = adjustedTickDelay(AGGRO_DELAY)
    }

    override fun canContinueToUse(mob: MobContext): Boolean {
        val player = pending?.let { mob.playerById(it) } ?: return false
        if (!staredAtBy(mob, player)) return false
        if (aggroTime > 0) {
            aggroTime--
            return true
        }
        mob.mob.targetId = player.entityId
        return false
    }

    override fun stop(mob: MobContext) {
        pending = null
    }

    /** parity: `isLookingAtMe(player, 0.025, true, false, eyeY)`. */
    private fun staredAtBy(
        mob: MobContext,
        player: PlayerView,
    ): Boolean {
        if (mob.distanceSquaredTo(player.x, player.y, player.z) > STARE_RANGE * STARE_RANGE) return false
        val eyeY = mob.mob.y + mob.mob.height * EYE_SHARE
        var dx = mob.mob.x - player.x
        var dy = eyeY - (player.y + PLAYER_EYE)
        var dz = mob.mob.z - player.z
        val distance = sqrt(dx * dx + dy * dy + dz * dz)
        if (distance < 1.0e-4) return false
        dx /= distance
        dy /= distance
        dz /= distance
        val pitch = player.pitch * PI / DEGREES
        val yaw = player.yaw * PI / DEGREES
        val lookX = -sin(yaw) * cos(pitch)
        val lookY = -sin(pitch)
        val lookZ = cos(yaw) * cos(pitch)
        val dot = lookX * dx + lookY * dy + lookZ * dz
        return dot > 1.0 - CONE / distance
    }

    private companion object {
        const val AGGRO_DELAY = 5
        const val STARE_RANGE = 64.0
        const val CONE = 0.025
        const val EYE_SHARE = 0.85
        const val PLAYER_EYE = 1.62
        const val DEGREES = 180.0
    }
}

/**
 * An enderman getting out of trouble.
 *
 * parity: `EnderMan.teleport`, `teleportTowards` and `customServerAiStep`: it goes away when it is
 * hit, when it is rained on or standing in water, and when the sun catches it holding no target.
 */
public class TeleportAwayGoal : Goal() {
    override val requiresUpdateEveryTick: Boolean = true

    override fun canUse(mob: MobContext): Boolean = true

    override fun tick(mob: MobContext) {
        val world = mob.world
        val blockX = floor(mob.mob.x).toInt()
        val blockY = floor(mob.mob.y).toInt()
        val blockZ = floor(mob.mob.z).toInt()
        // parity: `isSensitiveToWater`: water hurts it, and it leaves.
        if (isWater(world, blockX, blockY, blockZ)) {
            mob.actions.hurtMob(mob.mob, mob.mob, WATER_DAMAGE)
            teleportRandomly(mob)
            return
        }
        // parity: the teleport a hit provokes, which is what makes one so hard to corner.
        if (mob.mob.hurtBy != null && mob.random.nextInt(TELEPORT_ON_HURT_ODDS) == 0) {
            teleportRandomly(mob)
            return
        }
        // parity: `customServerAiStep`, the daylight teleport for an enderman with nothing to chase.
        if (mob.mob.targetId == null && !mob.night && mob.light > DAYLIGHT && mob.random.nextInt(DAY_ODDS) == 0) {
            teleportRandomly(mob)
        }
    }

    private fun teleportRandomly(mob: MobContext) {
        val x = mob.mob.x + (mob.random.nextDouble() - 0.5) * RANGE
        val y = mob.mob.y + (mob.random.nextInt(RANGE.toInt()) - RANGE / 2)
        val z = mob.mob.z + (mob.random.nextDouble() - 0.5) * RANGE
        val landing = groundBelow(mob.world, x, y, z) ?: return
        mob.mob.x = x
        mob.mob.y = landing
        mob.mob.z = z
        mob.mob.velocityX = 0.0
        mob.mob.velocityY = 0.0
        mob.mob.velocityZ = 0.0
        Navigation.stop(mob.mob)
        mob.actions.teleported(mob.mob)
    }

    /** parity: the teleport walks down until it finds something that blocks motion. */
    private fun groundBelow(
        world: WorldState,
        x: Double,
        y: Double,
        z: Double,
    ): Double? {
        val blockX = floor(x).toInt()
        val blockZ = floor(z).toInt()
        var blockY = floor(y).toInt()
        var steps = 0
        while (steps < SEARCH_LIMIT) {
            val state = world.getBlockState(blockX, blockY - 1, blockZ) ?: return null
            if (!BlockShapes.isEmptyShape(state)) {
                val feet = world.getBlockState(blockX, blockY, blockZ) ?: return null
                val head = world.getBlockState(blockX, blockY + 1, blockZ) ?: return null
                if (!BlockShapes.isEmptyShape(feet) || !BlockShapes.isEmptyShape(head)) return null
                return blockY.toDouble()
            }
            blockY--
            steps++
        }
        return null
    }

    private fun isWater(
        world: WorldState,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean =
        org.basaltmc.server.registry.BlockStateDefinitions
            .blockOf(world.getBlockState(x, y, z) ?: return false)
            ?.path == "water"

    private companion object {
        /** parity: sixty-four blocks across, thirty-two up or down. */
        const val RANGE = 64.0
        const val SEARCH_LIMIT = 64
        const val WATER_DAMAGE = 1f
        const val TELEPORT_ON_HURT_ODDS = 5
        const val DAY_ODDS = 30
        const val DAYLIGHT = 8
    }
}

/** How far apart two points are, for the goals that measure it themselves. */
internal fun distanceBetween(
    ax: Double,
    ay: Double,
    az: Double,
    bx: Double,
    by: Double,
    bz: Double,
): Double = sqrt(abs(ax - bx) * abs(ax - bx) + abs(ay - by) * abs(ay - by) + abs(az - bz) * abs(az - bz))
