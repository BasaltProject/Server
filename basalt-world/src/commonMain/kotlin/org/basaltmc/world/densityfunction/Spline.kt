package org.basaltmc.world.densityfunction

import org.basaltmc.world.spline.CubicSpline

/** // parity: net.minecraft.world.level.levelgen.DensityFunctions.Spline. Samples a [CubicSpline] whose x-axis is another density function. */
public class Spline(
    public val spline: CubicSpline<FunctionContext, SplineCoordinate>,
) : DensityFunction {
    override fun compute(context: FunctionContext): Double = spline.sample(context).toDouble()

    override fun minValue(): Double = spline.minValue().toDouble()

    override fun maxValue(): Double = spline.maxValue().toDouble()
}
