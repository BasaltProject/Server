package org.basaltmc.common

// parity: net.minecraft.util.Mth, the subset noise generation needs.
public object Mth {
    // kotlin.math.floor is called qualified, not imported: an unqualified call inside a member
    // named `floor` resolves to itself first (member takes priority over a same-named top-level
    // import), which would make this infinitely recurse instead of calling the stdlib function.
    public fun floor(v: Double): Int = kotlin.math.floor(v).toInt()

    public fun lfloor(v: Double): Long = kotlin.math.floor(v).toLong()

    public fun lerp(
        alpha: Double,
        p0: Double,
        p1: Double,
    ): Double = p0 + alpha * (p1 - p0)

    public fun lerp2(
        alpha1: Double,
        alpha2: Double,
        x00: Double,
        x10: Double,
        x01: Double,
        x11: Double,
    ): Double = lerp(alpha2, lerp(alpha1, x00, x10), lerp(alpha1, x01, x11))

    public fun lerp3(
        alpha1: Double,
        alpha2: Double,
        alpha3: Double,
        x000: Double,
        x100: Double,
        x010: Double,
        x110: Double,
        x001: Double,
        x101: Double,
        x011: Double,
        x111: Double,
    ): Double =
        lerp(
            alpha3,
            lerp2(alpha1, alpha2, x000, x100, x010, x110),
            lerp2(alpha1, alpha2, x001, x101, x011, x111),
        )

    public fun smoothstep(x: Double): Double = x * x * x * (x * (x * 6.0 - 15.0) + 10.0)

    public fun smoothstepDerivative(x: Double): Double = 30.0 * x * x * (x - 1.0) * (x - 1.0)

    public fun square(v: Long): Long = v * v

    public fun clamp(
        value: Double,
        min: Double,
        max: Double,
    ): Double = if (value < min) min else minOf(value, max)

    public fun clamp(
        value: Float,
        min: Float,
        max: Float,
    ): Float = if (value < min) min else minOf(value, max)

    public fun inverseLerp(
        value: Double,
        min: Double,
        max: Double,
    ): Double = (value - min) / (max - min)

    public fun clampedLerp(
        factor: Double,
        min: Double,
        max: Double,
    ): Double =
        when {
            factor < 0.0 -> min
            factor > 1.0 -> max
            else -> lerp(factor, min, max)
        }

    public fun clampedMap(
        value: Double,
        fromMin: Double,
        fromMax: Double,
        toMin: Double,
        toMax: Double,
    ): Double = clampedLerp(inverseLerp(value, fromMin, fromMax), toMin, toMax)

    public fun lerp(
        alpha: Float,
        p0: Float,
        p1: Float,
    ): Float = p0 + alpha * (p1 - p0)

    /** The first index in `[from, to)` for which [condition] holds, assuming it holds for a contiguous suffix of the range; `to` if it never does. */
    public fun binarySearch(
        from: Int,
        to: Int,
        condition: (Int) -> Boolean,
    ): Int {
        var lo = from
        var len = to - from
        while (len > 0) {
            val half = len / 2
            val middle = lo + half
            if (condition(middle)) {
                len = half
            } else {
                lo = middle + 1
                len -= half + 1
            }
        }
        return lo
    }
}
