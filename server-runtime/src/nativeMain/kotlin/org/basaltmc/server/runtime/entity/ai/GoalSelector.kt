package org.basaltmc.server.runtime.entity.ai

/** One goal with the priority it was registered at. parity: `WrappedGoal`. */
public class WrappedGoal(
    public val priority: Int,
    public val goal: Goal,
) {
    public var running: Boolean = false
        private set

    public fun canBeReplacedBy(other: WrappedGoal): Boolean = goal.isInterruptable && other.priority < priority

    public fun start(mob: MobContext) {
        if (running) return
        running = true
        goal.start(mob)
    }

    public fun stop(mob: MobContext) {
        if (!running) return
        running = false
        goal.stop(mob)
    }
}

/**
 * What decides, every tick, which of a mob's goals are running.
 *
 * parity: `GoalSelector.tick`, in its own order: stop what can no longer continue, free the flags it
 * held, start whatever can start and outranks what holds its flags, then tick everything running.
 */
public class GoalSelector {
    private val goals = mutableListOf<WrappedGoal>()
    private val lockedFlags = HashMap<GoalFlag, WrappedGoal>()

    public fun add(
        priority: Int,
        goal: Goal,
    ) {
        goals += WrappedGoal(priority, goal)
    }

    public val size: Int get() = goals.size

    /** Whether a goal of this kind is running, which is all the outside needs to know. */
    public fun isRunning(predicate: (Goal) -> Boolean): Boolean = goals.any { it.running && predicate(it.goal) }

    public fun tick(mob: MobContext) {
        for (wrapped in goals) {
            if (wrapped.running && !wrapped.goal.canContinueToUse(mob)) wrapped.stop(mob)
        }
        lockedFlags.entries.removeAll { !it.value.running }
        for (wrapped in goals) {
            if (wrapped.running) continue
            if (!canTakeFlags(wrapped)) continue
            if (!wrapped.goal.canUse(mob)) continue
            for (flag in wrapped.goal.flags) {
                lockedFlags[flag]?.stop(mob)
                lockedFlags[flag] = wrapped
            }
            wrapped.start(mob)
        }
        tickRunning(mob, all = true)
    }

    /**
     * parity: `GoalSelector.tickRunningGoals`, which is all a mob does on the ticks between full
     * decisions: `Mob` alternates by `tickCount + id`, so half the mobs decide on each tick.
     */
    public fun tickRunning(
        mob: MobContext,
        all: Boolean,
    ) {
        for (wrapped in goals) {
            if (wrapped.running && (all || wrapped.goal.requiresUpdateEveryTick)) wrapped.goal.tick(mob)
        }
    }

    private fun canTakeFlags(wrapped: WrappedGoal): Boolean =
        wrapped.goal.flags.all { flag -> lockedFlags[flag]?.canBeReplacedBy(wrapped) ?: true }
}
