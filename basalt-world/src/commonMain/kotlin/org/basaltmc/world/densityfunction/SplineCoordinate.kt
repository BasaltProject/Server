package org.basaltmc.world.densityfunction

import org.basaltmc.world.spline.BoundedFloatFunction

/**
 * // parity: net.minecraft.world.level.levelgen.DensityFunctions.Spline.Coordinate. Wraps a
 * [DensityFunction] as a spline's x-axis input. Vanilla routes this through an extra
 * `Spline.Point(FunctionContext)` record; that record carries no behavior of its own; folded away
 * here in favor of sampling directly on [FunctionContext].
 */
public class SplineCoordinate(
    public val function: DensityFunction,
) : BoundedFloatFunction<FunctionContext> {
    override fun apply(c: FunctionContext): Float = function.compute(c).toFloat()

    override fun minValue(): Float = function.minValue().toFloat()

    override fun maxValue(): Float = function.maxValue().toFloat()
}
