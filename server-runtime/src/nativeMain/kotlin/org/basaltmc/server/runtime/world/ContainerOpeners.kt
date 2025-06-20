package org.basaltmc.server.runtime.world

import kotlin.concurrent.AtomicReference

/**
 * How many people have a given container open.
 *
 * parity: `ContainerOpenersCounter`, which is what makes a chest lid rise, a barrel show itself open
 * and a trapped chest give off redstone. Vanilla keeps the count on the block entity and never
 * writes it to disk: a container reopened from a saved world is closed, because nobody is looking
 * at it yet.
 *
 * Opening one is rare and asking is not, so the whole thing is an immutable map swapped by compare
 * and set: a reader never waits, and the redstone path pays one field read when nothing is open.
 */
public object ContainerOpeners {
    private val state = AtomicReference<Map<String, Map<Long, Int>>>(emptyMap())

    /** One more looking. Answers the new count, which is what the block event carries. */
    public fun open(
        world: String,
        x: Int,
        y: Int,
        z: Int,
    ): Int = change(world, x, y, z, 1)

    /** One fewer. Answers the new count; at zero the entry goes rather than sitting at zero. */
    public fun close(
        world: String,
        x: Int,
        y: Int,
        z: Int,
    ): Int = change(world, x, y, z, -1)

    public fun count(
        world: String,
        x: Int,
        y: Int,
        z: Int,
    ): Int {
        val current = state.value
        if (current.isEmpty()) return 0
        return current[world]?.get(packed(x, y, z)) ?: 0
    }

    /** Everyone left, so nothing is open anywhere. For a world unloading or the server stopping. */
    public fun clear() {
        state.value = emptyMap()
    }

    private fun change(
        world: String,
        x: Int,
        y: Int,
        z: Int,
        by: Int,
    ): Int {
        val key = packed(x, y, z)
        while (true) {
            val current = state.value
            val inWorld = current[world].orEmpty()
            val next = ((inWorld[key] ?: 0) + by).coerceAtLeast(0)
            val updated = if (next == 0) inWorld - key else inWorld + (key to next)
            val whole = if (updated.isEmpty()) current - world else current + (world to updated)
            if (state.compareAndSet(current, whole)) return next
        }
    }

    /** The same packing a block position uses on the wire, which is unique within a world. */
    private fun packed(
        x: Int,
        y: Int,
        z: Int,
    ): Long = (x.toLong() and 0x3ffffff shl 38) or (z.toLong() and 0x3ffffff shl 12) or (y.toLong() and 0xfff)
}
