package org.basaltmc.server.world.spline

import org.basaltmc.wire.common.Mth

/**
 * // parity: net.minecraft.util.CubicSpline. Vanilla parametrizes this over a single type
 * `I extends BoundedFloatFunction<?>`, using a wildcard for the coordinate type and a separate
 * `<C, I extends BoundedFloatFunction<C>>` binding at every call site that needs it (`sample`,
 * `asSampler`). This project bakes the coordinate type `C` directly into [CubicSpline] itself:
 * behaviorally identical, and Kotlin has no wildcard-capture idiom to justify carrying the split.
 */
public sealed class CubicSpline<C, I : BoundedFloatFunction<C>> {
    public abstract fun sample(coordinate: C): Float

    public abstract fun minValue(): Float

    public abstract fun maxValue(): Float
}

/** // parity: net.minecraft.util.CubicSpline.Constant. A spline that ignores its coordinate and always samples to [value]. */
public class Constant<C, I : BoundedFloatFunction<C>>(
    public val value: Float,
) : CubicSpline<C, I>() {
    override fun sample(coordinate: C): Float = value

    override fun minValue(): Float = value

    override fun maxValue(): Float = value
}

/**
 * // parity: net.minecraft.util.CubicSpline.Multipoint. A monotone (Hermite) cubic spline through
 * [locations], each paired with a child [CubicSpline] (usually [Constant], but splines can nest)
 * and a tangent in [derivatives]. Outside the first/last location, extrapolates linearly using
 * that endpoint's own derivative. Bounds are derived once at construction, [create] instead of
 * the constructor is the real entry point (mirroring vanilla's own compact-canonical-constructor
 * plus explicit-bounds-constructor split).
 */
public class Multipoint<C, I : BoundedFloatFunction<C>> internal constructor(
    public val coordinate: I,
    public val locations: FloatArray,
    public val values: List<CubicSpline<C, I>>,
    public val derivatives: FloatArray,
    private val cachedMinValue: Float,
    private val cachedMaxValue: Float,
) : CubicSpline<C, I>() {
    override fun sample(coordinate: C): Float {
        val input = this.coordinate.apply(coordinate)
        val lastIndex = locations.size - 1
        val start = findIntervalStart(locations, input)
        if (start < 0) return linearExtend(input, locations, values.first().sample(coordinate), derivatives, 0)
        if (start ==
            lastIndex
        ) {
            return linearExtend(input, locations, values[lastIndex].sample(coordinate), derivatives, lastIndex)
        }

        val x1 = locations[start]
        val x2 = locations[start + 1]
        val t = (input - x1) / (x2 - x1)
        val d1 = derivatives[start]
        val d2 = derivatives[start + 1]
        val y1 = values[start].sample(coordinate)
        val y2 = values[start + 1].sample(coordinate)
        val a = d1 * (x2 - x1) - (y2 - y1)
        val b = -d2 * (x2 - x1) + (y2 - y1)
        return Mth.lerp(t, y1, y2) + t * (1.0f - t) * Mth.lerp(t, a, b)
    }

    override fun minValue(): Float = cachedMinValue

    override fun maxValue(): Float = cachedMaxValue

    public companion object {
        public fun <C, I : BoundedFloatFunction<C>> create(
            coordinate: I,
            locations: FloatArray,
            values: List<CubicSpline<C, I>>,
            derivatives: FloatArray,
        ): Multipoint<C, I> {
            require(locations.size == values.size && locations.size == derivatives.size) {
                "All lengths must be equal, got: ${locations.size} ${values.size} ${derivatives.size}"
            }
            require(locations.isNotEmpty()) { "Cannot create a multipoint spline with no points" }

            val lastIndex = locations.size - 1
            var minValue = Float.POSITIVE_INFINITY
            var maxValue = Float.NEGATIVE_INFINITY
            val minInput = coordinate.minValue()
            val maxInput = coordinate.maxValue()

            if (minInput < locations[0]) {
                val edge1 = linearExtend(minInput, locations, values[0].minValue(), derivatives, 0)
                val edge2 = linearExtend(minInput, locations, values[0].maxValue(), derivatives, 0)
                minValue = minOf(minValue, minOf(edge1, edge2))
                maxValue = maxOf(maxValue, maxOf(edge1, edge2))
            }
            if (maxInput > locations[lastIndex]) {
                val edge1 = linearExtend(maxInput, locations, values[lastIndex].minValue(), derivatives, lastIndex)
                val edge2 = linearExtend(maxInput, locations, values[lastIndex].maxValue(), derivatives, lastIndex)
                minValue = minOf(minValue, minOf(edge1, edge2))
                maxValue = maxOf(maxValue, maxOf(edge1, edge2))
            }
            for (value in values) {
                minValue = minOf(minValue, value.minValue())
                maxValue = maxOf(maxValue, value.maxValue())
            }
            for (i in 0 until lastIndex) {
                val x1 = locations[i]
                val x2 = locations[i + 1]
                val xDiff = x2 - x1
                val min1 = values[i].minValue()
                val max1 = values[i].maxValue()
                val min2 = values[i + 1].minValue()
                val max2 = values[i + 1].maxValue()
                val d1 = derivatives[i]
                val d2 = derivatives[i + 1]
                if (d1 != 0.0f || d2 != 0.0f) {
                    val p1 = d1 * xDiff
                    val p2 = d2 * xDiff
                    val minLerp1 = minOf(min1, min2)
                    val maxLerp1 = maxOf(max1, max2)
                    val minA = p1 - max2 + min1
                    val maxA = p1 - min2 + max1
                    val minB = -p2 + min2 - max1
                    val maxB = -p2 + max2 - min1
                    val minLerp2 = minOf(minA, minB)
                    val maxLerp2 = maxOf(maxA, maxB)
                    minValue = minOf(minValue, minLerp1 + 0.25f * minLerp2)
                    maxValue = maxOf(maxValue, maxLerp1 + 0.25f * maxLerp2)
                }
            }

            return Multipoint(coordinate, locations, values, derivatives, minValue, maxValue)
        }

        private fun linearExtend(
            input: Float,
            locations: FloatArray,
            value: Float,
            derivatives: FloatArray,
            index: Int,
        ): Float {
            val derivative = derivatives[index]
            return if (derivative == 0.0f) value else value + derivative * (input - locations[index])
        }

        private fun findIntervalStart(
            locations: FloatArray,
            input: Float,
        ): Int = Mth.binarySearch(0, locations.size) { input < locations[it] } - 1
    }
}
