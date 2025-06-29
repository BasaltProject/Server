package org.basaltmc.server.runtime.sched

import org.basaltmc.wire.io.NativeMutex
import org.basaltmc.wire.io.NativeThread
import kotlin.concurrent.AtomicInt
import kotlin.concurrent.AtomicLong

/** What a caller keeps of a task it handed to an owner: a way to take it back. */
public class TaskHandle internal constructor() {
    private val cancelled = AtomicInt(0)

    public val isCancelled: Boolean get() = cancelled.value != 0

    /**
     * Takes the task back. Safe from any thread and at any time: a task already running is not
     * interrupted, and one that has already run does not come back.
     */
    public fun cancel() {
        cancelled.value = 1
    }
}

private class ScheduledTask(
    val handle: TaskHandle,
    val task: () -> Unit,
    var dueInTicks: Int,
)

/**
 * One thread and the state it owns.
 *
 * parity of model with `RegionizedServer`: everything mutable belongs to exactly one owner, and
 * whoever is not that owner asks by [submit] instead of touching it. With one region there is one
 * world owner and one global owner; with several there is one per region. The rule is the same
 * either way, which is the point: code written against it does not change when regions are turned
 * on.
 *
 * **What a task may do.** Read and write whatever this owner owns, and hand work to another owner.
 * What it must not do is wait for another owner to answer: two owners waiting on each other is a
 * deadlock, and with one region "the other owner" is itself. The one place that pumps its own queue
 * while waiting is the chunk generation await, which is documented where it happens and exists
 * because the workers hand their chunks back here.
 *
 * **What a task adds belongs to the next turn**, not to this one: [runPending] takes the queue as
 * it stands and runs that, which is what keeps a tick bounded when a task submits another.
 */
public class Owner(
    /** What it owns, for the message a strict-mode failure prints. */
    public val name: String,
) {
    private val mutex = NativeMutex()

    /** Where new work lands. Swapped with [running] on every turn rather than allocated again. */
    private var incoming = ArrayDeque<ScheduledTask>()
    private var running = ArrayDeque<ScheduledTask>()

    /** What asked to run at the end of the turn rather than at its start. */
    private var atEnd = ArrayDeque<ScheduledTask>()
    private var atEndRunning = ArrayDeque<ScheduledTask>()

    /** What is waiting for a number of ticks to pass. Scanned once a turn, so a list is enough. */
    private val delayed = ArrayList<ScheduledTask>()

    private val boundThread = AtomicLong(NOBODY)
    private val closed = AtomicInt(0)

    /** Whether the calling thread is the one that owns this. */
    public val isCurrent: Boolean get() = boundThread.value == NativeThread.currentId()

    /**
     * Whether anybody has claimed this yet.
     *
     * Until somebody does there is nothing to enforce and nobody to hand a task to: a world built
     * by a test, or one that exists before the first tick, is whoever is holding it. Enforcement
     * starts the moment a thread claims it, which on a running server is the first tick.
     */
    public val isClaimed: Boolean get() = boundThread.value != NOBODY

    /** Whether this owner has gone: its world was closed, or its region merged into another. */
    public val isClosed: Boolean get() = closed.value != 0

    /** How many tasks are waiting, for the timings and the tests. */
    public val pending: Int get() = mutex.withLock { incoming.size + atEnd.size + delayed.size }

    /** Claims this owner for the calling thread. The tick thread does it on its first turn. */
    public fun bindToCurrentThread() {
        boundThread.value = NativeThread.currentId()
    }

    /**
     * Runs [task] on the owning thread. Straight away when the caller already is that thread, so a
     * chain of gameplay does not queue behind itself; otherwise it waits its turn.
     */
    public fun submit(task: () -> Unit): TaskHandle {
        val handle = TaskHandle()
        if (isClosed) {
            // Nobody is going to run it, and saying so is better than holding it for ever.
            handle.cancel()
            return handle
        }
        if (isCurrent || !isClaimed) {
            task()
            return handle
        }
        mutex.withLock { incoming.addLast(ScheduledTask(handle, task, dueInTicks = 0)) }
        return handle
    }

    /**
     * Runs [task] once [delayTicks] turns have passed, counting this one. Zero is the next turn.
     *
     * parity of intent with `ServerLevel.scheduleTick`: a delay is in ticks because everything else
     * here is, and a task that asks for a delay while running is due that many turns from now.
     */
    public fun schedule(
        delayTicks: Int,
        task: () -> Unit,
    ): TaskHandle {
        val handle = TaskHandle()
        if (isClosed) {
            handle.cancel()
            return handle
        }
        mutex.withLock { delayed.add(ScheduledTask(handle, task, dueInTicks = maxOf(delayTicks, 0))) }
        return handle
    }

    /**
     * Runs [task] at the end of this turn rather than at the start of the next.
     *
     * For what has to see everything the turn did: sending what changed, or writing out a snapshot
     * that would otherwise be a tick stale.
     */
    public fun scheduleAtEnd(task: () -> Unit): TaskHandle {
        val handle = TaskHandle()
        if (isClosed) {
            handle.cancel()
            return handle
        }
        mutex.withLock { atEnd.addLast(ScheduledTask(handle, task, dueInTicks = 0)) }
        return handle
    }

    /**
     * Runs what is waiting, on the owning thread. Answers how many ran.
     *
     * Takes the whole queue in one go and runs it outside the lock: a task that submits another
     * (a block change that schedules a tick) must not deadlock, and what it adds belongs to the
     * next turn rather than to this one, which is what keeps a tick bounded. The two queues are
     * swapped rather than allocated, so a turn costs no garbage of its own.
     */
    public fun runPending(onFailure: (Throwable) -> Unit = { throw it }): Int = drain(::takeIncoming, onFailure)

    /** The other half of a turn: what [scheduleAtEnd] left, run after the turn's own work. */
    public fun runEndOfTick(onFailure: (Throwable) -> Unit = { throw it }): Int = drain(::takeAtEnd, onFailure)

    /**
     * Moves every delayed task one turn closer and queues whatever came due.
     *
     * Called once a turn by whoever ticks this owner, before [runPending], so a task scheduled for
     * zero ticks runs on the very next turn.
     */
    public fun advanceTick() {
        mutex.withLock {
            if (delayed.isEmpty()) return
            var index = 0
            while (index < delayed.size) {
                val task = delayed[index]
                when {
                    task.handle.isCancelled -> delayed.removeAt(index)
                    task.dueInTicks <= 0 -> {
                        incoming.addLast(task)
                        delayed.removeAt(index)
                    }
                    else -> {
                        task.dueInTicks--
                        index++
                    }
                }
            }
        }
    }

    /**
     * Nothing else runs here: the world was closed, or this region merged into another.
     *
     * What was waiting is cancelled rather than run, and the handles say so. A caller that has to
     * know its work survived a merge asks its handle instead of assuming.
     */
    public fun close() {
        closed.value = 1
        mutex.withLock {
            for (task in incoming) task.handle.cancel()
            for (task in atEnd) task.handle.cancel()
            for (task in delayed) task.handle.cancel()
            incoming.clear()
            atEnd.clear()
            delayed.clear()
        }
    }

    /** Throws when the caller is not the owner and strict mode is on. See [Ownership]. */
    public fun check(what: String) {
        if (!Ownership.strict || !isClaimed || isCurrent) return
        error("$what belongs to '$name' and was touched from another thread")
    }

    private fun takeIncoming(): ArrayDeque<ScheduledTask> {
        val taken = incoming
        incoming = running
        running = taken
        return taken
    }

    private fun takeAtEnd(): ArrayDeque<ScheduledTask> {
        val taken = atEnd
        atEnd = atEndRunning
        atEndRunning = taken
        return taken
    }

    private inline fun drain(
        take: () -> ArrayDeque<ScheduledTask>,
        onFailure: (Throwable) -> Unit,
    ): Int {
        val taken = mutex.withLock { take() }
        if (taken.isEmpty()) return 0
        var ran = 0
        for (task in taken) {
            if (task.handle.isCancelled) continue
            // One task that throws costs itself and nothing else: the rest of this turn belongs to
            // whoever else asked, and dropping them would lose a player's packets for a tick.
            try {
                task.task()
                ran++
            } catch (e: Throwable) {
                onFailure(e)
            }
        }
        taken.clear()
        return ran
    }

    private companion object {
        const val NOBODY = -1L
    }
}

/**
 * Whether ownership is enforced or only documented.
 *
 * Off in a release build, on in the tests and behind a flag for whoever is developing: with one
 * region an illegal access usually works by luck, and luck is what stops working the day regions
 * are turned on. Checking it with a single region is how that shows up now rather than then.
 */
public object Ownership {
    private val enforced = AtomicLong(0)

    public var strict: Boolean
        get() = enforced.value != 0L
        set(value) {
            enforced.value = if (value) 1L else 0L
        }
}
