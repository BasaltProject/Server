package org.basaltmc.server.runtime.world

import org.basaltmc.wire.protocol.play.InitializeBorderPacket

/**
 * One world's border. parity: `WorldBorder`, minus the damage it deals outside itself, which needs
 * the per-tick player damage pass; the numbers are kept so `/worldborder damage` still reports and
 * persists what an operator set.
 */
public class WorldBorder {
    public var centerX: Double = 0.0
        private set

    public var centerZ: Double = 0.0
        private set

    /** Where the size is now, and where it is heading if a move is in progress. */
    public var size: Double = DEFAULT_SIZE
        private set

    public var targetSize: Double = DEFAULT_SIZE
        private set

    public var lerpTimeMillis: Long = 0L
        private set

    public var warningBlocks: Int = 5
    public var warningSeconds: Int = 15
    public var damageBuffer: Double = 5.0
    public var damagePerBlock: Double = 0.2

    public fun center(
        x: Double,
        z: Double,
    ) {
        centerX = x
        centerZ = z
    }

    public fun setSize(newSize: Double) {
        size = newSize
        targetSize = newSize
        lerpTimeMillis = 0L
    }

    /** parity: `WorldBorder.lerpSizeBetween`, which reports the old size while it moves. */
    public fun moveSize(
        newSize: Double,
        overMillis: Long,
    ) {
        targetSize = newSize
        lerpTimeMillis = overMillis
    }

    public fun initializePacket(): InitializeBorderPacket =
        InitializeBorderPacket(
            centerX = centerX,
            centerZ = centerZ,
            oldSize = size,
            newSize = targetSize,
            lerpTimeMillis = lerpTimeMillis,
            absoluteMaxSize = InitializeBorderPacket.ABSOLUTE_MAX_SIZE,
            warningBlocks = warningBlocks,
            warningSeconds = warningSeconds,
        )

    /**
     * How far outside the border a position is, in blocks, or 0 while it is inside.
     *
     * parity: `WorldBorder.getDistanceToBorder`, negated: vanilla measures towards the border and
     * damages on the distance past it plus the buffer.
     */
    public fun distanceOutside(
        x: Double,
        z: Double,
    ): Double {
        val half = targetSize / 2.0
        val dx = maxOf(kotlin.math.abs(x - centerX) - half, 0.0)
        val dz = maxOf(kotlin.math.abs(z - centerZ) - half, 0.0)
        return maxOf(dx, dz)
    }

    /**
     * The damage one tick outside the border costs, or 0 inside it or inside the buffer.
     *
     * parity: `ServerPlayer.tick`'s border pass, which damages every ten ticks by
     * `damagePerBlock * (distance - buffer)`, with a floor of the per-block amount.
     */
    public fun damageAt(
        x: Double,
        z: Double,
    ): Float {
        val outside = distanceOutside(x, z) - damageBuffer
        if (outside <= 0.0) return 0f
        return maxOf(1.0, outside * damagePerBlock).toFloat()
    }

    /** Called when a move finishes, so the stored size stops being the one it started from. */
    public fun settle() {
        size = targetSize
        lerpTimeMillis = 0L
    }

    public companion object {
        /** parity: `WorldBorder.DEFAULT_SIZE`. */
        public const val DEFAULT_SIZE: Double = 5.9999968E7

        /** parity: the caps `/worldborder set` refuses past. */
        public const val MAX_SIZE: Double = 5.999997E7
        public const val MAX_CENTER_COORDINATE: Double = 2.9999984E7
    }
}
