package org.basaltmc.server.runtime.entity.ai

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.entity.Breeding
import org.basaltmc.server.runtime.entity.LivingMovement
import org.basaltmc.server.runtime.entity.MobBrain
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.wire.common.Identifier
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.sin
import kotlin.math.sqrt

/** parity: `FloatGoal`, which is why a mob in water bobs instead of drowning. */
public class FloatGoal : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.JUMP)

    override val requiresUpdateEveryTick: Boolean = true

    override fun canUse(mob: MobContext): Boolean = inWater(mob)

    override fun tick(mob: MobContext) {
        if (mob.random.nextFloat() < JUMP_CHANCE) LivingMovement.jump(mob.world, mob.mob)
    }

    private fun inWater(mob: MobContext): Boolean {
        val state =
            mob.world.getBlockState(
                floor(mob.mob.x).toInt(),
                floor(mob.mob.y).toInt(),
                floor(mob.mob.z).toInt(),
            )
        val block = state?.let { BlockStateDefinitions.blockOf(it) } ?: return false
        return block.path == "water" || block.path == "lava"
    }

    private companion object {
        /** parity: `FloatGoal.tick`, four times out of five. */
        const val JUMP_CHANCE = 0.8f
    }
}

/**
 * parity: `RandomStrollGoal` over `DefaultRandomPos.getPos(mob, 10, 7)`: one roll in sixty (the
 * interval halved, since the goal is not ticked every tick), and the walk it starts is a real path.
 */
public open class RandomStrollGoal(
    private val speedModifier: Double,
    private val interval: Int = DEFAULT_INTERVAL,
) : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.MOVE)

    private var wanted: DoubleArray? = null

    override fun canUse(mob: MobContext): Boolean {
        // parity: a mob nobody has been near for five seconds stops wandering.
        if (mob.mob.idleTicks >= NO_ACTION_LIMIT) return false
        if (mob.random.nextInt(reducedTickDelay(interval)) != 0) return false
        wanted = MobBrain.strollTarget(mob.world, mob.mob, mob.random)
        return wanted != null
    }

    override fun canContinueToUse(mob: MobContext): Boolean = !Navigation.isDone(mob.mob)

    override fun start(mob: MobContext) {
        val target = wanted ?: return
        Navigation.moveTo(mob.world, mob.mob, target[0], target[1], target[2], speedModifier)
    }

    override fun stop(mob: MobContext) {
        Navigation.stop(mob.mob)
    }

    public companion object {
        public const val DEFAULT_INTERVAL: Int = 120
        private const val NO_ACTION_LIMIT = 100
    }
}

/** parity: `LookAtPlayerGoal`, the one that makes a mob turn its head as you walk past. */
public class LookAtPlayerGoal(
    private val lookDistance: Double,
    private val probability: Float = DEFAULT_PROBABILITY,
) : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.LOOK)

    override val requiresUpdateEveryTick: Boolean = true

    private var lookAt: Int? = null
    private var lookTime = 0

    override fun canUse(mob: MobContext): Boolean {
        if (mob.random.nextFloat() >= probability) return false
        lookAt = mob.nearestPlayer(lookDistance)?.entityId
        return lookAt != null
    }

    override fun canContinueToUse(mob: MobContext): Boolean {
        val player = lookAt?.let { mob.playerById(it) } ?: return false
        if (mob.distanceSquaredTo(player.x, player.y, player.z) > lookDistance * lookDistance) return false
        return lookTime > 0
    }

    override fun start(mob: MobContext) {
        lookTime = adjustedTickDelay(LOOK_TIME + mob.random.nextInt(LOOK_TIME))
    }

    override fun stop(mob: MobContext) {
        lookAt = null
    }

    override fun tick(mob: MobContext) {
        val player = lookAt?.let { mob.playerById(it) } ?: return
        LookControl.lookAt(mob.mob, player.x, player.y + PLAYER_EYE, player.z)
        lookTime--
    }

    private companion object {
        const val DEFAULT_PROBABILITY = 0.02f
        const val LOOK_TIME = 40
        const val PLAYER_EYE = 1.62
    }
}

/** parity: `RandomLookAroundGoal`, one roll in fifty a tick and a second of looking that way. */
public class RandomLookAroundGoal : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.MOVE, GoalFlag.LOOK)

    override val requiresUpdateEveryTick: Boolean = true

    private var relX = 0.0
    private var relZ = 0.0
    private var lookTime = 0

    override fun canUse(mob: MobContext): Boolean = mob.random.nextFloat() < CHANCE

    override fun canContinueToUse(mob: MobContext): Boolean = lookTime >= 0

    override fun start(mob: MobContext) {
        val angle = 2.0 * PI * mob.random.nextDouble()
        relX = cos(angle)
        relZ = sin(angle)
        lookTime = LOOK_TIME + mob.random.nextInt(LOOK_TIME)
    }

    override fun tick(mob: MobContext) {
        lookTime--
        LookControl.lookAt(mob.mob, mob.mob.x + relX, mob.mob.y + mob.mob.height * EYE_SHARE, mob.mob.z + relZ)
    }

    private companion object {
        const val CHANCE = 0.02f
        const val LOOK_TIME = 20
        const val EYE_SHARE = 0.85
    }
}

/**
 * parity: `PanicGoal`: something hurt it, so it runs to a spot within five blocks and keeps running
 * until it gets there.
 */
public class PanicGoal(
    private val speedModifier: Double,
) : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.MOVE)

    private var wanted: DoubleArray? = null

    override fun canUse(mob: MobContext): Boolean {
        if (mob.mob.panicTicks <= 0) return false
        wanted = MobBrain.strollTarget(mob.world, mob.mob, mob.random, range = PANIC_RANGE, height = PANIC_HEIGHT)
        return wanted != null
    }

    override fun canContinueToUse(mob: MobContext): Boolean = !Navigation.isDone(mob.mob)

    override fun start(mob: MobContext) {
        val target = wanted ?: return
        Navigation.moveTo(mob.world, mob.mob, target[0], target[1], target[2], speedModifier)
    }

    override fun stop(mob: MobContext) {
        Navigation.stop(mob.mob)
    }

    private companion object {
        /** parity: `PanicGoal.findRandomPosition`, five across and four up. */
        const val PANIC_RANGE = 5
        const val PANIC_HEIGHT = 4
    }
}

/**
 * parity: `TemptGoal`: a player holding the right food is followed to within two and a half blocks,
 * and the mob looks at them the whole way.
 */
public class TemptGoal(
    private val speedModifier: Double,
    private val items: Set<String>,
    private val stopDistance: Double = DEFAULT_STOP_DISTANCE,
) : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.MOVE, GoalFlag.LOOK)

    override val requiresUpdateEveryTick: Boolean = true

    private var player: Int? = null

    override fun canUse(mob: MobContext): Boolean {
        player =
            mob.players
                .filter { it.targetable && it.heldItem?.path in items }
                .filter { mob.distanceSquaredTo(it.x, it.y, it.z) <= TEMPT_RANGE * TEMPT_RANGE }
                .minByOrNull { mob.distanceSquaredTo(it.x, it.y, it.z) }
                ?.entityId
        return player != null
    }

    override fun canContinueToUse(mob: MobContext): Boolean {
        val holder = player?.let { mob.playerById(it) } ?: return false
        return holder.heldItem?.path in items &&
            mob.distanceSquaredTo(holder.x, holder.y, holder.z) <= TEMPT_RANGE * TEMPT_RANGE
    }

    override fun stop(mob: MobContext) {
        player = null
        Navigation.stop(mob.mob)
    }

    override fun tick(mob: MobContext) {
        val holder = player?.let { mob.playerById(it) } ?: return
        LookControl.lookAt(mob.mob, holder.x, holder.y + PLAYER_EYE, holder.z, maxYaw = LOOK_SPEED)
        if (mob.distanceSquaredTo(holder.x, holder.y, holder.z) < stopDistance * stopDistance) {
            Navigation.stop(mob.mob)
        } else {
            Navigation.moveTo(mob.world, mob.mob, holder.x, holder.y, holder.z, speedModifier)
        }
    }

    private companion object {
        const val DEFAULT_STOP_DISTANCE = 2.5

        /** parity: `Attributes.TEMPT_RANGE`, ten blocks for everything that has it. */
        const val TEMPT_RANGE = 10.0
        const val PLAYER_EYE = 1.62
        const val LOOK_SPEED = 40f
    }
}

/** parity: `BreedGoal`: two of the same kind in love walk together for three seconds and breed. */
public class BreedGoal(
    private val speedModifier: Double,
) : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.MOVE, GoalFlag.LOOK)

    override val requiresUpdateEveryTick: Boolean = true

    private var partner: ServerEntity? = null
    private var loveTime = 0

    override fun canUse(mob: MobContext): Boolean {
        if (mob.mob.loveTicks <= 0) return false
        partner =
            mob.entities
                .filter { Breeding.canMate(mob.mob, it) }
                .minByOrNull { mob.distanceSquaredTo(it.x, it.y, it.z) }
        return partner != null
    }

    override fun canContinueToUse(mob: MobContext): Boolean {
        val other = partner ?: return false
        return !other.removed && mob.mob.loveTicks > 0 && other.loveTicks > 0
    }

    override fun start(mob: MobContext) {
        loveTime = 0
    }

    override fun stop(mob: MobContext) {
        partner = null
        loveTime = 0
        Navigation.stop(mob.mob)
    }

    override fun tick(mob: MobContext) {
        val other = partner ?: return
        LookControl.lookAt(mob.mob, other.x, other.y + other.height * EYE_SHARE, other.z, maxYaw = LOOK_SPEED)
        Navigation.moveTo(mob.world, mob.mob, other.x, other.y, other.z, speedModifier)
        // The three seconds of standing together, and the calf itself, are counted by the world's
        // own breeding tick: this goal is the walking half of `BreedGoal`.
        loveTime++
    }

    private companion object {
        /** parity: `BreedGoal.loveTime`, sixty ticks of standing together. */
        const val BREED_TIME = 60
        const val BREED_RANGE = 9.0
        const val EYE_SHARE = 0.85
        const val LOOK_SPEED = 10f
    }
}

/** parity: `FollowParentGoal`: a calf walks after the nearest grown one of its own kind. */
public class FollowParentGoal(
    private val speedModifier: Double,
) : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.MOVE)

    private var parent: ServerEntity? = null
    private var repathCooldown = 0

    override fun canUse(mob: MobContext): Boolean {
        if (mob.mob.age >= 0) return false
        parent =
            mob.entities
                .filter { it !== mob.mob && it.type == mob.mob.type && it.age >= 0 && !it.removed }
                .filter { mob.distanceSquaredTo(it.x, it.y, it.z) <= FOLLOW_RANGE * FOLLOW_RANGE }
                .minByOrNull { mob.distanceSquaredTo(it.x, it.y, it.z) }
        return parent != null
    }

    override fun canContinueToUse(mob: MobContext): Boolean {
        val adult = parent ?: return false
        if (mob.mob.age >= 0 || adult.removed) return false
        val distance = mob.distanceSquaredTo(adult.x, adult.y, adult.z)
        return distance >= STOP_RANGE * STOP_RANGE && distance <= FOLLOW_RANGE * FOLLOW_RANGE
    }

    override fun start(mob: MobContext) {
        repathCooldown = 0
    }

    override fun stop(mob: MobContext) {
        parent = null
        Navigation.stop(mob.mob)
    }

    override fun tick(mob: MobContext) {
        val adult = parent ?: return
        if (--repathCooldown > 0) return
        repathCooldown = adjustedTickDelay(REPATH_INTERVAL)
        Navigation.moveTo(mob.world, mob.mob, adult.x, adult.y, adult.z, speedModifier)
    }

    private companion object {
        const val FOLLOW_RANGE = 8.0
        const val STOP_RANGE = 3.0
        const val REPATH_INTERVAL = 10
    }
}

/** parity: `EatBlockGoal`: a sheep stops, eats the grass under it, and leaves dirt behind. */
public class EatBlockGoal : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.MOVE, GoalFlag.LOOK, GoalFlag.JUMP)

    override val requiresUpdateEveryTick: Boolean = true

    private var eatTicks = 0

    override fun canUse(mob: MobContext): Boolean {
        if (mob.random.nextInt(if (mob.mob.age < 0) BABY_CHANCE else ADULT_CHANCE) != 0) return false
        return grassUnder(mob) || grassInFront(mob)
    }

    override fun canContinueToUse(mob: MobContext): Boolean = eatTicks > 0

    override fun start(mob: MobContext) {
        eatTicks = EAT_TICKS
        Navigation.stop(mob.mob)
    }

    override fun stop(mob: MobContext) {
        eatTicks = 0
    }

    override fun tick(mob: MobContext) {
        eatTicks = maxOf(0, eatTicks - 1)
        if (eatTicks != EAT_FINISH) return
        mob.actions.eatGrass(mob.mob)
    }

    private fun grassInFront(mob: MobContext): Boolean {
        val state =
            mob.world.getBlockState(
                floor(mob.mob.x).toInt(),
                floor(mob.mob.y).toInt(),
                floor(mob.mob.z).toInt(),
            )
        return state != null && BlockStateDefinitions.blockOf(state)?.path == "short_grass"
    }

    private fun grassUnder(mob: MobContext): Boolean {
        val under =
            mob.world.getBlockState(floor(mob.mob.x).toInt(), floor(mob.mob.y).toInt() - 1, floor(mob.mob.z).toInt())
        return under != null && BlockStateDefinitions.blockOf(under)?.path == "grass_block"
    }

    private companion object {
        /** parity: `EatBlockGoal`, one roll in a thousand for a grown sheep and fifty for a lamb. */
        const val ADULT_CHANCE = 1000
        const val BABY_CHANCE = 50
        const val EAT_TICKS = 40
        const val EAT_FINISH = 4
    }
}

/** parity: `AvoidEntityGoal`: a creeper walks away from a cat, a skeleton from a wolf. */
public class AvoidEntityGoal(
    private val avoided: Set<String>,
    private val distance: Double,
    private val walkSpeed: Double,
    private val sprintSpeed: Double,
) : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.MOVE)

    private var threat: ServerEntity? = null
    private var wanted: DoubleArray? = null

    override fun canUse(mob: MobContext): Boolean {
        threat =
            mob.entities
                .filter { it.type.path in avoided && !it.removed }
                .filter { mob.distanceSquaredTo(it.x, it.y, it.z) <= distance * distance }
                .minByOrNull { mob.distanceSquaredTo(it.x, it.y, it.z) }
        val from = threat ?: return false
        // parity: `DefaultRandomPos.getPosAway`, a spot roughly away from what it is avoiding.
        val away = MobBrain.strollTarget(mob.world, mob.mob, mob.random) ?: return false
        val towardsX = mob.mob.x - from.x
        val towardsZ = mob.mob.z - from.z
        if ((away[0] - mob.mob.x) * towardsX + (away[2] - mob.mob.z) * towardsZ < 0.0) return false
        wanted = away
        return true
    }

    override fun canContinueToUse(mob: MobContext): Boolean = !Navigation.isDone(mob.mob)

    override fun start(mob: MobContext) {
        val target = wanted ?: return
        Navigation.moveTo(mob.world, mob.mob, target[0], target[1], target[2], walkSpeed)
    }

    override fun tick(mob: MobContext) {
        val from = threat ?: return
        // parity: it breaks into a run once whatever it is avoiding is close.
        mob.mob.moveSpeed =
            if (mob.distanceSquaredTo(from.x, from.y, from.z) < SPRINT_RANGE * SPRINT_RANGE) sprintSpeed else walkSpeed
    }

    override fun stop(mob: MobContext) {
        threat = null
        Navigation.stop(mob.mob)
    }

    private companion object {
        const val SPRINT_RANGE = 7.0
    }
}

/** parity: `LeapAtTargetGoal`, the pounce a spider or a wolf makes at what it is chasing. */
public class LeapAtTargetGoal(
    private val height: Double,
) : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.JUMP, GoalFlag.MOVE)

    override fun canUse(mob: MobContext): Boolean {
        if (!mob.mob.onGround) return false
        val target = targetOf(mob) ?: return false
        val distance = mob.distanceSquaredTo(target[0], target[1], target[2])
        if (distance < MIN_RANGE || distance > MAX_RANGE) return false
        return mob.random.nextInt(reducedTickDelay(CHANCE)) == 0
    }

    override fun canContinueToUse(mob: MobContext): Boolean = !mob.mob.onGround

    override fun start(mob: MobContext) {
        val target = targetOf(mob) ?: return
        var dx = target[0] - mob.mob.x
        var dz = target[2] - mob.mob.z
        val length = sqrt(dx * dx + dz * dz)
        if (length < 1.0e-4) return
        dx = dx / length * LEAP_SPEED + mob.mob.velocityX * KEPT_SPEED
        dz = dz / length * LEAP_SPEED + mob.mob.velocityZ * KEPT_SPEED
        mob.mob.velocityX = dx
        mob.mob.velocityZ = dz
        mob.mob.velocityY = height
        mob.mob.onGround = false
    }

    private fun targetOf(mob: MobContext): DoubleArray? {
        val id = mob.mob.targetId ?: return null
        mob.playerById(id)?.let { return doubleArrayOf(it.x, it.y, it.z) }
        mob.entityById(id)?.let { return doubleArrayOf(it.x, it.y, it.z) }
        return null
    }

    private companion object {
        const val MIN_RANGE = 4.0
        const val MAX_RANGE = 16.0
        const val CHANCE = 5
        const val LEAP_SPEED = 0.4
        const val KEPT_SPEED = 0.2
    }
}

/** What a mob is standing on, for the goals that care about the ground. */
internal fun blockPathUnder(
    mob: ServerEntity,
    world: org.basaltmc.server.runtime.world.WorldState,
): Identifier? {
    val state = world.getBlockState(floor(mob.x).toInt(), floor(mob.y).toInt() - 1, floor(mob.z).toInt()) ?: return null
    return BlockStateDefinitions.blockOf(state)
}

/** Whether the block at these coordinates is air, which is what a leap needs above it. */
internal fun isAir(
    world: org.basaltmc.server.runtime.world.WorldState,
    x: Int,
    y: Int,
    z: Int,
): Boolean = (world.getBlockState(x, y, z) ?: Blocks.AIR.stateId) == Blocks.AIR.stateId

/** How far apart two points are on the ground, which several goals compare against a range. */
internal fun horizontalDistance(
    ax: Double,
    az: Double,
    bx: Double,
    bz: Double,
): Double = sqrt(abs(ax - bx) * abs(ax - bx) + abs(az - bz) * abs(az - bz))

/**
 * parity: `SwellGoal` over `Creeper.tick`: within three blocks of what it is after the fuse counts
 * up, past seven or out of sight it counts back down, and at thirty it goes off.
 */
public class SwellGoal : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.MOVE)

    override val requiresUpdateEveryTick: Boolean = true

    override fun canUse(mob: MobContext): Boolean {
        if (mob.mob.swell > 0) return true
        val target = targetOf(mob) ?: return false
        return mob.distanceSquaredTo(target.x, target.y, target.z) < SWELL_RANGE
    }

    override fun start(mob: MobContext) {
        Navigation.stop(mob.mob)
    }

    override fun tick(mob: MobContext) {
        val target = targetOf(mob)
        val direction =
            when {
                target == null -> -1
                mob.distanceSquaredTo(target.x, target.y, target.z) > STOP_RANGE -> -1
                else -> 1
            }
        if (direction > 0 && mob.mob.swell == 0) {
            mob.actions.primeCreeper(mob.mob)
        }
        mob.mob.swell = (mob.mob.swell + direction).coerceAtLeast(0)
        if (mob.mob.swell < MAX_SWELL) return
        mob.mob.swell = MAX_SWELL
        mob.actions.explodeCreeper(mob.mob)
    }

    private companion object {
        /** parity: `Creeper`'s own three blocks to start, seven to stop, and thirty ticks of fuse. */
        const val SWELL_RANGE = 9.0
        const val STOP_RANGE = 49.0
        const val MAX_SWELL = 30
    }
}

/**
 * parity: `RangedAttackGoal` over `AbstractSkeleton.performRangedAttack`: it closes to inside its
 * range, stands still once it can see you, and lets an arrow go on its own clock.
 */
public class RangedAttackGoal(
    private val speedModifier: Double,
    private val attackIntervalMin: Int,
    private val attackIntervalMax: Int,
    private val attackRadius: Double,
) : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.MOVE, GoalFlag.LOOK)

    override val requiresUpdateEveryTick: Boolean = true

    private var seeTime = 0
    private var attackTime = -1

    override fun canUse(mob: MobContext): Boolean = targetOf(mob) != null

    override fun canContinueToUse(mob: MobContext): Boolean = targetOf(mob) != null || !Navigation.isDone(mob.mob)

    override fun stop(mob: MobContext) {
        seeTime = 0
        attackTime = -1
        Navigation.stop(mob.mob)
    }

    override fun tick(mob: MobContext) {
        val target = targetOf(mob) ?: return
        val distanceSquared = mob.distanceSquaredTo(target.x, target.y, target.z)
        seeTime++
        if (distanceSquared <= attackRadius * attackRadius && seeTime >= SEE_TIME) {
            Navigation.stop(mob.mob)
        } else {
            Navigation.moveTo(mob.world, mob.mob, target.x, target.y, target.z, speedModifier)
        }
        LookControl.lookAt(mob.mob, target.x, target.y + target.height * EYE_SHARE, target.z, maxYaw = LOOK_SPEED)
        attackTime--
        if (attackTime == 0) {
            val reach = (sqrt(distanceSquared) / attackRadius).coerceIn(MIN_POWER, 1.0)
            mob.actions.shootArrow(mob.mob, target.x, target.y, target.z, reach.toFloat())
            attackTime = (reach * (attackIntervalMax - attackIntervalMin) + attackIntervalMin).toInt()
        } else if (attackTime < 0) {
            val reach = (sqrt(distanceSquared) / attackRadius).coerceIn(0.0, 1.0)
            attackTime = (attackIntervalMin + reach * (attackIntervalMax - attackIntervalMin)).toInt()
        }
    }

    private companion object {
        const val SEE_TIME = 5
        const val MIN_POWER = 0.1
        const val EYE_SHARE = 0.85
        const val LOOK_SPEED = 30f
    }
}

/**
 * parity: `Squid.SquidRandomMovementGoal` with `Squid.aiStep`: a squid does not walk a path at all,
 * it points itself somewhere and pushes, and the water slows it down between pushes.
 */
public class SwimAboutGoal(
    private val push: Double = SQUID_PUSH,
) : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.MOVE, GoalFlag.LOOK)

    override val requiresUpdateEveryTick: Boolean = true

    private var vectorX = 0.0
    private var vectorY = 0.0
    private var vectorZ = 0.0

    override fun canUse(mob: MobContext): Boolean = true

    override fun tick(mob: MobContext) {
        if (mob.mob.idleTicks > IDLE_LIMIT) {
            vectorX = 0.0
            vectorY = 0.0
            vectorZ = 0.0
        } else if (mob.random.nextInt(reducedTickDelay(CHANGE_INTERVAL)) == 0 || (vectorX == 0.0 && vectorZ == 0.0)) {
            val angle = mob.random.nextFloat() * 2f * PI.toFloat()
            vectorX = cos(angle).toDouble() * push
            vectorY = -RISE + mob.random.nextFloat() * (2 * RISE)
            vectorZ = sin(angle).toDouble() * push
        }
        mob.mob.velocityX = vectorX
        mob.mob.velocityY = vectorY
        mob.mob.velocityZ = vectorZ
        // parity: `Squid.aiStep`'s own turn towards where it is going.
        LookControl.lookAt(
            mob.mob,
            mob.mob.x + vectorX * LOOK_AHEAD,
            mob.mob.y + vectorY,
            mob.mob.z + vectorZ * LOOK_AHEAD,
        )
    }

    private companion object {
        /** parity: the 0.2 a squid pushes with, and the tenth of a block up or down it picks. */
        const val SQUID_PUSH = 0.2
        const val RISE = 0.1
        const val CHANGE_INTERVAL = 50
        const val IDLE_LIMIT = 100
        const val LOOK_AHEAD = 10.0
    }
}

/**
 * parity: `AbstractCubeMob.CubeMobMoveControl`: a slime does not walk, it turns to a chosen heading
 * and hops, with ten to thirty ticks between hops and a third of that when it is angry.
 */
public class HopAboutGoal : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.MOVE, GoalFlag.JUMP, GoalFlag.LOOK)

    override val requiresUpdateEveryTick: Boolean = true

    private var heading = 0f
    private var nextTurn = 0
    private var jumpDelay = 0

    override fun canUse(mob: MobContext): Boolean = true

    override fun tick(mob: MobContext) {
        val target = targetOf(mob)
        if (target != null) {
            LookControl.lookAt(mob.mob, target.x, target.y, target.z, maxYaw = TURN_SPEED)
            heading = mob.mob.headYaw
        } else if (--nextTurn <= 0) {
            // parity: `CubeMobRandomDirectionGoal`, a new heading every few seconds.
            nextTurn = TURN_INTERVAL + mob.random.nextInt(TURN_INTERVAL)
            heading = mob.random.nextFloat() * 360f
        }
        mob.mob.yaw = MoveControl.rotlerp(mob.mob.yaw, heading, TURN_RATE)
        mob.mob.headYaw = mob.mob.yaw
        // parity: `CubeMobMoveControl.tick`: in the air it keeps its speed, on the ground it only
        // has one on the tick it jumps, and none at all while it waits for the next hop.
        if (!mob.mob.onGround) {
            MoveControl.setForward(mob.mob, FORWARD_SHARE)
            return
        }
        if (--jumpDelay > 0) return
        jumpDelay = JUMP_DELAY_MIN + mob.random.nextInt(JUMP_DELAY_SPREAD)
        if (target != null) jumpDelay /= AGGRESSIVE_SHARE
        org.basaltmc.server.runtime.entity.LivingMovement
            .jump(mob.world, mob.mob)
        MoveControl.setForward(mob.mob, FORWARD_SHARE)
    }

    private companion object {
        const val TURN_INTERVAL = 40
        const val TURN_RATE = 90f
        const val TURN_SPEED = 10f
        const val JUMP_DELAY_MIN = 10
        const val JUMP_DELAY_SPREAD = 20
        const val AGGRESSIVE_SHARE = 3

        /** parity: `setWantedMovement(1.0)`, which is the whole of its speed and nothing more. */
        const val FORWARD_SHARE = 1.0
    }
}
