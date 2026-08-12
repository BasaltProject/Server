package org.basaltmc.server.world.densityfunction

import org.basaltmc.wire.common.Mth

/**
 * // parity: net.minecraft.world.level.levelgen.DensityFunctions.FindTopSurface. Scans [density]
 * downward from `floor(upperBound / cellHeight) * cellHeight`, in [cellHeight] steps, and returns
 * the first Y where it goes positive; [lowerBound] if it never does (or the scan starts at or
 * below [lowerBound] to begin with).
 */
public class FindTopSurface(
    public val density: DensityFunction,
    public val upperBound: DensityFunction,
    public val lowerBound: Int,
    public val cellHeight: Int,
) : DensityFunction {
    override fun compute(context: FunctionContext): Double {
        val topY = Mth.floor(upperBound.compute(context) / cellHeight) * cellHeight
        if (topY <= lowerBound) return lowerBound.toDouble()

        var blockY = topY
        while (blockY >= lowerBound) {
            if (density.compute(FunctionContext(context.blockX, blockY, context.blockZ, context.cache)) >
                0.0
            ) {
                return blockY.toDouble()
            }
            blockY -= cellHeight
        }
        return lowerBound.toDouble()
    }

    override fun minValue(): Double = lowerBound.toDouble()

    override fun maxValue(): Double = maxOf(lowerBound.toDouble(), upperBound.maxValue())
}
