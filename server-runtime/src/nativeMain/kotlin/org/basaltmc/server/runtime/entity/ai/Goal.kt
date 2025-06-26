package org.basaltmc.server.runtime.entity.ai

/**
 * What a goal takes hold of while it runs.
 *
 * parity: `Goal.Flag`. Two goals that want the same flag cannot run together, and the one with the
 * lower priority number wins: that is the whole of how vanilla decides what a mob is doing.
 */
public enum class GoalFlag {
    MOVE,
    LOOK,
    JUMP,
    TARGET,
}

/**
 * One thing a mob might be doing.
 *
 * parity: `Goal`. A goal is asked whether it can start, kept while it says it can continue, and
 * ticked in between; the instance belongs to one mob, so it can remember its own counters.
 */
public abstract class Goal {
    public open val flags: Set<GoalFlag> = emptySet()

    public abstract fun canUse(mob: MobContext): Boolean

    public open fun canContinueToUse(mob: MobContext): Boolean = canUse(mob)

    /** parity: `Goal.isInterruptable`, which is what stops a panicking mob being talked out of it. */
    public open val isInterruptable: Boolean = true

    public open val requiresUpdateEveryTick: Boolean = false

    public open fun start(mob: MobContext) {}

    public open fun stop(mob: MobContext) {}

    public open fun tick(mob: MobContext) {}

    /** parity: `Goal.adjustedTickDelay`, halved for the goals that are not ticked every tick. */
    protected fun adjustedTickDelay(ticks: Int): Int = if (requiresUpdateEveryTick) ticks else reducedTickDelay(ticks)

    protected fun reducedTickDelay(ticks: Int): Int = (ticks + 1) / 2
}
