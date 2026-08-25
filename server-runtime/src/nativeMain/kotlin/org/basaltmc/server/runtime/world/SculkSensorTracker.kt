package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.wire.io.NativeMutex
import kotlin.math.abs

/**
 * Where the sculk sensors are, so a walking player does not have to search for them.
 *
 * A vibration from a block event can afford to look around itself; a footstep happens every tick
 * for every player, and cannot. Kept as blocks are set, which covers everything placed or broken
 * while the server runs: a sensor that came with the terrain is not in here until something
 * touches it, so footsteps do not reach the deep dark yet.
 */
internal class SculkSensorTracker(
    private val mutex: NativeMutex,
) {
    private val sculkSensors = mutableSetOf<Long>()

    /**
     * Called from [WorldState.setBlockState] without the lock held: the owner is the only writer
     * of blocks, so this set only ever changes from that one thread, same as before it moved here.
     */
    public fun remember(
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ) {
        val path = BlockStateDefinitions.blockOf(stateId)?.path
        val position = blockKey(x, y, z)
        if (path == "sculk_sensor" || path == "calibrated_sculk_sensor") {
            sculkSensors += position
        } else {
            sculkSensors -= position
        }
    }

    /** Whether any known sensor is within [range] blocks of this point. */
    public fun hasNear(
        x: Double,
        y: Double,
        z: Double,
        range: Int,
    ): Boolean {
        if (sculkSensors.isEmpty()) return false
        return mutex.withLock {
            sculkSensors.any { position ->
                abs(unpackBlockX(position) - x) <= range &&
                    abs(unpackBlockY(position) - y) <= range &&
                    abs(unpackBlockZ(position) - z) <= range
            }
        }
    }

    private fun blockKey(
        x: Int,
        y: Int,
        z: Int,
    ): Long = (x.toLong() and 0x3ffffff shl 38) or (z.toLong() and 0x3ffffff shl 12) or (y.toLong() and 0xfff)

    private fun unpackBlockX(packed: Long): Int = (packed shl 0 shr 38).toInt()

    private fun unpackBlockY(packed: Long): Int = (packed shl 52 shr 52).toInt()

    private fun unpackBlockZ(packed: Long): Int = (packed shl 26 shr 38).toInt()
}
