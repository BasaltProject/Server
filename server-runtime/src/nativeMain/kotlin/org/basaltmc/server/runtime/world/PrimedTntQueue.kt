package org.basaltmc.server.runtime.world

import org.basaltmc.wire.io.NativeMutex

/**
 * The TNT blocks that were lit this tick and still have to become entities.
 *
 * The block itself goes the moment the signal reaches it, but only the caller knows how to
 * allocate an entity id, so what it left behind waits here for one tick at most.
 *
 * Shares [WorldState]'s single [mutex] rather than owning its own: splitting lock granularity
 * would change race outcomes between the tick thread and the save/eviction threads, which is a
 * real behavior change even though this list has nothing to do with vanilla parity.
 */
internal class PrimedTntQueue(
    private val mutex: NativeMutex,
) {
    private val primedTnt = mutableListOf<IntArray>()

    /** parity: `TntBlock.explode`, which spawns the entity where the block stood. */
    public fun primeTnt(
        x: Int,
        y: Int,
        z: Int,
    ) {
        mutex.withLock { primedTnt += intArrayOf(x, y, z) }
    }

    /** Takes what was lit, leaving the queue empty. */
    public fun takePrimedTnt(): List<IntArray> =
        mutex.withLock {
            if (primedTnt.isEmpty()) {
                emptyList()
            } else {
                val lit = primedTnt.toList()
                primedTnt.clear()
                lit
            }
        }
}
