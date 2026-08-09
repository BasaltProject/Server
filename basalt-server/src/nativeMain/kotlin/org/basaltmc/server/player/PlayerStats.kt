package org.basaltmc.server.player

import org.basaltmc.io.NativeMutex
import org.basaltmc.storage.StoredStats
import kotlin.math.roundToLong

/**
 * The counters of one logged-in player, incremented as the server notices things happening.
 *
 * Behind a lock because they are touched from two threads: this connection's own loop (walking,
 * hunger) and whatever thread ran a command. The lock is uncontended in practice, and a counter
 * that occasionally loses an increment to a race would be worse than useless, since the whole point
 * of a statistic is that it adds up.
 *
 * What is counted is exactly what this build can observe. Vanilla has around a hundred more, and
 * each one needs a place in the code that notices: they arrive with the systems that make them
 * observable, not before.
 */
public class PlayerStats {
    private val lock = NativeMutex()
    private var counters = mutableMapOf<String, Long>()

    /** Fractions of a block walked, kept until they add up to a whole centimetre. */
    private var walkRemainder = 0.0

    public fun increment(
        key: String,
        amount: Long = 1,
    ) {
        if (amount == 0L) return
        lock.withLock { counters[key] = (counters[key] ?: 0L) + amount }
    }

    public operator fun get(key: String): Long = lock.withLock { counters[key] ?: 0L }

    /**
     * Adds a distance in blocks, counted in centimetres as vanilla does.
     *
     * The remainder is kept rather than rounded away: a player moves a few centimetres per tick, so
     * rounding each one to the nearest centimetre would lose most of the distance at a walk and all
     * of it when standing almost still.
     */
    public fun addWalked(blocks: Double) {
        if (blocks <= 0.0) return
        lock.withLock {
            walkRemainder += blocks * 100.0
            val whole = walkRemainder.toLong()
            if (whole > 0) {
                walkRemainder -= whole
                counters[StoredStats.WALK_ONE_CM] = (counters[StoredStats.WALK_ONE_CM] ?: 0L) + whole
            }
        }
    }

    public fun addDamageTaken(amount: Float) {
        // Tenths of a heart, matching vanilla's own scale for this counter.
        increment(StoredStats.DAMAGE_TAKEN, (amount * 10f).roundToLong())
    }

    /** A snapshot for saving. [playTimeTicks] is kept in its own column and folded in here too. */
    public fun snapshot(playTimeTicks: Long): StoredStats =
        lock.withLock {
            StoredStats(counters.toMap() + (StoredStats.PLAY_TIME to playTimeTicks))
        }

    public fun restore(stored: StoredStats) {
        lock.withLock {
            counters = stored.counters.toMutableMap()
            // Play time has a column of its own and is authoritative there; keeping a second copy
            // here would let the two disagree.
            counters.remove(StoredStats.PLAY_TIME)
        }
    }
}
