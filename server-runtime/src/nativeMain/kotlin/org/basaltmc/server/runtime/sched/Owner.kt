package org.basaltmc.server.runtime.sched

import org.basaltmc.wire.io.NativeMutex
import org.basaltmc.wire.io.NativeThread
import kotlin.concurrent.AtomicLong

/**
 * One thread and the state it owns.
 *
 * parity of model with `RegionizedServer`: everything mutable belongs to exactly one owner, and
 * whoever is not that owner asks by [submit] instead of touching it. With one region there is one
 * world owner and one global owner; with several there is one per region. The rule is the same
 * either way, which is the point: code written against it does not change when regions are turned
 * on.
 */
public class Owner(
    /** What it owns, for the message a strict-mode failure prints. */
    public val name: String,
) {
    private val mutex = NativeMutex()
    private var queue = ArrayDeque<() -> Unit>()
    private val boundThread = AtomicLong(NOBODY)

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

    /** How many tasks are waiting, for the timings and the tests. */
    public val pending: Int get() = mutex.withLock { queue.size }

    /** Claims this owner for the calling thread. The tick thread does it on its first turn. */
    public fun bindToCurrentThread() {
        boundThread.value = NativeThread.currentId()
    }

    /**
     * Runs [task] on the owning thread. Straight away when the caller already is that thread, so a
     * chain of gameplay does not queue behind itself; otherwise it waits its turn.
     */
    public fun submit(task: () -> Unit) {
        if (isCurrent || !isClaimed) {
            task()
            return
        }
        mutex.withLock { queue.addLast(task) }
    }

    /**
     * Runs what is waiting, on the owning thread. Answers how many ran.
     *
     * Takes the whole queue in one go and runs it outside the lock: a task that submits another
     * (a block change that schedules a tick) must not deadlock, and what it adds belongs to the
     * next turn rather than to this one, which is what keeps a tick bounded.
     */
    public fun runPending(onFailure: (Throwable) -> Unit = { throw it }): Int {
        val taken = mutex.withLock { if (queue.isEmpty()) return 0 else queue.also { queue = ArrayDeque() } }
        for (task in taken) {
            // One task that throws costs itself and nothing else: the rest of this turn belongs to
            // whoever else asked, and dropping them would lose a player's packets for a tick.
            try {
                task()
            } catch (e: Throwable) {
                onFailure(e)
            }
        }
        return taken.size
    }

    /** Throws when the caller is not the owner and strict mode is on. See [Ownership]. */
    public fun check(what: String) {
        if (!Ownership.strict || !isClaimed || isCurrent) return
        error("$what belongs to '$name' and was touched from another thread")
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
