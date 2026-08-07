package org.basaltmc.world.densityfunction

import org.basaltmc.world.noise.NormalNoise

/**
 * // parity: net.minecraft.world.level.levelgen.DensityFunctions.ShiftNoise. Shared shape of
 * [Shift]/[ShiftA]/[ShiftB]: a quarter-scaled, quadrupled sample of [offsetNoise], used to warp
 * coordinates before they feed another density function.
 */
public interface ShiftNoise : DensityFunction {
    public val offsetNoise: NormalNoise

    override fun minValue(): Double = -maxValue()

    override fun maxValue(): Double = offsetNoise.maxValue * 4.0

    public fun compute(
        localX: Double,
        localY: Double,
        localZ: Double,
    ): Double = offsetNoise.getValue(localX * 0.25, localY * 0.25, localZ * 0.25) * 4.0
}

/** // parity: net.minecraft.world.level.levelgen.DensityFunctions.Shift. */
public class Shift(
    override val offsetNoise: NormalNoise,
) : ShiftNoise {
    override fun compute(context: FunctionContext): Double =
        compute(context.blockX.toDouble(), context.blockY.toDouble(), context.blockZ.toDouble())
}

/** // parity: net.minecraft.world.level.levelgen.DensityFunctions.ShiftA. */
public class ShiftA(
    override val offsetNoise: NormalNoise,
) : ShiftNoise {
    override fun compute(context: FunctionContext): Double =
        compute(context.blockX.toDouble(), 0.0, context.blockZ.toDouble())
}

/** // parity: net.minecraft.world.level.levelgen.DensityFunctions.ShiftB. */
public class ShiftB(
    override val offsetNoise: NormalNoise,
) : ShiftNoise {
    override fun compute(context: FunctionContext): Double =
        compute(context.blockZ.toDouble(), context.blockX.toDouble(), 0.0)
}
