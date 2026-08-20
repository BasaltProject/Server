package org.basaltmc.server.runtime.entity.ai

/**
 * parity: `SitWhenOrderedToGoal`: a pet told to sit stays sat, and nothing else it wants to do
 * outranks that.
 */
public class SitWhenOrderedToGoal : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.JUMP, GoalFlag.MOVE)

    override val isInterruptable: Boolean = false

    override fun canUse(mob: MobContext): Boolean = mob.mob.sitting

    override fun canContinueToUse(mob: MobContext): Boolean = mob.mob.sitting

    override fun start(mob: MobContext) {
        Navigation.stop(mob.mob)
    }
}

/**
 * parity: `FollowOwnerGoal`: a pet that is not sitting keeps within a couple of blocks of whoever
 * tamed it, and starts following again once they are ten blocks away.
 */
public class FollowOwnerGoal(
    private val speedModifier: Double,
    private val startDistance: Double = START_DISTANCE,
    private val stopDistance: Double = STOP_DISTANCE,
) : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.MOVE, GoalFlag.LOOK)

    override val requiresUpdateEveryTick: Boolean = true

    private var repathCooldown = 0

    override fun canUse(mob: MobContext): Boolean {
        if (mob.mob.sitting) return false
        val owner = ownerOf(mob) ?: return false
        return mob.distanceSquaredTo(owner.x, owner.y, owner.z) >= startDistance * startDistance
    }

    override fun canContinueToUse(mob: MobContext): Boolean {
        if (mob.mob.sitting) return false
        val owner = ownerOf(mob) ?: return false
        return mob.distanceSquaredTo(owner.x, owner.y, owner.z) > stopDistance * stopDistance
    }

    override fun start(mob: MobContext) {
        repathCooldown = 0
    }

    override fun stop(mob: MobContext) {
        Navigation.stop(mob.mob)
    }

    override fun tick(mob: MobContext) {
        val owner = ownerOf(mob) ?: return
        LookControl.lookAt(mob.mob, owner.x, owner.y + PLAYER_EYE, owner.z, maxYaw = LOOK_SPEED)
        if (--repathCooldown > 0) return
        repathCooldown = adjustedTickDelay(REPATH_INTERVAL)
        Navigation.moveTo(mob.world, mob.mob, owner.x, owner.y, owner.z, speedModifier)
    }

    private companion object {
        const val START_DISTANCE = 10.0
        const val STOP_DISTANCE = 2.0
        const val REPATH_INTERVAL = 10
        const val PLAYER_EYE = 1.62
        const val LOOK_SPEED = 10f
    }
}

/** parity: `BegGoal`: a wolf sits up at whoever is holding something it wants. */
public class BegGoal(
    private val lookDistance: Double,
    private val items: Set<String>,
) : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.LOOK)

    override val requiresUpdateEveryTick: Boolean = true

    private var player: Int? = null
    private var begTime = 0

    override fun canUse(mob: MobContext): Boolean {
        player =
            mob.players
                .filter { it.targetable && it.heldItem?.path in items }
                .filter { mob.distanceSquaredTo(it.x, it.y, it.z) <= lookDistance * lookDistance }
                .minByOrNull { mob.distanceSquaredTo(it.x, it.y, it.z) }
                ?.entityId
        return player != null
    }

    override fun canContinueToUse(mob: MobContext): Boolean {
        val holder = player?.let { mob.playerById(it) } ?: return false
        if (mob.distanceSquaredTo(holder.x, holder.y, holder.z) > lookDistance * lookDistance) return false
        return begTime > 0 && holder.heldItem?.path in items
    }

    override fun start(mob: MobContext) {
        begTime = BEG_TIME + mob.random.nextInt(BEG_TIME)
        mob.mob.begging = true
    }

    override fun stop(mob: MobContext) {
        mob.mob.begging = false
        player = null
    }

    override fun tick(mob: MobContext) {
        val holder = player?.let { mob.playerById(it) } ?: return
        LookControl.lookAt(mob.mob, holder.x, holder.y + PLAYER_EYE, holder.z, maxYaw = LOOK_SPEED)
        begTime--
    }

    private companion object {
        /** parity: `BegGoal.start`, forty to eighty ticks of it. */
        const val BEG_TIME = 40
        const val PLAYER_EYE = 1.62
        const val LOOK_SPEED = 10f
    }
}

/** parity: `OwnerHurtByTargetGoal`: whoever hit the owner is what the pet goes for. */
public class OwnerHurtByTargetGoal : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.TARGET)

    override fun canUse(mob: MobContext): Boolean {
        if (mob.mob.owner == null || mob.mob.sitting) return false
        return mob.mob.ownerHurtBy != null
    }

    override fun start(mob: MobContext) {
        mob.mob.targetId = mob.mob.ownerHurtBy
        mob.mob.ownerHurtBy = null
    }

    override fun canContinueToUse(mob: MobContext): Boolean = targetOf(mob) != null

    override fun stop(mob: MobContext) {
        mob.mob.targetId = null
    }
}

/** parity: `OwnerHurtTargetGoal`: whatever the owner hit is what the pet goes for. */
public class OwnerHurtTargetGoal : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.TARGET)

    override fun canUse(mob: MobContext): Boolean {
        if (mob.mob.owner == null || mob.mob.sitting) return false
        return mob.mob.ownerHurt != null
    }

    override fun start(mob: MobContext) {
        mob.mob.targetId = mob.mob.ownerHurt
        mob.mob.ownerHurt = null
    }

    override fun canContinueToUse(mob: MobContext): Boolean = targetOf(mob) != null

    override fun stop(mob: MobContext) {
        mob.mob.targetId = null
    }
}

/** Whoever tamed this mob, if they are here. */
internal fun ownerOf(mob: MobContext): PlayerView? {
    val owner = mob.mob.owner ?: return null
    return mob.players.firstOrNull { it.uuid == owner }
}
