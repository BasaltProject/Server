package org.basaltmc.world.densityfunction

import org.basaltmc.world.noise.NormalNoise

/**
 * // parity: net.minecraft.world.level.levelgen.DensityFunctions.ShiftedNoise. Samples [noise]
 * at the context position scaled by [xzScale]/[yScale] and warped by [shiftX]/[shiftY]/[shiftZ].
 */
public class ShiftedNoise(
    public val shiftX: DensityFunction,
    public val shiftY: DensityFunction,
    public val shiftZ: DensityFunction,
    public val xzScale: Double,
    public val yScale: Double,
    public val noise: NormalNoise,
) : DensityFunction {
    override fun compute(context: FunctionContext): Double {
        val x = context.blockX * xzScale + shiftX.compute(context)
        val y = context.blockY * yScale + shiftY.compute(context)
        val z = context.blockZ * xzScale + shiftZ.compute(context)
        return noise.getValue(x, y, z)
    }

    override fun minValue(): Double = -maxValue()

    override fun maxValue(): Double = noise.maxValue
}
