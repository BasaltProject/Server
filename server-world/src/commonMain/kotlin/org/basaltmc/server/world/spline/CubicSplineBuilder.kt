package org.basaltmc.server.world.spline

/** // parity: net.minecraft.util.CubicSpline.Builder. Accumulates ascending `(location, value, derivative)` points into a [Multipoint] spline. */
public class CubicSplineBuilder<C, I : BoundedFloatFunction<C>>(
    private val coordinate: I,
) {
    private val locations = mutableListOf<Float>()
    private val values = mutableListOf<CubicSpline<C, I>>()
    private val derivatives = mutableListOf<Float>()

    public fun addPoint(
        location: Float,
        value: Float,
        derivative: Float = 0.0f,
    ): CubicSplineBuilder<C, I> = addPoint(location, Constant(value), derivative)

    public fun addPoint(
        location: Float,
        sampler: CubicSpline<C, I>,
        derivative: Float = 0.0f,
    ): CubicSplineBuilder<C, I> {
        require(locations.isEmpty() || location > locations.last()) { "Please register points in ascending order" }
        locations += location
        values += sampler
        derivatives += derivative
        return this
    }

    public fun build(): CubicSpline<C, I> {
        check(locations.isNotEmpty()) { "No elements added" }
        return Multipoint.create(coordinate, locations.toFloatArray(), values.toList(), derivatives.toFloatArray())
    }
}
