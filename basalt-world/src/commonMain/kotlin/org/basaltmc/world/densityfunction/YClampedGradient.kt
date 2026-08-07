package org.basaltmc.world.densityfunction

import org.basaltmc.common.Mth

/**
 * // parity: net.minecraft.world.level.levelgen.DensityFunctions.YClampedGradient. A linear
 * ramp of [fromValue] to [toValue] over the block-Y span `[fromY, toY]`, clamped outside it.
 */
public class YClampedGradient(
    public val fromY: Int,
    public val toY: Int,
    public val fromValue: Double,
    public val toValue: Double,
) : DensityFunction {
    override fun compute(context: FunctionContext): Double =
        Mth.clampedMap(context.blockY.toDouble(), fromY.toDouble(), toY.toDouble(), fromValue, toValue)

    override fun minValue(): Double = minOf(fromValue, toValue)

    override fun maxValue(): Double = maxOf(fromValue, toValue)
}
