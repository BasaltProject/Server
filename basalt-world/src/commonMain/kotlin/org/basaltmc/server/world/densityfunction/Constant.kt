package org.basaltmc.server.world.densityfunction

/** // parity: net.minecraft.world.level.levelgen.DensityFunctions.Constant. A fixed value, ignoring position. */
public class Constant(
    public val value: Double,
) : DensityFunction {
    override fun compute(context: FunctionContext): Double = value

    override fun minValue(): Double = value

    override fun maxValue(): Double = value
}
