package org.basaltmc.server.world.densityfunction

import org.basaltmc.wire.common.Mth

/** // parity: net.minecraft.world.level.levelgen.DensityFunctions.Clamp. Clamps [input] into `[minValue, maxValue]`. */
public class Clamp(
    public val input: DensityFunction,
    private val clampMinValue: Double,
    private val clampMaxValue: Double,
) : DensityFunction {
    override fun compute(context: FunctionContext): Double =
        Mth.clamp(input.compute(context), clampMinValue, clampMaxValue)

    override fun minValue(): Double = clampMinValue

    override fun maxValue(): Double = clampMaxValue
}
